#include "AuthManager.h"
#include <algorithm>

namespace {
    // Constant-time string comparison to prevent timing attacks
    bool secureCompare(const std::string& a, const std::string& b) {
        if (a.size() != b.size()) {
            return false;
        }
        unsigned char result = 0;
        for (size_t i = 0; i < a.size(); ++i) {
            result |= (a[i] ^ b[i]);
        }
        return result == 0;
    }
}

AuthManager::AuthManager(const std::string& primary_token) : auth_enabled_(false) {
    if (!primary_token.empty()) {
        valid_tokens_.insert(primary_token);
        auth_enabled_ = true;
    }
}

void AuthManager::addTokens(const std::vector<std::string>& tokens) {
    for (const auto& t : tokens) {
        if (!t.empty()) {
            valid_tokens_.insert(t);
            auth_enabled_ = true;
        }
    }
}

bool AuthManager::isAuthEnabled() const {
    return auth_enabled_;
}

bool AuthManager::validate(const std::string& token) const {
    if (!isAuthEnabled()) {
        return true;
    }
    
    // Check against all valid tokens using constant-time comparison
    for (const auto& valid_token : valid_tokens_) {
        if (secureCompare(token, valid_token)) {
            return true;
        }
    }
    return false;
}
