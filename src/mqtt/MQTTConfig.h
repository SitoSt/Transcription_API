#pragma once
#include <string>
#include <cstdlib>

struct MQTTConfig {
    std::string broker_host    = "localhost";
    int         broker_port    = 1883;
    std::string topic          = "transcription";
    std::string client_id      = "transcription_server";
    int         keepalive_secs = 60;
    int         qos            = 1;
    bool        retain         = false;

    // Build config from an mqtt://host:port URL string.
    // Returns default config if the URL is empty or malformed.
    static MQTTConfig fromUrl(const std::string& url,
                              const std::string& topic     = "transcription",
                              const std::string& client_id = "transcription_server");

    // Read MQTT_URL from environment variable and call fromUrl().
    static MQTTConfig fromEnv(const std::string& env_var  = "MQTT_URL",
                              const std::string& topic     = "transcription",
                              const std::string& client_id = "transcription_server");
};
