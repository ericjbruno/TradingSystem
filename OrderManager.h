#include <memory>
#include <string>
#include <vector>
#include "OrderBook.h"
#include "MarketManager.h"
#include "SubBook.h"
#include "TradeManager.h"

#ifndef ORDERMANAGER_H
#define ORDERMANAGER_H

class OrderManager
{
private:
    std::unique_ptr<OrderBook>    orderBook;
    std::unique_ptr<TradeManager> tradeManager;
    MarketManager*                marketManager;

    // Insert an order into the bid or ask map and register it in the order index.
    void queueOrder(const Order& order, SubBook& sb);

public:
    OrderManager(MarketManager*);
    ~OrderManager();

    void processNewOrder(const Order& order);
    void processCancelOrder(long orderId);

    SubBook& getSubBook(const std::string& symbol);
    std::vector<std::string> getSymbols() const;
};

#endif
