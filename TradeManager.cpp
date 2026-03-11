#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>
#include "Counterparty.h"
#include "EventBus.h"
#include "OrderBook.h"
#include "SubBook.h"
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

// A trade occurs when the buyer's price is at or above the seller's price
bool TradeManager::pricesMatch(double bidPrice, double askPrice) {
    return bidPrice >= askPrice;
}

// Logs the fill to stdout and delivers a TradeNotification to each counterparty
void TradeManager::logAndNotify(const Trade& trade) {
    std::cout << std::fixed << std::setprecision(4)
              << "[TRADE] " << trade.symbol
              << "  qty=" << trade.quantity
              << "  @ "   << trade.price
              << "  | Buy#"  << trade.buyOrderId
              << " ("  << (trade.buyer  ? trade.buyer->getName()  : "?") << ")"
              << "  Sell#" << trade.sellOrderId
              << " ("  << (trade.seller ? trade.seller->getName() : "?") << ")"
              << "\n";

    // Store in ring buffer (newest at back, capped at 100)
    recentTrades_.push_back(trade);
    if (recentTrades_.size() > 100) recentTrades_.pop_front();

    // Publish SSE event to all connected UI clients
    if (eventBus_) {
        std::ostringstream j;
        j << std::fixed << std::setprecision(6);
        j << "event: trade\ndata: "
          << "{\"symbol\":\""    << trade.symbol   << "\""
          << ",\"price\":"       << trade.price
          << ",\"quantity\":"    << trade.quantity
          << ",\"buyOrderId\":"  << trade.buyOrderId
          << ",\"sellOrderId\":" << trade.sellOrderId
          << ",\"buyer\":\""     << (trade.buyer  ? trade.buyer->getName()  : "") << "\""
          << ",\"seller\":\""    << (trade.seller ? trade.seller->getName() : "") << "\""
          << "}\n\n";
        eventBus_->publish(j.str());
    }

    if (trade.buyer) {
        trade.buyer->onTrade({
            trade.buyOrderId,
            trade.price,
            trade.quantity,
            true,   // this side was the buyer
            trade.seller ? trade.seller->getName() : "unknown"
        });
    }

    if (trade.seller) {
        trade.seller->onTrade({
            trade.sellOrderId,
            trade.price,
            trade.quantity,
            false,  // this side was the seller
            trade.buyer ? trade.buyer->getName() : "unknown"
        });
    }
}

// ── Matching engine ───────────────────────────────────────────────────────────
//
// matchSpotOrders is called before a SPOT order is queued in the book.
// It walks the opposite side of the book from the best price inward, executing
// fills for as long as prices cross and quantity remains.
//
// For each fill:
//   • A Trade record is created, logged, and counterparties are notified.
//   • The incoming order's quantity is decremented.
//   • If the standing order is fully consumed it is erased from the price-level
//     list, removed from the order index, and de-registered from its counterparty.
//   • If only partially consumed, its stored quantity is reduced in-place via
//     setQuantity(); its position in the list and its index entry remain valid.
//   • If a price level becomes empty after fills, the map entry is erased.
//
// std::list iterators remain valid across erasures of other nodes, so advancing
// the iterator before erasing is safe and keeps the loop correct.
//
// Returns true if the incoming order was fully filled (nothing left to queue).

bool TradeManager::matchSpotOrders(Order& incoming, SubBook& sb, OrderBook& book) {

    if (incoming.isBuyOrder()) {
        // ── Incoming BUY: match against standing asks (lowest price first) ────
        AskMap& asks = sb.getSellOrdersRef();
        auto mapIt   = asks.begin();

        while (mapIt != asks.end() && incoming.getQuantity() > 0) {
            double askPrice = mapIt->first;

            // Stop as soon as the best available ask exceeds the bid's limit
            if (!pricesMatch(incoming.getPrice(), askPrice)) break;

            std::list<Order>& level   = mapIt->second;
            auto              orderIt = level.begin();

            while (orderIt != level.end() && incoming.getQuantity() > 0) {
                Order& standing = *orderIt;
                long   fillQty  = std::min(incoming.getQuantity(), standing.getQuantity());

                // Execution is at the standing order's (ask) price
                Trade trade{
                    incoming.getSymbol(),
                    askPrice,
                    fillQty,
                    incoming.getId(),           // buyOrderId
                    standing.getId(),           // sellOrderId
                    incoming.getCounterparty(), // buyer
                    standing.getCounterparty()  // seller
                };
                logAndNotify(trade);

                incoming.setQuantity(incoming.getQuantity() - fillQty);

                if (fillQty == standing.getQuantity()) {
                    // Standing ask fully consumed: erase from list, index, and counterparty
                    long          standingId = standing.getId();
                    Counterparty* cp         = standing.getCounterparty();
                    orderIt = level.erase(orderIt);   // advance before erase
                    book.removeFromIndex(standingId);
                    if (cp) cp->removeOrderId(standingId);
                } else {
                    // Standing ask partially consumed: reduce its remaining quantity
                    standing.setQuantity(standing.getQuantity() - fillQty);
                    ++orderIt;
                }
            }

            // Advance map iterator before potentially erasing the current level
            auto nextMapIt = std::next(mapIt);
            if (level.empty()) asks.erase(mapIt);
            mapIt = nextMapIt;
        }

    } else {
        // ── Incoming SELL: match against standing bids (highest price first) ──
        BidMap& bids = sb.getBuyOrdersRef();
        auto    mapIt = bids.begin();

        while (mapIt != bids.end() && incoming.getQuantity() > 0) {
            double bidPrice = mapIt->first;

            // Stop as soon as the best bid falls below the ask's limit
            if (!pricesMatch(bidPrice, incoming.getPrice())) break;

            std::list<Order>& level   = mapIt->second;
            auto              orderIt = level.begin();

            while (orderIt != level.end() && incoming.getQuantity() > 0) {
                Order& standing = *orderIt;
                long   fillQty  = std::min(incoming.getQuantity(), standing.getQuantity());

                // Execution is at the standing order's (bid) price
                Trade trade{
                    incoming.getSymbol(),
                    bidPrice,
                    fillQty,
                    standing.getId(),           // buyOrderId  (standing bid)
                    incoming.getId(),           // sellOrderId (incoming ask)
                    standing.getCounterparty(), // buyer
                    incoming.getCounterparty()  // seller
                };
                logAndNotify(trade);

                incoming.setQuantity(incoming.getQuantity() - fillQty);

                if (fillQty == standing.getQuantity()) {
                    // Standing bid fully consumed
                    long          standingId = standing.getId();
                    Counterparty* cp         = standing.getCounterparty();
                    orderIt = level.erase(orderIt);
                    book.removeFromIndex(standingId);
                    if (cp) cp->removeOrderId(standingId);
                } else {
                    // Standing bid partially consumed
                    standing.setQuantity(standing.getQuantity() - fillQty);
                    ++orderIt;
                }
            }

            auto nextMapIt = std::next(mapIt);
            if (level.empty()) bids.erase(mapIt);
            mapIt = nextMapIt;
        }
    }

    return incoming.getQuantity() == 0;
}
