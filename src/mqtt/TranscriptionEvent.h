#pragma once
#include <string>
#include <cstdint>
#include <chrono>

// Plain data struct representing a transcription result to be published via MQTT.
struct TranscriptionEvent {
    std::string session_id;
    std::string text;
    bool        is_final  = false;
    std::string language;

    // Unix timestamp in milliseconds (set automatically by make())
    int64_t timestamp_ms  = 0;

    // Factory: fills timestamp_ms with current wall-clock time.
    static TranscriptionEvent make(const std::string& session_id,
                                   const std::string& text,
                                   bool               is_final,
                                   const std::string& language)
    {
        using namespace std::chrono;
        TranscriptionEvent ev;
        ev.session_id    = session_id;
        ev.text          = text;
        ev.is_final      = is_final;
        ev.language      = language;
        ev.timestamp_ms  = duration_cast<milliseconds>(
                               system_clock::now().time_since_epoch()).count();
        return ev;
    }
};
