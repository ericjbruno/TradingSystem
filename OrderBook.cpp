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

void OrderBook::indexOrder(long orderId, OrderLocation loc) {
    orderIndex[orderId] = loc;
}

Counterparty* OrderBook::getOrderCounterparty(long orderId) const {
    auto it = orderIndex.find(orderId);
    if (it == orderIndex.end()) return nullptr;
    return it->second.it->getCounterparty();
}

bool OrderBook::cancel(long orderId) {
    auto indexIt = orderIndex.find(orderId);
    if (indexIt == orderIndex.end()) {
        return false;
    }

    OrderLocation& loc = indexIt->second;

    // Remove the order from its price-level list: O(1)
    auto& priceLevel = (*loc.priceMap)[loc.price];
    priceLevel.erase(loc.it);

    // Clean up the price level if it's now empty
    if (priceLevel.empty()) {
        loc.priceMap->erase(loc.price);
    }

    orderIndex.erase(indexIt);
    return true;
}

