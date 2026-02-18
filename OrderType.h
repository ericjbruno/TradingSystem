#ifndef ORDERTYPE_H
#define ORDERTYPE_H

enum class OrderType
{
    MARKET_BUY = 0,
    MARKET_SELL = 1,
    LIMIT_BUY = 2,
    LIMIT_SELL = 3,
    STOP_BUY = 4,
    STOP_SELL = 5,
    SPOT_BUY = 6,
    SPOT_SELL = 7,
    SWAP_BUY = 8,
    SWAP_SELL = 9,
    //inline bool operator==(int other) const { return *this == other; };
};

// Helper functions
inline bool isBuyOrder(OrderType type) {
    return static_cast<int>(type) % 2 == 0;
}

inline bool isSellOrder(OrderType type) {
    return !isBuyOrder(type);
}

inline const char* toString(OrderType type) {
    switch(type) {
        case OrderType::MARKET_BUY:  return "MARKET_BUY";
        case OrderType::MARKET_SELL: return "MARKET_SELL";
        case OrderType::LIMIT_BUY:   return "LIMIT_BUY";
        case OrderType::LIMIT_SELL:  return "LIMIT_SELL";
        case OrderType::STOP_BUY:    return "STOP_BUY";
        case OrderType::STOP_SELL:   return "STOP_SELL";
        case OrderType::SPOT_BUY:    return "SPOT_BUY";
        case OrderType::SPOT_SELL:   return "SPOT_SELL";
        case OrderType::SWAP_BUY:    return "SWAP_BUY";
        case OrderType::SWAP_SELL:   return "SWAP_SELL";
    }
    return "UNKNOWN";
}


#endif