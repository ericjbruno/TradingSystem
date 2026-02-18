#include <atomic>
#include <string>
#include "OrderType.h"

#ifndef ORDER_H
#define ORDER_H

class Counterparty;  // forward declaration — Order stores a non-owning pointer

class Order
{
private:
    long id;
    bool active;
    double price;
    long quantity;
    std::string symbol;
    OrderType type;
    Counterparty* counterparty;

    static std::atomic<long> nextId;

public:
    Order( std::string symbol,
           double price,
           int quantity,
           OrderType type,
           Counterparty* counterparty);
    ~Order();
    long getId() const;
    Counterparty* getCounterparty() const;
    const std::string& getSymbol() const;
    double getPrice() const;
    OrderType getType() const;
    long getQuantity() const;
    void setActive(bool active);
    bool isLimitOrder() const;
    bool isActive() const;
    bool isBuyOrder() const;
    bool isSellOrder() const;
    bool comesBefore(double otherPrice) const;
};

#endif