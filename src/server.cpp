#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>
#include <boost/beast/websocket.hpp>
#include <iostream>
#include <fstream>
#include <string>
#include <thread>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <sstream>
#include <cstdlib>
#include "server/StreamingSession.h"
#include "server/ServerConfig.h"
#include "server/ConnectionLimiter.h"
#include "server/ConnectionGuard.h"
#include "server/AuthManager.h"
#include "auth/ApiAuthConfig.h"

using tcp = boost::asio::ip::tcp;
namespace websocket = boost::beast::websocket;
namespace ssl = boost::asio::ssl;

namespace {

// ---------------------------------------------------------------------------
// .env loader
// Reads KEY=VALUE lines from `path` and sets them in the process environment
// without overwriting variables that are already set.
// Supports:
//   - Blank lines and lines starting with '#' are ignored.
//   - Optional surrounding quotes on the value: KEY="value" or KEY='value'.
//   - Inline comments after an unquoted value are NOT stripped (keep it simple).
// ---------------------------------------------------------------------------
void loadDotEnv(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return; // No .env file — that's fine
    }

    std::string line;
    while (std::getline(file, line)) {
        // Strip trailing \r
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        // Skip blank lines and comments
        if (line.empty() || line[0] == '#') {
            continue;
        }

        auto eq = line.find('=');
        if (eq == std::string::npos) {
            continue;
        }

        std::string key   = line.substr(0, eq);
        std::string value = line.substr(eq + 1);

        // Strip surrounding quotes (" or ')
        if (value.size() >= 2) {
            char q = value.front();
            if ((q == '"' || q == '\'') && value.back() == q) {
                value = value.substr(1, value.size() - 2);
            }
        }

        // Only set if not already defined in the environment
        if (!key.empty() && ::getenv(key.c_str()) == nullptr) {
            ::setenv(key.c_str(), value.c_str(), 0);
        }
    }
}

// Helper: returns getenv(var) or "" if not set.
std::string env(const char* var) {
    const char* v = ::getenv(var);
    return v ? std::string(v) : std::string{};
}

// Build a ServerConfig from environment variables (used as defaults before
// CLI parsing overrides them).
ServerConfig configFromEnv() {
    ServerConfig cfg;

    if (auto v = env("MODEL_PATH"); !v.empty())
        cfg.model_path = v;

    if (auto v = env("BIND_ADDRESS"); !v.empty())
        cfg.bind_address = v;

    if (auto v = env("PORT"); !v.empty())
        cfg.port = static_cast<unsigned short>(std::stoul(v));

    if (auto v = env("AUTH_API_URL"); !v.empty())
        cfg.auth_api_url = v;

    if (auto v = env("AUTH_API_SECRET"); !v.empty())
        cfg.auth_api_secret = v;

    if (auto v = env("AUTH_CACHE_TTL"); !v.empty())
        cfg.auth_cache_ttl = std::stoi(v);

    if (auto v = env("AUTH_API_TIMEOUT"); !v.empty())
        cfg.auth_api_timeout = std::stoi(v);

    if (auto v = env("TLS_CERT"); !v.empty())
        cfg.cert_path = v;

    if (auto v = env("TLS_KEY"); !v.empty())
        cfg.key_path = v;

    if (auto v = env("MAX_CONNECTIONS"); !v.empty())
        cfg.max_connections = static_cast<size_t>(std::stoul(v));

    if (auto v = env("MAX_CONNECTIONS_PER_IP"); !v.empty())
        cfg.max_connections_per_ip = static_cast<size_t>(std::stoul(v));

    return cfg;
}

// ---------------------------------------------------------------------------

void printUsage(const char* binary) {
    std::cout << "Usage: " << binary
              << " [--model path] [--bind address] [--port N]"
              << " [--auth-api-url URL] [--auth-api-secret SECRET]"
              << " [--auth-cache-ttl N] [--auth-api-timeout N]"
              << " [--cert cert.pem] [--key key.pem]"
              << " [--max-connections N] [--max-connections-per-ip N]"
              << " [--env-file path]" << std::endl;
    std::cout << "All options can also be set via environment variables (or a .env file):" << std::endl;
    std::cout << "  MODEL_PATH, BIND_ADDRESS, PORT," << std::endl;
    std::cout << "  AUTH_API_URL, AUTH_API_SECRET, AUTH_CACHE_TTL, AUTH_API_TIMEOUT," << std::endl;
    std::cout << "  TLS_CERT, TLS_KEY, MAX_CONNECTIONS, MAX_CONNECTIONS_PER_IP" << std::endl;
    std::cout << "CLI arguments override environment variables." << std::endl;
}

