#include <vector>
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

std::vector<std::string> OrderBook::getSymbols() const {
    std::vector<std::string> result;
    result.reserve(books.size());
    for (const auto& kv : books)
        result.push_back(kv.first);
    return result;
}

void OrderBook::removeFromIndex(long orderId) {
    orderIndex.erase(orderId);
}

std::string OrderBook::getOrderSymbol(long orderId) const {
    auto it = orderIndex.find(orderId);
    if (it == orderIndex.end()) return "";
    return it->second.it->getSymbol();
}

bool OrderBook::cancel(long orderId) {
    auto indexIt = orderIndex.find(orderId);
    if (indexIt == orderIndex.end()) {
        return false;
    }

    OrderLocation& location = indexIt->second;

    // Erase just this order from its price-level list using its stored iterator.
    // Because the list may contain other orders at the same price, we remove
    // only the targeted node — O(1) and does not invalidate any other iterator
    // in the list, so concurrent OrderLocations at the same price remain valid.
    location.priceList->erase(location.it);

    // If this was the last order at this price level, remove the price level
    // entry from the map entirely so the book stays clean.
    // eraseLevel is a lambda that was captured at insert time and knows which
    // map (BidMap or AskMap) owns this price level.
    if (location.priceList->empty()) {
        location.eraseLevel(location.price);
    }

    orderIndex.erase(indexIt);
    return true;
}

