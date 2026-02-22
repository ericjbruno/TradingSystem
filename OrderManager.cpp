#include <functional>
#include <iostream>
#include <iterator>
#include <memory>
#include "Counterparty.h"
#include "MarketManager.h"
#include "OrderManager.h"
#include "OrderBook.h"
#include "SubBook.h"
#include "Order.h"

OrderManager::OrderManager(MarketManager* marketMgr) {
    orderBook    = std::make_unique<OrderBook>();
    tradeManager = std::make_unique<TradeManager>();
    marketManager = marketMgr;
}

OrderManager::~OrderManager() {

}

// ── Order processing ──────────────────────────────────────────────────────────

void OrderManager::processNewOrder(const Order& newOrder) {
    // Get (or lazily create) the SubBook for this trading symbol
    SubBook& sb = orderBook->get(newOrder.getSymbol());

    // For SPOT orders, attempt to match against the opposite side before queuing.
    // We work with a mutable copy so the matching engine can decrement the quantity.
    if (newOrder.getType() == OrderType::SPOT_BUY ||
        newOrder.getType() == OrderType::SPOT_SELL) {

        Order order = newOrder;   // mutable copy (same ID as the original)

        if (tradeManager->matchSpotOrders(order, sb, *orderBook)) {
            return;  // fully filled — nothing left to queue
        }

        // Partially filled: queue the unfilled remainder.
        queueOrder(order, sb);
        return;
    }

    // Non-SPOT orders go straight into the book with no matching
    queueOrder(newOrder, sb);
}

// ── Private helper: insert one order into the book and index it ───────────────
//
// BidMap (buys) and AskMap (sells) are different C++ types — BidMap uses
// std::greater so begin() always gives the best bid (highest price), while
// AskMap uses default ascending so begin() gives the best ask (lowest price).
//
// Because the two map types differ, OrderLocation cannot store a raw map
// pointer that covers both.  Instead we store two type-erased values:
//   priceListPtr — points to the std::list<Order> at this price level
//   eraseLevel   — a lambda that removes that price level from its map
//
// The lambda captures the map pointer by value at insert time, so cancel()
// can call eraseLevel(price) later without knowing which map type it owns.
//
// operator[] on the map returns the existing list if this price level already
// has orders, or inserts a new empty list if it doesn't — either way we get a
// reference to the list where this order belongs.
//
// Multiple orders at the same price all share the same list, held in arrival
// (FIFO) order.  std::list iterators are stable — cancelling a different order
// at the same price level never invalidates this iterator, so each order can
// be erased independently in O(1) without affecting others.

void OrderManager::queueOrder(const Order& order, SubBook& sb) {
    std::list<Order>*           priceListPtr;
    std::function<void(double)> eraseLevel;

    if (order.isBuyOrder()) {
        BidMap* bids = &sb.getBuyOrdersRef();
        auto&   pl   = (*bids)[order.getPrice()];
        pl.push_back(order);
        priceListPtr = &pl;
        eraseLevel   = [bids](double price) { bids->erase(price); };
    } else {
        AskMap* asks = &sb.getSellOrdersRef();
        auto&   pl   = (*asks)[order.getPrice()];
        pl.push_back(order);
        priceListPtr = &pl;
        eraseLevel   = [asks](double price) { asks->erase(price); };
    }

    auto it = std::prev(priceListPtr->end());
    orderBook->indexOrder(order.getId(),
                          {priceListPtr, order.getPrice(), it, eraseLevel});

    // Notify the counterparty that it now owns this order ID
    if (Counterparty* cp = order.getCounterparty())
        cp->addOrderId(order.getId());
}

void OrderManager::processCancelOrder(long orderId) {
    // Retrieve counterparty before the order is erased from the book
    Counterparty* cp = orderBook->getOrderCounterparty(orderId);

    if (!orderBook->cancel(orderId)) {
        std::cerr << "Cancel failed: order " << orderId << " not found" << std::endl;
        return;
    }

    if (cp) cp->removeOrderId(orderId);
}

SubBook& OrderManager::getSubBook(const std::string& symbol) {
    return orderBook->get(symbol);
}

std::vector<std::string> OrderManager::getSymbols() const {
    return orderBook->getSymbols();
}
