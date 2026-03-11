#include <deque>
#include <memory>
#include <string>
#include <vector>
#include "OrderBook.h"
#include "MarketManager.h"
#include "SubBook.h"
#include "TradeManager.h"

#ifndef ORDERMANAGER_H
#define ORDERMANAGER_H

class EventBus;  // forward declaration

class OrderManager
{
private:
    std::unique_ptr<OrderBook>    orderBook;
    std::unique_ptr<TradeManager> tradeManager;
    MarketManager*                marketManager;
    EventBus*                     eventBus_{nullptr};

    void queueOrder(const Order& order, SubBook& sb);
    void publishBookUpdate(const std::string& symbol);

public:
    OrderManager(MarketManager*);
    ~OrderManager();

    void setEventBus(EventBus* bus);

    void processNewOrder(const Order& order);
    void processCancelOrder(long orderId);

    SubBook& getSubBook(const std::string& symbol);
    std::vector<std::string> getSymbols() const;
    const std::deque<Trade>& getRecentTrades() const;
};

#endif
