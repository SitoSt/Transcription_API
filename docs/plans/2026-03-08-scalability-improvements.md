# Scalability Improvements Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Fix two scalability issues: flushLoop threads blocking indefinitely under GPU saturation, and clients receiving no feedback when the audio buffer is full.

**Architecture:** Three independent changes — (1) add `try_acquire()` to `InferenceLimiter`, (2) move `InferenceLimiter::Guard` out of `transcribeSlidingWindow` into callers so flushLoop can use the non-blocking variant, (3) change `processAudioChunk` to return `bool` and send a JSON warning to the client on first overflow.

**Tech Stack:** C++17, Google Test (existing), Boost.Beast WebSocket, nlohmann/json

---

### Task 1: Add `try_acquire()` to `InferenceLimiter`

**Files:**
- Modify: `src/whisper/InferenceLimiter.h`
- Modify: `tests/unit/test_inference_limiter.cpp`

**Step 1: Write the three failing tests**

In `tests/unit/test_inference_limiter.cpp`, append after the last test (line 71):

```cpp
TEST_F(InferenceLimiterTest, TryAcquireSucceedsWhenSlotFree) {
    bool acquired = InferenceLimiter::instance().try_acquire();
    EXPECT_TRUE(acquired);
    if (acquired) InferenceLimiter::instance().release();
}

TEST_F(InferenceLimiterTest, TryAcquireFailsWhenAtLimit) {
    InferenceLimiter::instance().setMaxConcurrency(1);
    InferenceLimiter::Guard g; // occupy the single slot
    EXPECT_FALSE(InferenceLimiter::instance().try_acquire());
}

TEST_F(InferenceLimiterTest, TryAcquireSucceedsAfterRelease) {
    InferenceLimiter::instance().setMaxConcurrency(1);
    {
        InferenceLimiter::Guard g; // occupy
        EXPECT_FALSE(InferenceLimiter::instance().try_acquire());
    } // g released
    bool acquired = InferenceLimiter::instance().try_acquire();
    EXPECT_TRUE(acquired);
    if (acquired) InferenceLimiter::instance().release();
}
```

**Step 2: Run tests to verify they fail**

```bash
cmake --build build --target unit_tests -j$(nproc) 2>&1 | tail -5
./build/tests/unit_tests --gtest_filter="InferenceLimiterTest.TryAcquire*" 2>&1 | tail -5
```

Expected: compile error — `try_acquire` does not exist yet.

**Step 3: Implement `try_acquire()` in `InferenceLimiter.h`**

In `src/whisper/InferenceLimiter.h`, add after the `acquire()` method (after line 38):

```cpp
    /**
     * @brief Try to acquire an inference slot without blocking.
     * @return true if a slot was acquired, false if all slots are taken.
     */
    bool try_acquire() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (active_count_ < max_concurrent_) {
            ++active_count_;
            return true;
        }
        return false;
    }
```

**Step 4: Run tests to verify they pass**

```bash
cmake --build build --target unit_tests -j$(nproc) 2>&1 | tail -5
./build/tests/unit_tests --gtest_filter="InferenceLimiterTest.*" 2>&1 | tail -10
```

Expected: all 8 InferenceLimiterTest tests pass (5 existing + 3 new).

**Step 5: Commit**

```bash
git add src/whisper/InferenceLimiter.h tests/unit/test_inference_limiter.cpp
git commit -m "feat: add try_acquire() to InferenceLimiter for non-blocking inference"
```

---

### Task 2: Non-blocking inference in flushLoop

**Files:**
- Modify: `src/whisper/StreamingWhisperEngine.cpp` (~line 127)
- Modify: `src/server/StreamingSession.h` (~line 16 for include, ~line 498 for flushLoop)

**Context:** Currently `InferenceLimiter::Guard` is inside `transcribeSlidingWindow()`, which means flushLoop blocks the whole thread when the GPU is busy. The fix moves the Guard into the callers so flushLoop can use the non-blocking `try_acquire()`.

**Step 1: Move `InferenceLimiter::Guard` out of `transcribeSlidingWindow()`**

In `src/whisper/StreamingWhisperEngine.cpp`, find this block (~line 126–134):

```cpp
    int result = -1;
    {
        InferenceLimiter::Guard limit_guard;
        result = whisper_full_with_state(
            ctx_, state_, params,
            audio_buffer_.data(),
            audio_buffer_.size()
        );
    }
```

Replace with (remove the Guard wrapper, keep only the call):

```cpp
    int result = whisper_full_with_state(
        ctx_, state_, params,
        audio_buffer_.data(),
        audio_buffer_.size()
    );
```

**Step 2: Add blocking Guard to `transcribe()` (legacy path)**

In `src/whisper/StreamingWhisperEngine.cpp`, find the `transcribe()` function (~line 222):

