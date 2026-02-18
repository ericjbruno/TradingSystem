#include <atomic>
#include "Order.h"

std::atomic<long> Order::nextId{1};

Order::Order( std::string symbol,
              double price,
              int quantity,
              OrderType type,
              Counterparty* counterparty ) {
    this->id = nextId.fetch_add(1, std::memory_order_relaxed);
    this->symbol = symbol;
    this->price = price;
    this->quantity = quantity;
    this->type = type;
    this->active = true;
    this->counterparty = counterparty;
}

long Order::getId() const { return id; }

Counterparty* Order::getCounterparty() const { return counterparty; }

Order::~Order()
{
}

const std::string& Order::getSymbol() const { return symbol; }
double Order::getPrice() const { return price; }
bool Order::isBuyOrder() const { return ::isBuyOrder(type); }
bool Order::isSellOrder() const { return ::isSellOrder(type); }
bool Order::comesBefore(double otherPrice) const {
    if (isBuyOrder()) {
        return price > otherPrice; // Higher price first for buys
    } else {
        return price < otherPrice; // Lower price first for sells
    }
}

OrderType Order::getType() const {
    return type;
}

long Order::getQuantity() const {
    return quantity;
}

void Order::setActive(bool active) {
    this->active = active;
}

bool Order::isLimitOrder() const {
    return type == OrderType::MARKET_BUY || type == OrderType::MARKET_SELL;
}

bool Order::isActive() const {
    return active;
}    