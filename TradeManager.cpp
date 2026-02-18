#include "TradeManager.h"
#include "Order.h"

TradeManager::TradeManager() {
}

TradeManager::~TradeManager() {
}

bool TradeManager::checkForTrade(const Order& order, double marketPrice) {
    if (order.getType() == OrderType::SPOT_BUY || order.getType() == OrderType::SPOT_SELL) {
        return checkForSpotTrade(order, marketPrice);
    }
    else {
        return checkForSecuritiesTrade(order, marketPrice);
    }
}

bool TradeManager::checkForSecuritiesTrade(const Order& order, double marketPrice) {
    if (!order.isActive()) {
        return false;
    }

    if (order.isBuyOrder()) {
        return order.getPrice() >= marketPrice;
    } else if (order.isSellOrder()) {
        return order.getPrice() <= marketPrice;
    }
    return false;
}

bool TradeManager::checkForSpotTrade(const Order& order, double marketPrice) {
    if (!order.isActive()) {
        return false;
    }

    if (order.isBuyOrder()) {
        return order.getPrice() >= marketPrice;
    } else if (order.isSellOrder()) {
        return order.getPrice() <= marketPrice;
    }
    return false;
}