```cpp
std::string StreamingWhisperEngine::transcribe(size_t start_offset) {
    // Legacy mapping (ignores start_offset which is no longer used outside of testing)
    return transcribeSlidingWindow(true).committed_text;
}
```

Replace with:

```cpp
std::string StreamingWhisperEngine::transcribe(size_t start_offset) {
    // Legacy mapping — uses blocking Guard for direct callers (e.g. tests).
    InferenceLimiter::Guard limit_guard;
    return transcribeSlidingWindow(true).committed_text;
}
```

**Step 3: Add `InferenceLimiter` include to `StreamingSession.h`**

In `src/server/StreamingSession.h`, after line 24 (`#include "utils/HallucinationGuard.h"`), add:

```cpp
#include "whisper/InferenceLimiter.h"
```

**Step 4: Use `try_acquire()` in `flushLoop`**

In `src/server/StreamingSession.h`, find this line in `flushLoop()` (~line 498):

```cpp
            auto res = engine_->transcribeSlidingWindow(false);
```

Replace with:

```cpp
            // Non-blocking inference: skip cycle if GPU is saturated.
            if (!InferenceLimiter::instance().try_acquire()) {
                Log::debug("flushLoop: GPU busy, skipping inference cycle", session_id_);
                continue;
            }
            auto res = engine_->transcribeSlidingWindow(false);
            InferenceLimiter::instance().release();
```

**Step 5: Build and run full test suite**

```bash
cmake --build build --target unit_tests -j$(nproc) 2>&1 | tail -5
./build/tests/unit_tests 2>&1 | tail -10
```

Expected: all 60 tests still pass. No new failures.

**Step 6: Build server binary**

```bash
cmake --build build --target transcription_server -j$(nproc) 2>&1 | tail -5
```

Expected: compiles without errors.

**Step 7: Commit**

```bash
git add src/whisper/StreamingWhisperEngine.cpp src/server/StreamingSession.h
git commit -m "feat: non-blocking inference in flushLoop using try_acquire"
```

---

### Task 3: Audio buffer high-water mark with client warning

**Files:**
- Modify: `src/whisper/StreamingWhisperEngine.h` (line 39 — change return type)
- Modify: `src/whisper/StreamingWhisperEngine.cpp` (~line 48)
- Modify: `src/server/StreamingSession.h` (~line 191 and ~line 436)
- Modify: `tests/unit/test_streaming_whisper_engine.cpp`

**Context:** The engine's 30s hard cap silently drops old audio. The new HWM (20s) catches overflow early and returns a flag the session uses to warn the client.

**Step 1: Write the three failing tests**

In `tests/unit/test_streaming_whisper_engine.cpp`, append at the end of the file (after the last test):

```cpp
TEST_F(StreamingWhisperEngineTest, ProcessAudioChunkReturnsFalseWhenBelowHWM) {
    StreamingWhisperEngine engine(ctx_);
    std::vector<float> chunk(1600, 0.0f); // 100ms
    bool overflow = engine.processAudioChunk(chunk);
    EXPECT_FALSE(overflow);
}

TEST_F(StreamingWhisperEngineTest, ProcessAudioChunkReturnsTrueWhenBufferAtHWM) {
    StreamingWhisperEngine engine(ctx_);
    // Fill exactly to HWM: 20s = 320000 samples
    constexpr size_t HWM = 16000 * 20;
    std::vector<float> fill(HWM, 0.0f);
    engine.processAudioChunk(fill);
    // Buffer is at HWM — next chunk must overflow
    std::vector<float> extra(1600, 0.0f);
    bool overflow = engine.processAudioChunk(extra);
    EXPECT_TRUE(overflow);
}

TEST_F(StreamingWhisperEngineTest, ProcessAudioChunkReturnsFalseAfterReset) {
    StreamingWhisperEngine engine(ctx_);
    constexpr size_t HWM = 16000 * 20;
    std::vector<float> fill(HWM, 0.0f);
    engine.processAudioChunk(fill);
    engine.reset(); // drain buffer
    std::vector<float> extra(1600, 0.0f);
    bool overflow = engine.processAudioChunk(extra);
    EXPECT_FALSE(overflow); // buffer cleared, no overflow
}
```

**Step 2: Run tests to verify they fail**

```bash
cmake --build build --target unit_tests -j$(nproc) 2>&1 | tail -5
```

Expected: compile error — `processAudioChunk` returns `void`, not `bool`.

**Step 3: Update `processAudioChunk` declaration in `StreamingWhisperEngine.h`**

In `src/whisper/StreamingWhisperEngine.h`, find line 39:

```cpp
    void processAudioChunk(const std::vector<float>& pcm_data);
```

Replace with:

