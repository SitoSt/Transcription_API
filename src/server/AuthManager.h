#pragma once
#include <memory>
#include <string>
#include "auth/ApiAuthConfig.h"
#include "auth/ApiAuthClient.h"
#include "auth/AuthCache.h"

class AuthManager {
public:
    explicit AuthManager(const ApiAuthConfig& config);

    bool isAuthEnabled() const;
    bool validate(const std::string& token);

private:
    std::unique_ptr<ApiAuthClient> api_client_;
    AuthCache cache_;
    bool auth_enabled_;
    int cache_ttl_seconds_;
    std::string static_token_; // non-empty = static token mode (no API call)

    // Constant-time comparison to resist timing attacks.
    static bool constantTimeEqual(const std::string& a, const std::string& b);
};
