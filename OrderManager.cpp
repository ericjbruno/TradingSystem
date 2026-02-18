#include <iostream>
#include <iterator>
#include "MarketManager.h"
#include "OrderManager.h"
#include "OrderBook.h"
#include "SubBook.h"
#include "Order.h"

OrderManager::OrderManager(MarketManager* marketMgr) {
    orderBook = new OrderBook();
    marketManager = marketMgr;
}

OrderManager::~OrderManager() {

}

void OrderManager::processNewOrder(const Order& newOrder) {
    // Get the order book for this symbol
    SubBook& sb = orderBook->get( newOrder.getSymbol() );

    // Select buy or sell price-level map and insert at the order's price
    // map maintains sorted order automatically: O(log n)
    PriceLevelMap& orders = newOrder.isBuyOrder()
        ? sb.getBuyOrdersRef()
        : sb.getSellOrdersRef();

    auto& priceLevel = orders[newOrder.getPrice()];
    priceLevel.push_back(newOrder);

    // Index the order for O(1) cancellation
    auto it = std::prev(priceLevel.end());
    orderBook->indexOrder(newOrder.getId(), {&orders, newOrder.getPrice(), it});
}

void OrderManager::processCancelOrder(long orderId) {
    if (!orderBook->cancel(orderId)) {
        std::cerr << "Cancel failed: order " << orderId << " not found" << std::endl;
    }
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
