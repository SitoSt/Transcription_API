#pragma once
#include <boost/beast/websocket.hpp>
#include <boost/asio.hpp>
#include <memory>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "whisper/StreamingWhisperEngine.h"

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = net::ip::tcp;
using json = nlohmann::json;

/**
 * @brief Sesión de streaming para un cliente WebSocket
 * 
 * Cada sesión mantiene:
 * - StreamingWhisperEngine para transcripción
 * - SimpleVAD para detección de voz
 * - WebSocket stream para comunicación
 * 
 * Protocolo JSON:
 * - Cliente envía: {"type": "config", ...} o {"type": "audio", "data": "base64"}
 * - Servidor envía: {"type": "transcription", "text": "...", "is_final": true}
 */
class StreamingSession : public std::enable_shared_from_this<StreamingSession> {
public:
    /**
     * @brief Constructor
     * @param ws WebSocket stream (movido)
     * @param model_path Ruta al modelo de whisper
     */
    StreamingSession(
        websocket::stream<tcp::socket> ws,
        const std::string& model_path
    );
    
    /**
     * @brief Iniciar el loop de procesamiento
     */
    void run();
    
private:
    // Handlers de mensajes
    void handleJsonMessage(const std::string& message);
    void handleBinaryMessage(const std::vector<unsigned char>& data);
    
    void handleConfig(const json& msg);
    void handleEnd();
    
    // Procesamiento de audio
    void processAudioChunk(const std::vector<float>& audio);
    
    // Envío de mensajes al cliente
    void sendReady();
    void sendTranscription(const std::string& text, bool is_final);
    void sendError(const std::string& message, const std::string& code);
    void sendMessage(const json& msg);
    
    // Utilidades
    std::string generateSessionId();
    
    // Estado de la sesión
    websocket::stream<tcp::socket> ws_;
    std::unique_ptr<StreamingWhisperEngine> engine_;
    std::string session_id_;
    std::string model_path_;
    bool configured_;
    size_t last_transcribed_size_;
    
    // Configuración
    std::string language_;
};
