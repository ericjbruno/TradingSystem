#include <list>
#include "Order.h"

#ifndef SUBBOOK_H
#define SUBBOOK_H

class SubBook
{
private:
    std::list<Order> buyOrders;
    std::list<Order> sellOrders;

public:
    SubBook();
    ~SubBook();

    std::list<Order> getBuyOrders() const { return buyOrders; }
    std::list<Order> getSellOrders() const { return sellOrders; }

    std::list<Order>& getBuyOrdersRef() { return buyOrders; }
    std::list<Order>& getSellOrdersRef() { return sellOrders; }

    std::list<Order> getBuyOrdersCopy()  { return buyOrders; }
    std::list<Order> getSellOrdersCopy()  { return sellOrders; }
};


#endif