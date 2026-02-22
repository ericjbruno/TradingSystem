#include <functional>
#include <list>
#include <map>
#include "Order.h"

#ifndef SUBBOOK_H
#define SUBBOOK_H

// Sell (ask) map: ascending — begin() == best ask (lowest price)
using AskMap = std::map<double, std::list<Order>>;

// Buy (bid) map: descending — begin() == best bid (highest price)
using BidMap = std::map<double, std::list<Order>, std::greater<double>>;

class SubBook
{
private:
    BidMap buyOrders;
    AskMap sellOrders;

public:
    SubBook();
    ~SubBook();

    const BidMap& getBuyOrders()  const { return buyOrders; }
    const AskMap& getSellOrders() const { return sellOrders; }

    BidMap& getBuyOrdersRef()  { return buyOrders; }
    AskMap& getSellOrdersRef() { return sellOrders; }
};


#endif
