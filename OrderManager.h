#include <memory>
#include <string>
#include "OrderBook.h"
#include "MarketManager.h"
#include "SubBook.h"

#ifndef ORDERMANAGER_H
#define ORDERMANAGER_H
class OrderManager
{
private:
    std::unique_ptr<OrderBook> orderBook;
    MarketManager* marketManager;

public:
    OrderManager(MarketManager*);
    ~OrderManager();

    void processNewOrder(const Order& order);
    void processCancelOrder(long orderId);
    void displayOrderBook();
    void displayOrderBook(std::string symbol);
    void displaySubBook(const PriceLevelMap& orders);

    SubBook& getSubBook(const std::string& symbol);
};

#endif