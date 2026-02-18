#pragma once
#include <string>

class MarketPrice {
private:
    double price;
    long quantity;
    std::string timestamp;
    std::string symbol;

public:
    MarketPrice() = default;
    MarketPrice(std::string s, double p, long q, std::string t);

    ~MarketPrice() = default;

    double getPrice() const;
    void setPrice(double p);

    long getQuantity();
    void setQuantity(long q);

    std::string getTimestamp();
    void setTimestamp(std::string t);

    std::string getSymbol();
    void setSymbol(std::string s);
};
