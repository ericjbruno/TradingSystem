#include <list>
#include <map>
#include <string>
#include "Order.h"
#include "SubBook.h"

#ifndef ORDERBOOK_H
#define ORDERBOOK_H

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
    // Map of trading symbols to their respective order sub-books
    // Key: symbol (e.g., "AAPL", "GOOGL")
    // Value: SubBook containing buy and sell order lists for that symbol
    std::map<std::string, SubBook> books;

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
};
#endif
