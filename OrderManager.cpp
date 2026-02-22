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
    // Get the order book for this symbol
    SubBook& sb = orderBook->get(newOrder.getSymbol());

    // Insert into the correct map (BidMap or AskMap) and capture a type-erased
    // erase function so OrderLocation works with both map types.
    std::list<Order>*           priceListPtr;
    std::function<void(double)> eraseLevel;

    if (newOrder.isBuyOrder()) {
        BidMap* bids = &sb.getBuyOrdersRef();
        auto&   pl   = (*bids)[newOrder.getPrice()];
        pl.push_back(newOrder);
        priceListPtr = &pl;
        eraseLevel   = [bids](double p) { bids->erase(p); };
    } else {
        AskMap* asks = &sb.getSellOrdersRef();
        auto&   pl   = (*asks)[newOrder.getPrice()];
        pl.push_back(newOrder);
        priceListPtr = &pl;
        eraseLevel   = [asks](double p) { asks->erase(p); };
    }

    // Index the order for O(1) cancellation
    auto it = std::prev(priceListPtr->end());
    orderBook->indexOrder(newOrder.getId(),
                          {priceListPtr, newOrder.getPrice(), it, eraseLevel});

    // Register order with its counterparty
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
