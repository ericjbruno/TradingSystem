#ifndef TRADEMANAGER_H
#define TRADEMANAGER_H

#include "Order.h"

class TradeManager
{
public:
    TradeManager();
    ~TradeManager();

    bool checkForTrade(const Order& order, double marketPrice);
    bool checkForSecuritiesTrade(const Order& order, double marketPrice);
    bool checkForSpotTrade(const Order& order, double marketPrice);
};

#endif
