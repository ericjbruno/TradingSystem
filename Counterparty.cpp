#include <algorithm>
#include <atomic>
#include "Counterparty.h"

std::atomic<long> Counterparty::nextId{1};

Counterparty::Counterparty(std::string name) : name(std::move(name)) {
    this->id = nextId.fetch_add(1, std::memory_order_relaxed);
}

long Counterparty::getId() const { return id; }

const std::string& Counterparty::getName() const { return name; }

const std::vector<long>& Counterparty::getOrderIds() const { return orderIds; }

void Counterparty::addOrderId(long orderId) {
    orderIds.push_back(orderId);
}

void Counterparty::removeOrderId(long orderId) {
    orderIds.erase(
        std::remove(orderIds.begin(), orderIds.end(), orderId),
        orderIds.end()
    );
}
