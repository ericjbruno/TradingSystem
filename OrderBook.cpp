#include <iostream>
#include "OrderBook.h"

/**
 * Constructor - Initializes an empty order book
 * Currently just gets the initial size (which is 0)
 */
OrderBook::OrderBook(/* args */)
{
    int size = books.size();
}

/**
 * Destructor - Cleans up all order books
 * The map will automatically clean up all SubBooks
 */
OrderBook::~OrderBook()
{
}

/**
 * Retrieves or creates a SubBook for the given trading symbol
 *
 * This method uses lazy initialization - if a SubBook doesn't exist
 * for the given symbol, it will be automatically created by the map's
 * operator[] when accessed.
 *
 * @param symbol The trading symbol to look up (e.g., "AAPL", "GOOGL")
 * @return Reference to the SubBook for this symbol (existing or newly created)
 */
SubBook& OrderBook::get(const std::string& symbol) {
    // Check if this is the first time we're seeing this symbol
    if ( books.find(symbol) == books.end() ) {
        std::cout << "Creating order book for symbol: " << symbol << std::endl;
    }

    // Return reference to the SubBook (creates new one if it doesn't exist)
    // Using operator[] will default-construct a SubBook if the key is not found
    return books[symbol];
}

/**
 * Stores or updates a SubBook for a given trading symbol
 *
 * This method explicitly sets the SubBook for a symbol, replacing
 * any existing SubBook that may have been there.
 *
 * @param symbol The trading symbol
 * @param subBook The SubBook to store (will be copied into the map)
 */
void OrderBook::put(const std::string& symbol, const SubBook& subBook) {
    books[symbol] = subBook;
}

