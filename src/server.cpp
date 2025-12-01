#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/websocket.hpp>
#include <iostream>
#include <string>
#include <thread>
#include <memory>
#include "server/StreamingSession.h"

using tcp = boost::asio::ip::tcp;
namespace websocket = boost::beast::websocket;

// Ruta al modelo de whisper (puede ser argumento de línea de comandos)
std::string g_model_path = "third_party/whisper.cpp/models/ggml-base.bin";

/**
 * @brief Manejar una sesión de cliente
 */
void handleSession(tcp::socket socket) {
    try {
        // Crear WebSocket stream
        websocket::stream<tcp::socket> ws(std::move(socket));
        
        // Crear sesión de streaming
        auto session = std::make_shared<StreamingSession>(std::move(ws), g_model_path);
        
        // Ejecutar sesión (bloqueante hasta que el cliente se desconecte)
        session->run();
    }
    catch (std::exception& e) {
        std::cerr << "Session error: " << e.what() << std::endl;
    }
}

/**
 * @brief Servidor WebSocket principal
 */
int main(int argc, char* argv[]) {
    try {
        // Parsear argumentos
        if (argc > 1) {
            g_model_path = argv[1];
        }
        
        std::cout << "🎙️  Streaming Transcription Server" << std::endl;
        std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << std::endl;
        std::cout << "Model: " << g_model_path << std::endl;
        std::cout << "Port:  9001" << std::endl;
        std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << std::endl;
        
        // Crear contexto de IO
        boost::asio::io_context ioc;
        
        // Crear acceptor en puerto 9001
        tcp::acceptor acceptor(ioc, tcp::endpoint(tcp::v4(), 9001));
        
        std::cout << "\n🚀 Server listening on ws://localhost:9001" << std::endl;
        std::cout << "Waiting for connections...\n" << std::endl;
        
        // Loop principal de aceptación
        while (true) {
            tcp::socket socket(ioc);
            acceptor.accept(socket);
            
            std::cout << "✓ New connection accepted" << std::endl;
            
            // Crear thread para manejar la sesión
            std::thread(handleSession, std::move(socket)).detach();
        }
    }
    catch (std::exception& e) {
        std::cerr << "❌ Server error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
