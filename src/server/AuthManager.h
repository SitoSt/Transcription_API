#pragma once
#include <string>
#include <unordered_set>
#include <vector>

class AuthManager {
public:
    explicit AuthManager(const std::string& primary_token);
    
    // Add multiple tokens (e.g. from environment or config file)
    void addTokens(const std::vector<std::string>& tokens);

    bool isAuthEnabled() const;
    bool validate(const std::string& token) const;

private:
    std::unordered_set<std::string> valid_tokens_;
    bool auth_enabled_;
};
