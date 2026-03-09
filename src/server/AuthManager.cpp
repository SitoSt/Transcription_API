#include "AuthManager.h"
#include "log/Log.h"

AuthManager::AuthManager(const ApiAuthConfig& config)
    : auth_enabled_(!config.api_base_url.empty() || !config.static_token.empty()),
      cache_ttl_seconds_(config.cache_ttl_seconds),
      static_token_(config.static_token) {
    if (!config.api_base_url.empty()) {
        api_client_ = std::make_unique<ApiAuthClient>(config);
        Log::info("Auth enabled (API: " + config.api_base_url +
                  ", cache TTL: " + std::to_string(config.cache_ttl_seconds) + "s)");
    } else if (!config.static_token.empty()) {
        Log::info("Auth enabled (static token)");
    } else {
        Log::info("Auth disabled");
    }
}

// static
bool AuthManager::constantTimeEqual(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    volatile int diff = 0;
    for (size_t i = 0; i < a.size(); ++i) {
        diff |= static_cast<unsigned char>(a[i]) ^ static_cast<unsigned char>(b[i]);
    }
    return diff == 0;
}

bool AuthManager::isAuthEnabled() const {
    return auth_enabled_;
}

bool AuthManager::validate(const std::string& token) {
    if (!auth_enabled_) {
        return true;
    }

    // Static token mode — constant-time comparison, no API call, no cache.
    if (!static_token_.empty()) {
        bool ok = constantTimeEqual(token, static_token_);
        Log::debug(std::string("Static token auth: ") + (ok ? "ok" : "denied"));
        return ok;
    }

    const std::string masked = Log::maskKey(token);

    // Cache hit?
    auto cached = cache_.get(token);
    if (cached.has_value()) {
        Log::debug("Auth cache hit key=" + masked + " valid=" + (cached.value() ? "true" : "false"));
        return cached.value();
    }

    Log::debug("Auth cache miss key=" + masked + ", querying API");
    AuthResult result = api_client_->validate(token);

    if (result == AuthResult::ApiUnavailable) {
        Log::warn("Auth API unavailable for key=" + masked + ", denying (fail-closed)");
        return false;
    }

    bool allowed = (result == AuthResult::Allowed);
    Log::debug("Caching auth result key=" + masked +
               " allowed=" + (allowed ? "true" : "false") +
               " ttl=" + std::to_string(cache_ttl_seconds_) + "s");
    cache_.put(token, allowed, cache_ttl_seconds_);
    return allowed;
}
