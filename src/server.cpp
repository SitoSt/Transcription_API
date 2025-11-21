#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/websocket.hpp>
#include <iostream>
#include <string>
#include <vector>
#include "WhisperWrapper.h"

using tcp = boost::asio::ip::tcp;
namespace websocket = boost::beast::websocket;

// ---------------------------------------------------------------------------
// Simulación de tu transcriptor
// Sustituye esta función por tu implementación real
// ---------------------------------------------------------------------------
std::string transcribeAudio(const std::vector<uint8_t>& data) {
    WhisperWrapper* ww = WhisperWrapper::instance("../models/ggml-medium.bin", "../bin/whisper-cli");
    ww.transcribe()
    return "Transcripción generada, bro. (" + std::to_string(data.size()) + " bytes recibidos)";
}

// ---------------------------------------------------------------------------
// Sesión WebSocket
// ---------------------------------------------------------------------------
void do_session(tcp::socket socket) {
    try {
        websocket::stream<tcp::socket> ws(std::move(socket));

        // Aceptar conexión WebSocket
        ws.accept();

        while (true) {
            boost::beast::flat_buffer buffer;
            ws.read(buffer);

            // Convertir buffer → vector<uint8_t>
            std::vector<uint8_t> received;

            auto seq = buffer.data();
            received.reserve(boost::asio::buffer_size(seq));

            for (auto it = boost::asio::buffers_begin(seq);
                it != boost::asio::buffers_end(seq);
                ++it)
            {
                received.push_back(*it);
            }

            // Llamar tu transcriptor
            std::string result = transcribeAudio(received);

            // Responder texto
            ws.text(true);
            ws.write(boost::asio::buffer(result));
        }
    }
    catch (std::exception& e) {
        std::cerr << "Error en sesión: " << e.what() << "\n";
    }
}

// ---------------------------------------------------------------------------
// Servidor WebSocket
// ---------------------------------------------------------------------------
int main() {
    try {

        boost::asio::io_context ioc;

        tcp::acceptor acceptor(ioc, tcp::endpoint(tcp::v4(), 9001));

        std::cout << "🔥 Servidor WebSocket escuchando en ws://localhost:9001\n";

        while (true) {
            tcp::socket socket(ioc);
            acceptor.accept(socket);
            std::thread(&do_session, std::move(socket)).detach();
        }
    }
    catch (std::exception& e) {
        std::cerr << "Error en servidor: " << e.what() << "\n";
    }
}