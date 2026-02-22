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
    orderBook = std::make_unique<OrderBook>();
    marketManager = marketMgr;
}

OrderManager::~OrderManager() {

}

void OrderManager::processNewOrder(const Order& newOrder) {
    // Get (or lazily create) the SubBook for this trading symbol
    SubBook& sb = orderBook->get(newOrder.getSymbol());

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
    std::list<Order>*           priceListPtr;
    std::function<void(double)> eraseLevel;

    if (newOrder.isBuyOrder()) {
        BidMap* bids = &sb.getBuyOrdersRef();

        // operator[] returns the existing list if this price level already has
        // orders, or inserts a new empty list if it doesn't — either way pl is
        // a reference to the list where this order belongs.
        auto& pl = (*bids)[newOrder.getPrice()];

        // Multiple orders at the same price all share the same list, held in
        // arrival (FIFO) order.
        pl.push_back(newOrder);
        priceListPtr = &pl;
        
        // Capture bids by value (pointer copy) so the lambda stays valid after
        // this function returns
        eraseLevel   = [bids](double price) { bids->erase(price); };
    } else {
        AskMap* asks = &sb.getSellOrdersRef();
        auto& pl = (*asks)[newOrder.getPrice()];
        pl.push_back(newOrder);
        priceListPtr = &pl;
        eraseLevel   = [asks](double price) { asks->erase(price); };
    }

    // Store an iterator to the newly inserted order (the last element in the
    // list).  std::list iterators are stable — cancelling a different order at
    // the same price level never invalidates this iterator, so each order in
    // the list can be erased independently in O(1) without affecting others.
    auto it = std::prev(priceListPtr->end());
    orderBook->indexOrder(newOrder.getId(),
                          {priceListPtr, newOrder.getPrice(), it, eraseLevel});

    // Notify the counterparty that it now owns this order ID
    if (Counterparty* cp = newOrder.getCounterparty())
        cp->addOrderId(newOrder.getId());
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

/*
// Display the orders within a sub book
void OrderManager::displaySubBook(std::list<OrderEntry> orders) {
    for ( auto order: orders.begin() ) {
        std::cout << ("Price: " + order.getPrice()
                + ", Qty: " + order.getQuantity()
                + ", Type: "
                + OrderType.getAsString(order.getType()));
    }
}

void OrderManager::displayOrderBook() {
    Set<StringBuffer> keys = orderBook.keySet();
    orderBookKeys = new StringBuffer[orderBook.size()];
    keys.toArray(orderBookKeys);

    for (int i = 0; i < orderBook.size(); i++) {
        displayOrderBook(orderBookKeys[i]);
    }
}

// Display the order book for a symbol
void OrderManager::displayOrderBook(std::string symbol) {
    // Get the order book for this symbol if it exists
    SubBook sb = orderBook.get(symbol);
    if (sb != null) {
        System.out.println("******************************************");
        System.out.println("Buy orders for " + symbol);
        displaySubBook(sb.buy);
        System.out.println("Sell orders for " + symbol);
        displaySubBook(sb.sell);
        System.out.println(" ");
    }
}
*/
