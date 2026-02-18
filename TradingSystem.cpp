#include <memory>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include "Counterparty.h"
#include "OrderManager.h"
#include "MarketManager.h"

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

    return 0;
}

