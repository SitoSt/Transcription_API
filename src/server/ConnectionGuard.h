#pragma once
#include <memory>
#include <string>

class ConnectionLimiter;

class ConnectionGuard {
public:
    ConnectionGuard(std::shared_ptr<ConnectionLimiter> limiter, std::string ip);
    ~ConnectionGuard();

    // Disable copying
    ConnectionGuard(const ConnectionGuard&) = delete;
    ConnectionGuard& operator=(const ConnectionGuard&) = delete;

    // Enable moving
    ConnectionGuard(ConnectionGuard&&) = default;
    ConnectionGuard& operator=(ConnectionGuard&&) = default;

private:
    std::shared_ptr<ConnectionLimiter> limiter_;
    std::string ip_;
};
