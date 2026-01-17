# Real-Time C++ Transcription Microservice

A high-performance, real-time audio transcription microservice built with C++ and [whisper.cpp](https://github.com/ggerganov/whisper.cpp). Designed for low-latency streaming applications, it processes raw audio via WebSocket and delivers accurate text transcriptions using OpenAI's Whisper models.

## 🚀 Features

- **Real-Time Streaming**: Processes audio chunks as they arrive with minimal latency.
- **WebSocket Protocol**: Efficient, bidirectional communication for audio streaming and control.
- **High Performance**: Built on C++17 and optimized for both CPU (OpenBLAS) and GPU (CUDA/Metal).
- **Thread-Safe Architecture**: Robust circular buffer management and multi-threaded processing.
- **VAD (Voice Activity Detection)**: (Planned) Integrated voice activity detection for optimized processing.
- **Cross-Platform**: Runs on Linux (Ubuntu), macOS (Apple Silicon optimized), and Windows.

## 🏗️ Architecture

The system is composed of two main components:

1.  **StreamingSession**: Handles WebSocket connections, protocol parsing (JSON config/control), and binary audio reception.
2.  **StreamingWhisperEngine**: The core transcription engine. It manages a thread-safe circular buffer to handle incoming audio streams asynchronously and interfaces with `whisper.cpp` for inference.

## 🛠️ Getting Started

### Prerequisites

- **CMake** 3.16+
- **C++ Compiler** (GCC 7+, Clang 5+, MSVC 2017+)
- **Git**

### Build Instructions

1.  **Clone the repository**:
    ```bash
    git clone --recursive https://github.com/your-username/transcription-service.git
    cd transcription-service
    ```

2.  **Compile**:
    ```bash
    cmake -B build -DBUILD_TESTS=ON
    cmake --build build -j$(nproc)
    ```

3.  **Run Tests** (Optional but recommended):
    ```bash
    ./run_tests.sh
    ```

### Running the Server

Start the server listening on port `9001`:

```bash
./build/transcription_server --model /path/to/ggml-model.bin --bind 0.0.0.0 --auth-token YOUR_TOKEN --cert server.crt --key server.key
```

For a LAN-only deployment, set `--bind` to the server's local IP (for example the IP behind `transcript.local`).
To enable **TLS Encryption (HTTPS/WSS)**, provide both `--cert` and `--key`.

*Note: You can download models from the [whisper.cpp repository](https://github.com/ggerganov/whisper.cpp).*

## 📡 WebSocket Protocol

The service uses a mixed Text/Binary protocol over `ws://` or `wss://`:

1.  **Configuration** (JSON):
    *   Client -> Server: `{"type": "config", "language": "en", "token": "YOUR_TOKEN"}`
    *   Server -> Client: `{"type": "ready"}`

2.  **Audio Streaming** (Binary):
    *   Client -> Server: Raw PCM Audio Data (Float32, 16kHz, Mono).
    *   The server buffers this data efficiently.

3.  **Transcription** (JSON):
    *   Client -> Server: `{"type": "end"}` (Signals end of utterance/stream)
    *   Server -> Client: `{"type": "transcription", "text": "Hello world", "is_final": true}`

## 🧪 Testing

The project maintains high code quality with a suite of 13 unit tests covering:
- Model loading and initialization.
- Circular buffer logic and thread safety.
- Audio format conversion.
- Transcription accuracy.

Run them with:
```bash
cd build/tests && ./test_streaming_whisper
```

## 📦 Dependencies

- **whisper.cpp**: Core inference engine.
- **Boost.Beast & Boost.ASIO**: WebSocket and networking.
- **Google Test**: Unit testing framework.

---
*Built by Sito. Open for opportunities.*
