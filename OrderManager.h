#include "OrderBook.h"
#include "MarketManager.h"
#include <string>
#include <list>

#ifndef ORDERMANAGER_H
#define ORDERMANAGER_H
class OrderManager
{
private:
    OrderBook* orderBook;
    MarketManager* marketManager;

public:
    OrderManager(MarketManager*);
    ~OrderManager();

    void processNewOrder(const Order& order);
    void processCancelOrder(long orderId);
    void displayOrderBook();
    void displayOrderBook(std::string symbol);
    void displaySubBook(std::list<Order> orders);
};

#endif