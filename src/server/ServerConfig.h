#pragma once
#include <string>

struct ServerConfig {
    std::string model_path = "third_party/whisper.cpp/models/ggml-base.bin";
    std::string bind_address = "0.0.0.0";
    unsigned short port = 9001;
    std::string auth_token;
    std::string cert_path;
    std::string key_path;
    size_t max_connections = 8;
    size_t max_connections_per_ip = 2;
};