// First pass: extract --env-file if present, load it, then build full config.
std::string extractEnvFile(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--env-file" && i + 1 < argc) {
            return argv[i + 1];
        }
    }
    return ".env"; // default
}

ServerConfig parseArgs(int argc, char* argv[]) {
    // Start with env-based defaults
    ServerConfig config = configFromEnv();

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--help") {
            printUsage(argv[0]);
            std::exit(0);
        } else if (arg == "--env-file" && i + 1 < argc) {
            ++i; // already handled before parseArgs
        } else if (arg == "--model" && i + 1 < argc) {
            config.model_path = argv[++i];
        } else if (arg == "--bind" && i + 1 < argc) {
            config.bind_address = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            config.port = static_cast<unsigned short>(std::stoul(argv[++i]));
        } else if (arg == "--auth-api-url" && i + 1 < argc) {
            config.auth_api_url = argv[++i];
        } else if (arg == "--auth-api-secret" && i + 1 < argc) {
            config.auth_api_secret = argv[++i];
        } else if (arg == "--auth-cache-ttl" && i + 1 < argc) {
            config.auth_cache_ttl = std::stoi(argv[++i]);
        } else if (arg == "--auth-api-timeout" && i + 1 < argc) {
            config.auth_api_timeout = std::stoi(argv[++i]);
        } else if (arg == "--cert" && i + 1 < argc) {
            config.cert_path = argv[++i];
        } else if (arg == "--key" && i + 1 < argc) {
            config.key_path = argv[++i];
        } else if (arg == "--max-connections" && i + 1 < argc) {
            config.max_connections = static_cast<size_t>(std::stoul(argv[++i]));
        } else if (arg == "--max-connections-per-ip" && i + 1 < argc) {
            config.max_connections_per_ip = static_cast<size_t>(std::stoul(argv[++i]));
        } else if (arg == "--thread-safe") {
            // accepted for backwards compatibility
        } else if (arg.rfind("--", 0) != 0 &&
                   config.model_path == "third_party/whisper.cpp/models/ggml-base.bin") {
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
            websocket::stream<ssl::stream<tcp::socket>> ws(std::move(socket), *ssl_ctx);
            ws.next_layer().handshake(ssl::stream_base::server);
            auto session = std::make_shared<StreamingSession<websocket::stream<ssl::stream<tcp::socket>>>>(
                std::move(ws), model_path, auth_manager
            );
            session->run();
        } else {
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
        // Load .env before anything else (CLI --env-file overrides the default path)
        loadDotEnv(extractEnvFile(argc, argv));

        ServerConfig config = parseArgs(argc, argv);

        std::cout << "🎙️  Streaming Transcription Server" << std::endl;
        std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << std::endl;
        std::cout << "Model: " << config.model_path << std::endl;
        std::cout << "Bind:  " << config.bind_address << ":" << config.port << std::endl;

        bool use_ssl = !config.cert_path.empty() && !config.key_path.empty();
        std::cout << "SSL:   " << (use_ssl ? "Enabled" : "Disabled") << std::endl;

        bool auth_enabled = !config.auth_api_url.empty();
        std::cout << "Auth:  " << (auth_enabled ? "API (" + config.auth_api_url + ")" : "disabled") << std::endl;
        if (auth_enabled) {
            std::cout << "       Cache TTL: " << config.auth_cache_ttl << "s"
                      << "  Timeout: " << config.auth_api_timeout << "s" << std::endl;
        }
        std::cout << "Max:   " << config.max_connections << " total, "
                  << config.max_connections_per_ip << " per IP" << std::endl;
        std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << std::endl;

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

        ApiAuthConfig auth_config;
        auth_config.api_base_url      = config.auth_api_url;
        auth_config.api_secret_key    = config.auth_api_secret;
        auth_config.cache_ttl_seconds = config.auth_cache_ttl;
        auth_config.timeout_seconds   = config.auth_api_timeout;
        auto auth_manager = std::make_shared<AuthManager>(auth_config);

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
