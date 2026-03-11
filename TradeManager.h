#ifndef TRADEMANAGER_H
#define TRADEMANAGER_H

#include <deque>
#include <string>
#include "Counterparty.h"
#include "Order.h"

class EventBus;  // forward declaration — TradeManager holds a non-owning pointer

class SubBook;   // forward declarations — full types only needed in TradeManager.cpp
class OrderBook;

// Represents a single executed fill between a buyer and a seller
struct Trade {
    std::string   symbol;
    double        price;        // execution price (standing order's price)
    long          quantity;     // fill quantity
    long          buyOrderId;
    long          sellOrderId;
    Counterparty* buyer;        // non-owning pointer; may be nullptr
    Counterparty* seller;       // non-owning pointer; may be nullptr
};

class TradeManager
{
    EventBus*         eventBus_{nullptr};
    std::deque<Trade> recentTrades_;   // capped at 100; newest at back

public:
    TradeManager();
    ~TradeManager();

    void setEventBus(EventBus* bus) { eventBus_ = bus; }
    const std::deque<Trade>& getRecentTrades() const { return recentTrades_; }

    bool checkForTrade(const Order& order, double marketPrice);
    bool checkForSecuritiesTrade(const Order& order, double marketPrice);
    bool checkForSpotTrade(const Order& order, double marketPrice);

    // Returns true if a bid price crosses (or meets) an ask price
    static bool pricesMatch(double bidPrice, double askPrice);

    // Logs the fill to stdout and delivers a TradeNotification to each counterparty
    void logAndNotify(const Trade& trade);

    // Match an incoming SPOT order against the standing orders on the opposite side.
    // Fills are executed in-place: standing orders are modified or removed from the
    // book as they are consumed, and incoming.quantity is decremented for each fill.
    // Returns true if the incoming order was fully filled (caller should not queue it).
    bool matchSpotOrders(Order& incoming, SubBook& sb, OrderBook& book);
};

#endif
