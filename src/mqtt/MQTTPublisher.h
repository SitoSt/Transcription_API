#pragma once
#include <mosquittopp.h>
#include <atomic>
#include <string>
#include "MQTTConfig.h"
#include "TranscriptionEvent.h"

// Publishes transcription events to an MQTT broker.
//
// Only final transcriptions (is_final == true) are published.
// Partial results are silently ignored.
//
// Topic: config.topic  (e.g. "transcription", fully configurable)
//
// JSON payload:
//   {
//     "session_id":   "session-...",
//     "text":         "transcribed text",
//     "is_final":     true,
//     "language":     "es",
//     "timestamp_ms": 1709123456789
//   }
//
// Usage:
//   MQTTPublisher pub(MQTTConfig::fromEnv());
//   pub.connect();
//   pub.publishTranscription(event);
//   pub.disconnect();
//
// Thread safety: publishTranscription() is safe to call from multiple threads.

class MQTTPublisher : public mosqpp::mosquittopp {
public:
    explicit MQTTPublisher(const MQTTConfig& config);
    ~MQTTPublisher() override;

    // Connects to the broker and starts the background networking loop.
    // Returns false if the initial connect() call fails.
    bool connect();

    // Gracefully disconnects and stops the networking loop.
    void disconnect();

    bool isConnected() const;

    // Serialises event to JSON and publishes it to the appropriate topic.
    // Returns false if not connected or if mosquitto_publish fails.
    bool publishTranscription(const TranscriptionEvent& event);

private:
    // mosquittopp callbacks
    void on_connect(int rc) override;
    void on_disconnect(int rc) override;
    void on_publish(int mid) override;

    std::string buildTopic() const;
    std::string buildPayload(const TranscriptionEvent& event) const;

    MQTTConfig           config_;
    std::atomic<bool>    connected_{false};
};
