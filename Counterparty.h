#ifndef COUNTERPARTY_H
#define COUNTERPARTY_H

#include <atomic>
#include <string>
#include <vector>

// Notification sent to a counterparty each time one of its orders is filled
struct TradeNotification {
    long        orderId;           // the counterparty's order involved in the trade
    double      price;             // execution price
    long        quantity;          // fill quantity
    bool        wasBuy;            // true = this side was the buyer; false = seller
    std::string counterpartyName;  // name of the other party in the trade
};

class Counterparty {
private:
    long id;
    std::string name;
    std::vector<long> orderIds;
    std::vector<TradeNotification> trades;
    static std::atomic<long> nextId;

public:
    Counterparty(std::string name);

    long getId() const;
    const std::string& getName() const;
    const std::vector<long>& getOrderIds() const;
    const std::vector<TradeNotification>& getTrades() const;

    void addOrderId(long orderId);
    void removeOrderId(long orderId);
    void onTrade(TradeNotification notification);
};

#endif
