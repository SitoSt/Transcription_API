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
                // Convertir buffer a string
                std::string message(
                    boost::asio::buffers_begin(buffer.data()),
                    boost::asio::buffers_end(buffer.data())
                );
                handleJsonMessage(message);
            } else {
                // Procesar mensaje binario (audio)
                handleBinaryMessage(buffer);
            }
        }
    }
    catch (beast::system_error const& se) {
        if (se.code() != websocket::error::closed) {
            std::cerr << "[Session " << session_id_ << "] Error: " << se.what() << std::endl;
        }
    }
    catch (std::exception const& e) {
        std::cerr << "[Session " << session_id_ << "] Exception: " << e.what() << std::endl;
        sendError(e.what(), "INTERNAL_ERROR");
    }
    
    std::cout << "[Session " << session_id_ << "] Closed" << std::endl;
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

void StreamingSession::handleBinaryMessage(const beast::flat_buffer& buffer) {
    if (!configured_) {
        sendError("Session not configured. Send 'config' first.", "NOT_CONFIGURED");
        return;
    }

    try {
        // Asumimos que los datos son float32 little-endian
        auto data = buffer.data();
        size_t size = buffer.size();
        
        if (size % sizeof(float) != 0) {
            // TODO: Si no es múltiplo de float, algo anda mal, pero intentamos procesar lo que se pueda
            // o simplemente ignoramos el resto.
        }
        
        size_t num_samples = size / sizeof(float);
        std::vector<float> audio(num_samples);
        
        // Copiar datos del buffer a vector<float>
        // boost::asio::buffer_copy podría usarse, o memcpy si es contiguo.
        // beast::flat_buffer garantiza contiguidad? No necesariamente.
        // Usamos buffers_begin/end para iterar o copiar.
        
        // Forma segura de copiar desde sequence de buffers:
        net::buffer_copy(
            net::buffer(audio.data(), audio.size() * sizeof(float)),
            data
        );
        
        if (audio.empty()) {
            return;
        }
        
        processAudioChunk(audio);
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
        
        std::cout << "[Session " << session_id_ << "] Configured: lang=" << language_ << std::endl;
        
        // Enviar confirmación
        sendReady();
    }
    catch (std::exception& e) {
        sendError("Configuration failed: " + std::string(e.what()), "CONFIG_ERROR");
    }
}



void StreamingSession::handleEnd() {
    // Transcribir cualquier audio restante
    if (engine_ && engine_->getBufferSize() > 0) {
        try {
            std::cout << "[Session " << session_id_ << "] Final transcription of " 
                      << engine_->getBufferSize() << " samples..." << std::endl;
            
            std::string text = engine_->transcribe();
            if (!text.empty()) {
                std::cout << "[Session " << session_id_ << "] Final result: " << text << std::endl;
                sendTranscription(text, true);
            }
        }
        catch (std::exception& e) {
            std::cerr << "[Session " << session_id_ << "] Final transcription error: " 
                      << e.what() << std::endl;
        }
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
    // Simplemente agregar al buffer del motor
    engine_->processAudioChunk(audio);
    
    // NO transcribir aquí - solo acumular
    // La transcripción se hará cuando el cliente envíe "end"
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
