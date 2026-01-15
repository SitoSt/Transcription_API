#include "StreamingSession.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <random>
#include <chrono>


StreamingSession::StreamingSession(
    websocket::stream<tcp::socket> ws,
    const std::string& model_path
)
    : ws_(std::move(ws)),
      model_path_(model_path),
      configured_(false),
      last_transcribed_size_(0),
      language_("es")
{
    session_id_ = generateSessionId();
    std::cout << "[Session " << session_id_ << "] Created" << std::endl;
}

void StreamingSession::run() {
    try {
        // Aceptar la conexión WebSocket
        ws_.accept();
        std::cout << "[Session " << session_id_ << "] WebSocket accepted" << std::endl;
        
        // Loop principal de mensajes
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
                // Binary data - Audio
                // Copiar datos del buffer
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

void StreamingSession::handleJsonMessage(const std::string& message) {
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

void StreamingSession::handleBinaryMessage(const std::vector<unsigned char>& data) {
    if (!configured_) {
        sendError("Session not configured. Send 'config' first.", "NOT_CONFIGURED");
        return;
    }

    try {
        std::cout << "[Session " << session_id_ << "] Received " << data.size() << " bytes of audio" << std::endl;
        
        // Convertir de unsigned char a float
        // Asumimos que los datos llegan en bytes, que son float32 little-endian
        if (data.size() % sizeof(float) != 0) {
            std::cerr << "Invalid audio data size: " << data.size() << std::endl;
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

void StreamingSession::handleConfig(const json& msg) {
    try {
        // Extraer configuración
        if (msg.contains("language")) {
            language_ = msg["language"];
        }
        
        // Inicializar motor de whisper
        engine_ = std::make_unique<StreamingWhisperEngine>(model_path_);
        engine_->setLanguage(language_);
        engine_->setThreads(4);
        
        configured_ = true;
        last_transcribed_size_ = 0;
        
        std::cout << "[Session " << session_id_ << "] Configured: lang=" << language_ << std::endl;
        
        // Enviar confirmación
        sendReady();
    }
    catch (std::exception& e) {
        sendError("Configuration failed: " + std::string(e.what()), "CONFIG_ERROR");
    }
}


void StreamingSession::handleEnd() {
    std::cout << "[Session " << session_id_ << "] Ending streaming..." << std::endl;

    // Transcribir lo que quede en el buffer (flush final)
    if (engine_) {
        std::string finalText = engine_->transcribe(); // TODO: Add force_flush param to engine if needed?
        // Asumimos que transcribe() normal pilla lo que queda si es el final, o deberiamos tener un flush()
        
        json msg = {
            {"type", "transcription"},
            {"text", finalText},
            {"is_final", true}
        };
        sendMessage(msg);
    }
    
    std::cout << "[Session " << session_id_ << "] Ending session" << std::endl;
    
    // Cerrar conexión gracefully
    try {
        ws_.close(websocket::close_code::normal);
    }
    catch (std::exception& e) {
        std::cerr << "[Session " << session_id_ << "] Close error: " << e.what() << std::endl;
    }
}

void StreamingSession::processAudioChunk(const std::vector<float>& audio) {
    if (!configured_ || !engine_) {
        std::cout << "[Session " << session_id_ << "] Ignored audio (not configured)" << std::endl;
        return;
    }

    // Agregar al buffer del motor
    engine_->processAudioChunk(audio);
    
    // Obtener tamaño actual del buffer
    size_t current_size = engine_->getBufferSize();
    
    // Detectar si el buffer se ha truncado o reiniciado
    if (current_size < last_transcribed_size_) {
        last_transcribed_size_ = 0;
    }
    
    // Transcribir solo si ha llegado suficiente audio NUEVO (1 segundo = 16000 samples)
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

void StreamingSession::sendReady() {
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

void StreamingSession::sendTranscription(const std::string& text, bool is_final) {
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()
    ).count();
    
    json msg = {
        {"type", "transcription"},
        {"text", text},
        {"is_final", is_final},
        {"timestamp", timestamp}
    };
    sendMessage(msg);
}



void StreamingSession::sendError(const std::string& message, const std::string& code) {
    json msg = {
        {"type", "error"},
        {"message", message},
        {"code", code}
    };
    sendMessage(msg);
}

void StreamingSession::sendMessage(const json& msg) {
    try {
        std::string str = msg.dump();
        ws_.text(true);
        ws_.write(net::buffer(str));
    }
    catch (std::exception& e) {
        std::cerr << "[Session " << session_id_ << "] Send error: " << e.what() << std::endl;
    }
}



std::string StreamingSession::generateSessionId() {
    // Generar UUID simple (timestamp + random)
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()
    ).count();
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1000, 9999);
    
    std::ostringstream oss;
    oss << "session-" << timestamp << "-" << dis(gen);
    return oss.str();
}