```cpp
    /**
     * @brief Agregar chunk de audio al buffer.
     * @return true if the chunk was dropped because the buffer is at the 20s high-water mark.
     */
    bool processAudioChunk(const std::vector<float>& pcm_data);
```

**Step 4: Implement the HWM check in `StreamingWhisperEngine.cpp`**

In `src/whisper/StreamingWhisperEngine.cpp`, find the function definition (line 48):

```cpp
void StreamingWhisperEngine::processAudioChunk(const std::vector<float>& pcm_data) {
    std::lock_guard<std::mutex> lock(buffer_mutex_);

    std::vector<float> prepped_data = pcm_data;
    AudioPreprocessor::process(prepped_data, hp_prev_raw_, hp_prev_filtered_);
```

Replace the first three lines (signature + lock + prepped_data) with:

```cpp
bool StreamingWhisperEngine::processAudioChunk(const std::vector<float>& pcm_data) {
    std::lock_guard<std::mutex> lock(buffer_mutex_);

    // High-water mark: 20s = 320 000 samples. Drop incoming chunk if buffer is already full.
    // The hard cap (30s) is still enforced below for the overflow path; HWM provides early warning.
    constexpr size_t HIGH_WATER_MARK = 16000 * 20;
    if (audio_buffer_.size() >= HIGH_WATER_MARK) {
        return true; // chunk dropped — caller should warn the client
    }

    std::vector<float> prepped_data = pcm_data;
    AudioPreprocessor::process(prepped_data, hp_prev_raw_, hp_prev_filtered_);
```

At the end of the function, the existing code returns nothing (void). Add `return false;` before the closing brace:

Find (last few lines of the function body, around line 68):

```cpp
    } else {
        audio_buffer_.insert(audio_buffer_.end(), prepped_data.begin(), prepped_data.end());
    }
}
```

Replace with:

```cpp
    } else {
        audio_buffer_.insert(audio_buffer_.end(), prepped_data.begin(), prepped_data.end());
    }
    return false;
}
```

**Step 5: Update `StreamingSession` to handle the overflow flag**

In `src/server/StreamingSession.h`, add `buffer_overflowed_` member. Find the members block (~line 436):

```cpp
    bool configured_;
    size_t last_transcribed_size_;
```

Replace with:

```cpp
    bool configured_;
    bool buffer_overflowed_; // true while engine buffer is above 20s HWM
    size_t last_transcribed_size_;
```

In the constructor initializer list, find where `configured_` is initialized (~line 68, inside the constructor body):

```cpp
          flush_running_(false),
```

The initializer list sets members in declaration order — find the `configured_` initialization and add `buffer_overflowed_` next to it. Look for:

```cpp
        configured_(false),
```

Add after it:

```cpp
        buffer_overflowed_(false),
```

Now update `processAudioChunk` in `StreamingSession.h` (~line 191). Find:

```cpp
    void processAudioChunk(const std::vector<float>& audio) {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (!configured_ || !engine_) return;

        last_audio_time_ = std::chrono::steady_clock::now();
        engine_->processAudioChunk(audio);
        // Inference is handled entirely by flushLoop to avoid blocking the receive loop.
    }
```

Replace with:

```cpp
    void processAudioChunk(const std::vector<float>& audio) {
        std::unique_lock<std::mutex> lock(state_mutex_);
        if (!configured_ || !engine_) return;

        last_audio_time_ = std::chrono::steady_clock::now();
        bool overflow = engine_->processAudioChunk(audio);
        bool should_warn = overflow && !buffer_overflowed_;
        buffer_overflowed_ = overflow;
        lock.unlock();

        if (should_warn) {
            Log::warn("Audio buffer full, dropping incoming audio", session_id_);
            json warning = {
                {"type", "warning"},
                {"code", "buffer_full"},
                {"message", "Audio buffer full, dropping incoming audio"}
            };
            sendMessage(warning);
        }
        // Inference is handled entirely by flushLoop to avoid blocking the receive loop.
    }
```

**Step 6: Run full test suite**

```bash
cmake --build build --target unit_tests -j$(nproc) 2>&1 | tail -5
./build/tests/unit_tests 2>&1 | tail -10
```

Expected: all 63 tests pass (60 existing + 3 new).

**Step 7: Build server binary**

```bash
cmake --build build --target transcription_server -j$(nproc) 2>&1 | tail -5
```

Expected: compiles without errors or new warnings.

**Step 8: Commit**

```bash
git add src/whisper/StreamingWhisperEngine.h src/whisper/StreamingWhisperEngine.cpp \
        src/server/StreamingSession.h tests/unit/test_streaming_whisper_engine.cpp
git commit -m "feat: audio buffer HWM (20s) with JSON warning to client on overflow"
```
