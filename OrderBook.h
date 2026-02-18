#include <list>
#include <string>
#include <unordered_map>
#include "Order.h"
#include "SubBook.h"

#ifndef ORDERBOOK_H
#define ORDERBOOK_H

// Locates a specific order within a price-level map for O(1) cancellation
struct OrderLocation {
    PriceLevelMap* priceMap;           // pointer to buy or sell map in SubBook
    double price;                       // price level key in the map
    std::list<Order>::iterator it;      // iterator to this order in the list
};

/**
 * OrderBook - Manages order books for multiple trading symbols
 *
 * This class maintains a collection of SubBooks (buy/sell order lists)
 * organized by trading symbol. Each symbol gets its own SubBook containing
 * separate lists for buy and sell orders.
 */
class OrderBook
{
private:
    std::unordered_map<std::string, SubBook> books;  // O(1) symbol lookup
    std::unordered_map<long, OrderLocation> orderIndex;  // O(1) lookup by order ID

public:
    /**
     * Constructor - Initializes an empty order book
     */
    OrderBook(/* args */);

    /**
     * Destructor - Cleans up order book resources
     */
    ~OrderBook();

    /**
     * Retrieves the SubBook for a given trading symbol
     * Creates a new SubBook if one doesn't exist for the symbol
     *
     * @param symbol The trading symbol (e.g., "AAPL")
     * @return Reference to the SubBook for this symbol
     */
    SubBook& get(const std::string& symbol);

    /**
     * Stores or updates a SubBook for a given trading symbol
     *
     * @param symbol The trading symbol
     * @param subBook The SubBook to associate with this symbol
     */
    void put(const std::string& symbol, const SubBook& subBook);

    // Index an order for O(1) cancellation lookup
    void indexOrder(long orderId, OrderLocation loc);

    // Cancel an order by ID: removes from price-level list and index
    // Returns true if found and cancelled, false if ID not found
    bool cancel(long orderId);
};
#endif
