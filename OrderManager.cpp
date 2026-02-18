#include <list>
#include <iostream>
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
    std::string symbol = newOrder.getSymbol();
    SubBook& sb = orderBook->get( symbol );

    // Get the right sub book (buy or sell orders) - using reference to modify in place
    std::list<Order>& orders = newOrder.isBuyOrder()
        ? sb.getBuyOrdersRef()
        : sb.getSellOrdersRef();

    // Walk the sub book and find the correct location for this order
    auto it = orders.begin();
    while ( it != orders.end() ) {
        Order entry = *it;
        if (newOrder.comesBefore(entry.getPrice())) {
            orders.insert(it, newOrder);
            return;
        }
        ++it;
    }

    // Just add the order to the end of the list
    orders.push_back(newOrder);
}

void OrderManager::processCancelOrder(long orderId) {
    // Logic to cancel an existing order
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
