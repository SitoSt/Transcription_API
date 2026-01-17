#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>
#include <boost/beast/websocket.hpp>
#include <iostream>
#include <string>
#include <thread>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <sstream>
#include "server/StreamingSession.h"
#include "server/ServerConfig.h"
#include "server/ConnectionLimiter.h"
#include "server/ConnectionGuard.h"
#include "server/AuthManager.h"

using tcp = boost::asio::ip::tcp;
namespace websocket = boost::beast::websocket;
namespace ssl = boost::asio::ssl;

namespace {

void printUsage(const char* binary) {
    std::cout << "Usage: " << binary << " [--model path] [--bind address] [--auth-token token]"
              << " [--cert cert.pem] [--key key.pem]"
              << " [--max-connections N] [--max-connections-per-ip N]" << std::endl;
    std::cout << "If no flags are provided, the first argument is treated as model path." << std::endl;
}

ServerConfig parseArgs(int argc, char* argv[]) {
    ServerConfig config;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--help") {
            printUsage(argv[0]);
            std::exit(0);
        } else if (arg == "--model" && i + 1 < argc) {
            config.model_path = argv[++i];
        } else if (arg == "--bind" && i + 1 < argc) {
            config.bind_address = argv[++i];
        } else if (arg == "--auth-token" && i + 1 < argc) {
            config.auth_token = argv[++i];
        } else if (arg == "--cert" && i + 1 < argc) {
            config.cert_path = argv[++i];
        } else if (arg == "--key" && i + 1 < argc) {
            config.key_path = argv[++i];
        } else if (arg == "--max-connections" && i + 1 < argc) {
            config.max_connections = static_cast<size_t>(std::stoul(argv[++i]));
        } else if (arg == "--max-connections-per-ip" && i + 1 < argc) {
            config.max_connections_per_ip = static_cast<size_t>(std::stoul(argv[++i]));
        } else if (arg.rfind("--", 0) != 0 && config.model_path == "third_party/whisper.cpp/models/ggml-base.bin") {
            config.model_path = arg;
        } else {
            std::cerr << "Unknown argument: " << arg << std::endl;
            printUsage(argv[0]);
            std::exit(1);
        }
    }

    return config;
}

void handleSession(tcp::socket socket,
                   const std::shared_ptr<ConnectionLimiter>& limiter,
                   const std::string& client_ip,
                   const std::string& model_path,
                   const std::shared_ptr<AuthManager>& auth_manager,
                   std::shared_ptr<ssl::context> ssl_ctx) {
    ConnectionGuard guard(limiter, client_ip);

    try {
        if (ssl_ctx) {
            // Secure WebSocket (WSS)
            websocket::stream<ssl::stream<tcp::socket>> ws(std::move(socket), *ssl_ctx);
            
            // Perform SSL handshake
            ws.next_layer().handshake(ssl::stream_base::server);
            
            auto session = std::make_shared<StreamingSession<websocket::stream<ssl::stream<tcp::socket>>>>(
                std::move(ws), model_path, auth_manager
            );
            session->run();
        } else {
            // Plain WebSocket (WS)
            websocket::stream<tcp::socket> ws(std::move(socket));
            auto session = std::make_shared<StreamingSession<websocket::stream<tcp::socket>>>(
                std::move(ws), model_path, auth_manager
            );
            session->run();
        }
    } catch (std::exception& e) {
        std::cerr << "Session error (" << client_ip << "): " << e.what() << std::endl;
    }
}
} // namespace

int main(int argc, char* argv[]) {
    try {
        ServerConfig config = parseArgs(argc, argv);

        std::cout << "🎙️  Streaming Transcription Server" << std::endl;
        std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << std::endl;
        std::cout << "Model: " << config.model_path << std::endl;
        std::cout << "Bind:  " << config.bind_address << ":" << config.port << std::endl;
        
        bool use_ssl = !config.cert_path.empty() && !config.key_path.empty();
        std::cout << "SSL:   " << (use_ssl ? "Enabled" : "Disabled") << std::endl;
        
        std::cout << "Auth:  " << (config.auth_token.empty() ? "disabled" : "enabled") << std::endl;
        std::cout << "Max:   " << config.max_connections << " total, "
                  << config.max_connections_per_ip << " per IP" << std::endl;
        std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << std::endl;

        // Initialize SSL context if needed
        std::shared_ptr<ssl::context> ssl_ctx;
        if (use_ssl) {
            try {
                ssl_ctx = std::make_shared<ssl::context>(ssl::context::tlsv12);
                ssl_ctx->use_certificate_chain_file(config.cert_path);
                ssl_ctx->use_private_key_file(config.key_path, ssl::context::pem);
            } catch (std::exception& e) {
                std::cerr << "❌ SSL Init Error: " << e.what() << std::endl;
                return 1;
            }
        }

        boost::asio::io_context ioc;
        auto bind_address = boost::asio::ip::make_address(config.bind_address);
        tcp::acceptor acceptor(ioc, tcp::endpoint(bind_address, config.port));

        auto limiter = std::make_shared<ConnectionLimiter>(
            config.max_connections,
            config.max_connections_per_ip
        );

        auto auth_manager = std::make_shared<AuthManager>(config.auth_token);

        std::string protocol = use_ssl ? "wss" : "ws";
        std::cout << "\n🚀 Server listening on " << protocol << "://" << config.bind_address << ":" << config.port << std::endl;
        std::cout << "Waiting for connections...\n" << std::endl;

        while (true) {
            tcp::socket socket(ioc);
            acceptor.accept(socket);

            std::string client_ip = socket.remote_endpoint().address().to_string();

            if (!limiter->tryAcquire(client_ip)) {
                std::cerr << "✗ Connection rejected (limits): " << client_ip << std::endl;
                socket.close();
                continue;
            }

            std::cout << "✓ New connection accepted from " << client_ip << std::endl;

            try {
                std::thread(handleSession,
                            std::move(socket),
                            limiter,
                            client_ip,
                            config.model_path,
                            auth_manager,
                            ssl_ctx).detach();
            } catch (...) {
                limiter->release(client_ip);
                throw;
            }
        }
    } catch (std::exception& e) {
        std::cerr << "❌ Server error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
