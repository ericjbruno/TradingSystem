#include <algorithm>
#include <iomanip>
#include <memory>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>
#include "Counterparty.h"
#include "OrderManager.h"
#include "MarketManager.h"
#include "SubBook.h"

// ── Order book display helpers ──────────────────────────────────────────────

// Format a long quantity with comma separators (e.g. 19970 → "19,970")
static std::string fmtQty(long qty) {
    std::string s = std::to_string(qty);
    for (int i = static_cast<int>(s.size()) - 3; i > 0; i -= 3)
        s.insert(static_cast<std::string::size_type>(i), ",");
    return s;
}

// Build a bracketed order-ID list (e.g. "[#1 #26 #51]")
static std::string buildIds(const std::list<Order>& orders) {
    std::string result = "[";
    bool first = true;
    for (const auto& o : orders) {
        if (!first) result += " ";
        result += "#" + std::to_string(o.getId());
        first = false;
    }
    result += "]";
    return result;
}

// Print a complete ASCII snapshot of every symbol in the order book.
// Sell (ask) side is shown above the spread line, highest price first so the
// best ask sits closest to the spread.  Buy (bid) side is shown below with
// the best bid at the top — also closest to the spread.
void printOrderBook(OrderManager& om) {
    const int         W   = 64;
    const std::string SEP = std::string(W, '=');
    const std::string DOT = std::string(W, '.');

    std::vector<std::string> symbols = om.getSymbols();
    std::sort(symbols.begin(), symbols.end());

    std::cout << "\n" << SEP << "\n";
    std::cout << std::string((W - 10) / 2, ' ') << "ORDER BOOK\n";
    std::cout << SEP << "\n";

    for (const auto& sym : symbols) {
        SubBook& sb   = om.getSubBook(sym);
        auto&    bids = sb.getBuyOrdersRef();
        auto&    asks = sb.getSellOrdersRef();

        if (bids.empty() && asks.empty()) continue;

        std::cout << "\n  " << sym << "\n";

        // ── Sell (ask) side: highest price first, best ask at bottom ────────
        std::cout << "  SELL (Ask)\n";
        if (asks.empty()) {
            std::cout << "    (none)\n";
        } else {
            for (auto it = asks.rbegin(); it != asks.rend(); ++it) {
                bool isBest = (std::next(it) == asks.rend());
                long total  = 0;
                for (const auto& o : it->second) total += o.getQuantity();

                std::cout << std::fixed << std::setprecision(4)
                          << "    " << std::setw(10) << std::right << it->first
                          << "    " << std::setw(12) << std::right << fmtQty(total)
                          << "    " << buildIds(it->second);
                if (isBest) std::cout << "  <- best ask";
                std::cout << "\n";
            }
        }

        // ── Spread line ──────────────────────────────────────────────────────
        std::cout << "  " << DOT << "\n";

        // ── Buy (bid) side: highest price (best bid) first ──────────────────
        std::cout << "  BUY  (Bid)\n";
        if (bids.empty()) {
            std::cout << "    (none)\n";
        } else {
            for (auto it = bids.rbegin(); it != bids.rend(); ++it) {
                bool isBest = (it == bids.rbegin());
                long total  = 0;
                for (const auto& o : it->second) total += o.getQuantity();

                std::cout << std::fixed << std::setprecision(4)
                          << "    " << std::setw(10) << std::right << it->first
                          << "    " << std::setw(12) << std::right << fmtQty(total)
                          << "    " << buildIds(it->second);
                if (isBest) std::cout << "  <- best bid";
                std::cout << "\n";
            }
        }

        std::cout << "  " << SEP << "\n";
    }
    std::cout << "\n";
}

int main() {
    // Create the Market and Trade Managers
    auto marketManager = std::make_unique<MarketManager>();
    auto orderManager  = std::make_unique<OrderManager>(marketManager.get());

    // Sample counterparties — orders are assigned round-robin
    Counterparty counterparties[] = {
        Counterparty("Goldman Sachs"),
        Counterparty("JP Morgan"),
        Counterparty("Deutsche Bank")
    };
    const int cpCount = 3;

    // Open the CSV file
    std::ifstream file("forex_orders.csv");
    if (!file.is_open()) {
        std::cerr << "Error: Could not open forex_orders.csv" << std::endl;
        return 1;
    }

    std::string line;
    // Skip the header line
    std::getline(file, line);

    int orderCount = 0;

    // Read each line from the CSV
    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string symbol, priceStr, quantityStr, side;

        // Parse CSV fields (Symbol,Price,Quantity,Side)
        std::getline(ss, symbol, ',');
        std::getline(ss, priceStr, ',');
        std::getline(ss, quantityStr, ',');
        std::getline(ss, side, ',');

        // Convert strings to appropriate types
        double price = std::stod(priceStr);
        int quantity = std::stoi(quantityStr);

        // Determine order type based on side
        OrderType orderType;
        if (side == "BUY") {
            orderType = OrderType::SPOT_BUY;
        } else {
            orderType = OrderType::SPOT_SELL;
        }

        // Assign counterparty round-robin and create order
        Counterparty* cp = &counterparties[orderCount % cpCount];
        Order order(symbol, price, quantity, orderType, cp);
        orderManager->processNewOrder(order);

        orderCount++;
        std::cout << "Processed order #" << orderCount << ": "
                  << side << " " << quantity << " " << symbol
                  << " @ " << price
                  << "  [" << cp->getName() << "]" << std::endl;
    }

    file.close();

    std::cout << "\nTotal orders processed: " << orderCount << std::endl;

    printOrderBook(*orderManager);

    return 0;
}

