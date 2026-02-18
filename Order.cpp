#include "Order.h"

Order::Order( std::string symbol,
              double price,
              int quantity,
              OrderType type ) {
    this->symbol = symbol;
    this->price = price;
    this->quantity = quantity;
    this->type = type;
    this->active = true;
}

Order::~Order()
{
}

std::string Order::getSymbol() const { return symbol; }
double Order::getPrice() const { return price; }
bool Order::isBuyOrder() const { return type == OrderType::MARKET_BUY; }
bool Order::isSellOrder() const { return type == OrderType::MARKET_SELL; }
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