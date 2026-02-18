#ifndef COUNTERPARTY_H
#define COUNTERPARTY_H

#include <atomic>
#include <string>
#include <vector>

class Counterparty {
private:
    long id;
    std::string name;
    std::vector<long> orderIds;
    static std::atomic<long> nextId;

public:
    Counterparty(std::string name);

    long getId() const;
    const std::string& getName() const;
    const std::vector<long>& getOrderIds() const;

    void addOrderId(long orderId);
    void removeOrderId(long orderId);
};

#endif
