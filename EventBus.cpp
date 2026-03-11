#include "EventBus.h"
#include <algorithm>

std::shared_ptr<EventBus::Connection> EventBus::subscribe() {
    auto conn = std::make_shared<Connection>();
    std::lock_guard<std::mutex> lk(mu_);
    conns_.push_back(conn);
    return conn;
}

void EventBus::unsubscribe(std::shared_ptr<Connection> conn) {
    {
        std::lock_guard<std::mutex> lk(conn->mu);
        conn->closed = true;
    }
    conn->cv.notify_all();
    std::lock_guard<std::mutex> lk(mu_);
    conns_.erase(std::remove(conns_.begin(), conns_.end(), conn), conns_.end());
}

void EventBus::publish(const std::string& sseMsg) {
    std::lock_guard<std::mutex> lk(mu_);
    for (auto& c : conns_) {
        std::lock_guard<std::mutex> clk(c->mu);
        c->queue.push(sseMsg);
        c->cv.notify_one();
    }
}
