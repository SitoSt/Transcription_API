#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace beast = boost::beast;   // Para Beast
namespace http = beast::http;     // Para HTTP
namespace net = boost::asio;      // Para Asio
using tcp = net::ip::tcp;         // Para TCP

// ------------------------------------------------------------------
// 1. MANEJADOR DE SESIÓN (Procesa la Solicitud)
// ------------------------------------------------------------------
void handle_session(tcp::socket socket) {
    try {
        // Objeto para leer la solicitud HTTP. Usamos beast::flat_buffer
        // para almacenar el búfer de entrada.
        beast::flat_buffer buffer;
        http::request<http::string_body> req;

        // Lee la solicitud del cliente
        http::read(socket, buffer, req);

        // ----------------------------------------------------------
        // LÓGICA DEL ENDPOINT: /transcribe (POST)
        // ----------------------------------------------------------
        if (req.method() == http::verb::post && req.target() == "/transcribe") {
            
            // 1. Extraer los datos de audio
            // El cuerpo de la solicitud (req.body()) contiene los bytes de audio.
            // Si el audio es binario, es mejor usar http::request<http::vector_body<char>> 
            // para evitar la interpretación como string, pero para este esquema string_body 
            // sirve para ejemplificar la extracción.
            std::string audio_data = req.body();
            std::cout << "Recibida solicitud POST en /transcribe." << std::endl;
            std::cout << "Tamaño de los datos de audio: " << audio_data.size() << " bytes." << std::endl;

            // 2. Implementación de la Transcripción (Tu código iría aquí)
            // Llama a tu función de transcripción:
            // std::string transcribed_text = transcribe_audio(audio_data);
            std::string transcribed_text = "¡El audio se ha transcrito con éxito!"; 

            // 3. Construir la Respuesta
            http::response<http::string_body> res{http::status::ok, req.version()};
            res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
            res.set(http::field::content_type, "text/plain");
            res.keep_alive(req.keep_alive());
            res.body() = transcribed_text;
            res.prepare_payload();

            // Envía la respuesta al cliente
            http::write(socket, res);

        } else {
            // Manejar otras rutas o métodos
            http::response<http::string_body> res{http::status::not_found, req.version()};
            res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
            res.set(http::field::content_type, "text/plain");
            res.body() = "Ruta no encontrada.";
            res.prepare_payload();
            http::write(socket, res);
        }

    } catch (beast::system_error const& se) {
        if(se.code() != beast::errc::end_of_file)
            std::cerr << "Error en la sesión: " << se.code().message() << std::endl;
    } catch (std::exception const& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

// ------------------------------------------------------------------
// 2. SERVIDOR PRINCIPAL (Acepta Conexiones)
// ------------------------------------------------------------------
int main(int argc, char* argv[]) {
    // Definiciones del Servidor
    auto const address = net::ip::make_address("0.0.0.0"); // Escucha en todas las interfaces
    unsigned short const port = 8080;
    int const num_threads = 4; // Número de hilos para el pool

    // Contexto de E/S de Asio (Necesario para toda la actividad de E/S)
    net::io_context ioc{num_threads};

    // Objeto 'Acceptor' para aceptar nuevas conexiones
    tcp::acceptor acceptor{ioc, {address, port}};

    // Bucle principal del servidor para aceptar y manejar conexiones
    std::cout << "Servidor escuchando en http://0.0.0.0:" << port << " con " << num_threads << " hilos." << std::endl;
    
    // Ejecutar el Acceptor de forma asíncrona para que el hilo principal no se bloquee
    // y pueda empezar a aceptar conexiones.
    auto listen_handler = [&]() {
        for (;;) {
            // Crea un nuevo socket para la conexión entrante
            tcp::socket socket{ioc};
            
            // Bloquea hasta que una conexión sea aceptada
            acceptor.accept(socket);
            
            // Lanza la sesión en un nuevo hilo o envíala a un pool de hilos
            // Aquí usamos std::thread para simplificar el ejemplo de pool de hilos.
            std::thread(handle_session, std::move(socket)).detach();
        }
    };

    // Lanzar el pool de hilos para ejecutar el io_context (necesario para la asincronía)
    std::vector<std::thread> threads;
    threads.reserve(num_threads - 1);
    for(int i = 0; i < num_threads - 1; ++i) {
        threads.emplace_back([&ioc] {
            ioc.run();
        });
    }

    // El hilo principal también ejecuta el io_context (o el listen_handler)
    listen_handler(); // Puedes reemplazar esto con ioc.run() si usas 'async_accept'

    return 0;
}