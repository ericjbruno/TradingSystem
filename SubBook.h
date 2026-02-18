#include <list>
#include <map>
#include "Order.h"

#ifndef SUBBOOK_H
#define SUBBOOK_H

// Price-level map: price -> list of orders at that price
// Buy orders: highest price first (best bid = rbegin())
// Sell orders: lowest price first (best ask = begin())
using PriceLevelMap = std::map<double, std::list<Order>>;

class SubBook
{
private:
    PriceLevelMap buyOrders;
    PriceLevelMap sellOrders;

public:
    SubBook();
    ~SubBook();

    const PriceLevelMap& getBuyOrders() const { return buyOrders; }
    const PriceLevelMap& getSellOrders() const { return sellOrders; }

    PriceLevelMap& getBuyOrdersRef() { return buyOrders; }
    PriceLevelMap& getSellOrdersRef() { return sellOrders; }
};


#endif