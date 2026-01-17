#pragma once
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <memory>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include <iostream>
#include "whisper/StreamingWhisperEngine.h"
#include "AuthManager.h"
#include "ConnectionGuard.h"

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = net::ip::tcp;
using json = nlohmann::json;

template <class StreamType>
class StreamingSession : public std::enable_shared_from_this<StreamingSession<StreamType>> {
public:
    StreamingSession(
        StreamType&& ws,
        const std::string& model_path,
        std::shared_ptr<AuthManager> auth_manager
    )
        : ws_(std::move(ws)),
          model_path_(model_path),
          auth_manager_(auth_manager),
          configured_(false),
          last_transcribed_size_(0),
          language_("es")
    {
        session_id_ = generateSessionId();
        std::cout << "[Session " << session_id_ << "] Created" << std::endl;
    }
    
    void run() {
        // Need to run session logic. 
        // For SSL, we might need handshake, but usually run() is called AFTER handshake in server loop?
        // Actually, websocket handshake is done here.
        
        // We need to capture shared_from_this() for async operations if we were fully async.
        // But the previous implementation was synchronous (while loop).
        // Let's keep it synchronous as per original implementation for now, or minimal changes.
        
        try {
            // Accept the websocket handshake
            ws_.accept();
            std::cout << "[Session " << session_id_ << "] WebSocket accepted" << std::endl;
            
            while (true) {
                beast::flat_buffer buffer;
                ws_.read(buffer);
                
                if (ws_.got_text()) {
                    std::string message(
                        boost::asio::buffers_begin(buffer.data()),
                        boost::asio::buffers_end(buffer.data())
                    );
                    handleJsonMessage(message);
                } else {
                    std::vector<unsigned char> data(buffer.size());
                    boost::asio::buffer_copy(boost::asio::buffer(data), buffer.data());
                    handleBinaryMessage(data);
                }
            }
        }
        catch (beast::system_error const& se) {
            if (se.code() != websocket::error::closed)
                std::cerr << "Error: " << se.code().message() << std::endl;
        }
        catch (std::exception const& e) {
            std::cerr << "Error: " << e.what() << std::endl;
        }
    }
    
private:
    void handleJsonMessage(const std::string& message) {
        try {
            json msg = json::parse(message);
            
            if (!msg.contains("type")) {
                sendError("Missing 'type' field", "INVALID_MESSAGE");
                return;
            }
            
            std::string type = msg["type"];
            
            if (type == "config") {
                handleConfig(msg);
            }
            else if (!configured_) {
                sendError("Session not configured. Send 'config' first.", "NOT_CONFIGURED");
                return;
            }
            else if (type == "end") {
                handleEnd();
            }
            else {
                sendError("Unknown message type: " + type, "UNKNOWN_TYPE");
            }
        }
        catch (json::parse_error& e) {
            sendError("Invalid JSON: " + std::string(e.what()), "PARSE_ERROR");
        }
    }

    void handleBinaryMessage(const std::vector<unsigned char>& data) {
         if (!configured_) {
            sendError("Session not configured. Send 'config' first.", "NOT_CONFIGURED");
            return;
        }

        try {
            // std::cout << "[Session " << session_id_ << "] Received " << data.size() << " bytes" << std::endl;
            
            if (data.size() % sizeof(float) != 0) {
                return;
            }

            auto data_ptr = reinterpret_cast<const float*>(data.data());
            size_t size = data.size() / sizeof(float);
            std::vector<float> pcm(data_ptr, data_ptr + size);
            
            processAudioChunk(pcm);
        }
        catch (std::exception& e) {
            sendError("Binary audio processing failed: " + std::string(e.what()), "AUDIO_ERROR");
        }
    }
    
    void handleConfig(const json& msg) {
        try {
            if (auth_manager_->isAuthEnabled()) {
                if (!msg.contains("token") || !msg["token"].is_string()) {
                    sendError("Missing or invalid 'token'", "AUTH_REQUIRED");
                    ws_.close(websocket::close_code::policy_error);
                    return;
                }

                std::string token = msg["token"];
                if (!auth_manager_->validate(token)) {
                    sendError("Invalid token", "AUTH_FAILED");
                    ws_.close(websocket::close_code::policy_error);
                    return;
                }
            }

            if (msg.contains("language")) {
                language_ = msg["language"];
            }
            
            engine_ = std::make_unique<StreamingWhisperEngine>(model_path_);
            engine_->setLanguage(language_);
            engine_->setThreads(4);
            
            configured_ = true;
            last_transcribed_size_ = 0;
            
            std::cout << "[Session " << session_id_ << "] Configured: lang=" << language_ << std::endl;
            sendReady();
        }
        catch (std::exception& e) {
            sendError("Configuration failed: " + std::string(e.what()), "CONFIG_ERROR");
        }
    }

    void handleEnd() {
        std::cout << "[Session " << session_id_ << "] Ending streaming..." << std::endl;

        if (engine_) {
            std::string finalText = engine_->transcribe();
            json msg = {
                {"type", "transcription"},
                {"text", finalText},
                {"is_final", true}
            };
            sendMessage(msg);
        }
        
        std::cout << "[Session " << session_id_ << "] Ending session" << std::endl;
        
        try {
            ws_.close(websocket::close_code::normal);
        }
        catch (std::exception& e) {
            std::cerr << "[Session " << session_id_ << "] Close error: " << e.what() << std::endl;
        }
    }
    
    void processAudioChunk(const std::vector<float>& audio) {
        if (!configured_ || !engine_) return;

        engine_->processAudioChunk(audio);
        
        size_t current_size = engine_->getBufferSize();
        if (current_size < last_transcribed_size_) {
            last_transcribed_size_ = 0;
        }
        
        const size_t MIN_NEW_SAMPLES = 16000;
        if (current_size >= MIN_NEW_SAMPLES && 
            (current_size - last_transcribed_size_ >= MIN_NEW_SAMPLES)) {
            
            last_transcribed_size_ = current_size;
            std::string partialText = engine_->transcribe();
            
            if (!partialText.empty()) {
                json msg = {
                    {"type", "transcription"},
                    {"text", partialText},
                    {"is_final", false}
                };
                sendMessage(msg);
            }
        }
    }
    
    void sendReady() {
        json msg = {
            {"type", "ready"},
            {"session_id", session_id_},
            {"config", {
                {"language", language_},
                {"sample_rate", 16000}
            }}
        };
        sendMessage(msg);
    }

    void sendError(const std::string& message, const std::string& code) {
        json msg = {
            {"type", "error"},
            {"message", message},
            {"code", code}
        };
        sendMessage(msg);
    }

    void sendMessage(const json& msg) {
        try {
            std::string str = msg.dump();
            ws_.text(true);
            ws_.write(net::buffer(str));
        }
        catch (std::exception& e) {
            std::cerr << "[Session " << session_id_ << "] Send error: " << e.what() << std::endl;
        }
    }
    
    std::string generateSessionId() {
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()
        ).count();
        
        // Simple random generator
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(1000, 9999);
        
        std::ostringstream oss;
        oss << "session-" << timestamp << "-" << dis(gen);
        return oss.str();
    }
    
    // Member variables
    StreamType ws_;
    std::string model_path_;
    std::shared_ptr<AuthManager> auth_manager_;
    std::unique_ptr<StreamingWhisperEngine> engine_;
    std::string session_id_;
    bool configured_;
    size_t last_transcribed_size_;
    std::string language_;
};
