#include "MarketPrice.h"

MarketPrice::MarketPrice(std::string s, double p, long q, std::string t) {
    price = p;
    quantity = q;
    timestamp = std::string(t);
    symbol = std::string(s);
}

double MarketPrice::getPrice() const {
    return price;
}   
void MarketPrice::setPrice(double p) {
    price = p;
}
long MarketPrice::getQuantity() {
    return quantity;
}
void MarketPrice::setQuantity(long q) {
    this->quantity = q;
}
std::string MarketPrice::getTimestamp() {
    return timestamp;
}
std::string MarketPrice::getSymbol() {
    return symbol;
}
void MarketPrice::setTimestamp(std::string t) {
    this->timestamp = t;
}
void MarketPrice::setSymbol(std::string s) {
    this->symbol = s;
}   
