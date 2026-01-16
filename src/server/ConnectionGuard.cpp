#include "ConnectionGuard.h"
#include "ConnectionLimiter.h"

ConnectionGuard::ConnectionGuard(std::shared_ptr<ConnectionLimiter> limiter, std::string ip)
    : limiter_(std::move(limiter)), ip_(std::move(ip)) {}

ConnectionGuard::~ConnectionGuard() {
    if (limiter_) {
        limiter_->release(ip_);
    }
}
