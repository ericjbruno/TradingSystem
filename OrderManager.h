#include <memory>
#include <string>
#include <vector>
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

    SubBook& getSubBook(const std::string& symbol);
    std::vector<std::string> getSymbols() const;
};

#endif