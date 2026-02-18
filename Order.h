#include <string>
#include "OrderType.h"

#ifndef ORDER_H
#define ORDER_H

class Order
{
private:
    long id;
    bool active;
    double price;
    long quantity;
    std::string symbol;
    OrderType type;

    static long nextId;

public:
    Order( std::string symbol,
           double price,
           int quantity,
           OrderType type);
    ~Order();
    long getId() const;
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