#include <iostream>
#include <string>
#include "Counterparty.h"
#include "OrderManager.h"
#include "MarketManager.h"
#include "Order.h"
#include "OrderType.h"
#include "SubBook.h"

// ─── Minimal test framework ───────────────────────────────────────────────────

static int passed = 0, failed = 0;

void check(const std::string& name, bool result) {
    if (result) {
        std::cout << "  [PASS] " << name << "\n";
        ++passed;
    } else {
        std::cout << "  [FAIL] " << name << "\n";
        ++failed;
    }
}

void section(const std::string& name) {
    std::cout << "\n" << name << "\n" << std::string(name.size(), '-') << "\n";
}

// ─── Tests ────────────────────────────────────────────────────────────────────

int main() {
    MarketManager mm;
    OrderManager om(&mm);

    // Shared counterparty for routing/priority/cancellation tests
    Counterparty cp("TestTrader");

    // ── 1. Order routing ──────────────────────────────────────────────────────
    section("Order Routing");

    // All five buy types must land in buyOrders, not sellOrders
    {
        OrderType buyTypes[] = {
            OrderType::MARKET_BUY, OrderType::LIMIT_BUY, OrderType::STOP_BUY,
            OrderType::SPOT_BUY,   OrderType::SWAP_BUY
        };
        const char* names[] = { "MARKET_BUY", "LIMIT_BUY", "STOP_BUY", "SPOT_BUY", "SWAP_BUY" };

        for (int i = 0; i < 5; i++) {
            std::string sym = std::string("BUY.") + names[i];
            Order o(sym, 1.0000, 100, buyTypes[i], &cp);
            om.processNewOrder(o);
            SubBook& sb = om.getSubBook(sym);
            check(std::string(names[i]) + " routes to buyOrders",  !sb.getBuyOrdersRef().empty());
            check(std::string(names[i]) + " not in sellOrders",     sb.getSellOrdersRef().empty());
        }
    }

    // All five sell types must land in sellOrders, not buyOrders
    {
        OrderType sellTypes[] = {
            OrderType::MARKET_SELL, OrderType::LIMIT_SELL, OrderType::STOP_SELL,
            OrderType::SPOT_SELL,   OrderType::SWAP_SELL
        };
        const char* names[] = { "MARKET_SELL", "LIMIT_SELL", "STOP_SELL", "SPOT_SELL", "SWAP_SELL" };

        for (int i = 0; i < 5; i++) {
            std::string sym = std::string("SELL.") + names[i];
            Order o(sym, 1.0000, 100, sellTypes[i], &cp);
            om.processNewOrder(o);
            SubBook& sb = om.getSubBook(sym);
            check(std::string(names[i]) + " routes to sellOrders",  !sb.getSellOrdersRef().empty());
            check(std::string(names[i]) + " not in buyOrders",       sb.getBuyOrdersRef().empty());
        }
    }

    // ── 2. Buy-side price priority ────────────────────────────────────────────
    section("Buy-Side Price Priority  (best bid = highest price)");

    {
        // Insert in deliberately non-sorted order
        om.processNewOrder(Order("EUR/USD", 1.0842, 100, OrderType::SPOT_BUY, &cp));
        om.processNewOrder(Order("EUR/USD", 1.0835, 200, OrderType::SPOT_BUY, &cp));
        om.processNewOrder(Order("EUR/USD", 1.0856, 150, OrderType::SPOT_BUY, &cp));

        SubBook& sb  = om.getSubBook("EUR/USD");
        auto&    bids = sb.getBuyOrdersRef();

        check("3 price levels in buy book",          bids.size() == 3);
        check("Best bid (rbegin) is 1.0856",         bids.rbegin()->first == 1.0856);
        check("Lowest buy level (begin) is 1.0835",  bids.begin()->first  == 1.0835);
    }

    // ── 3. Sell-side price priority ───────────────────────────────────────────
    section("Sell-Side Price Priority  (best ask = lowest price)");

    {
        // Insert in deliberately non-sorted order
        om.processNewOrder(Order("GBP/USD", 1.2645, 100, OrderType::SPOT_SELL, &cp));
        om.processNewOrder(Order("GBP/USD", 1.2621, 200, OrderType::SPOT_SELL, &cp));
        om.processNewOrder(Order("GBP/USD", 1.2634, 150, OrderType::SPOT_SELL, &cp));

        SubBook& sb   = om.getSubBook("GBP/USD");
        auto&    asks  = sb.getSellOrdersRef();

        check("3 price levels in sell book",          asks.size() == 3);
        check("Best ask (begin) is 1.2621",           asks.begin()->first  == 1.2621);
        check("Highest ask level (rbegin) is 1.2645", asks.rbegin()->first == 1.2645);
    }

    // ── 4. Time priority (FIFO) within a price level ─────────────────────────
    section("Time Priority within Price Level  (FIFO)");

    {
        Order first ("USD/JPY", 149.23, 1000, OrderType::SPOT_BUY, &cp);
        Order second("USD/JPY", 149.23, 2000, OrderType::SPOT_BUY, &cp);
        long firstId  = first.getId();
        long secondId = second.getId();

        om.processNewOrder(first);
        om.processNewOrder(second);

        SubBook& sb    = om.getSubBook("USD/JPY");
        auto&    level = sb.getBuyOrdersRef().at(149.23);

        check("2 orders at same price level",         level.size() == 2);
        check("First inserted is at front of list",   level.front().getId() == firstId);
        check("Second inserted is at back of list",   level.back().getId()  == secondId);
    }

    // Multiple price levels all in correct FIFO order
    {
        Order a("AUD/USD", 0.6321, 500,  OrderType::SPOT_BUY, &cp);
        Order b("AUD/USD", 0.6321, 1500, OrderType::SPOT_BUY, &cp);
        Order c("AUD/USD", 0.6321, 750,  OrderType::SPOT_BUY, &cp);
        long idA = a.getId(), idB = b.getId(), idC = c.getId();

        om.processNewOrder(a);
        om.processNewOrder(b);
        om.processNewOrder(c);

        auto&  level = om.getSubBook("AUD/USD").getBuyOrdersRef().at(0.6321);
        auto   it    = level.begin();

        check("3 orders at same price level",    level.size() == 3);
        check("Position 1 is order A",           (it++)->getId() == idA);
        check("Position 2 is order B",           (it++)->getId() == idB);
        check("Position 3 is order C",           it->getId()     == idC);
    }

    // ── 5. Order cancellation ─────────────────────────────────────────────────
    section("Order Cancellation");

    // Cancel the only order at a level — price level should be removed
    {
        Order o("NZD/USD", 0.5789, 300, OrderType::SPOT_BUY, &cp);
        long id = o.getId();

        om.processNewOrder(o);
        SubBook& sb = om.getSubBook("NZD/USD");
        check("Price level exists before cancel",       sb.getBuyOrdersRef().count(0.5789) == 1);

        om.processCancelOrder(id);
        check("Price level removed after last cancel",  sb.getBuyOrdersRef().count(0.5789) == 0);
    }

    // Cancel one of two orders at a level — level must remain with one order
    {
        Order a("EUR/GBP", 0.8567, 100, OrderType::SPOT_SELL, &cp);
        Order b("EUR/GBP", 0.8567, 200, OrderType::SPOT_SELL, &cp);
        long idA = a.getId(), idB = b.getId();

        om.processNewOrder(a);
        om.processNewOrder(b);
        om.processCancelOrder(idA);

        SubBook& sb    = om.getSubBook("EUR/GBP");
        auto&    level = sb.getSellOrdersRef().at(0.8567);

        check("Price level remains after partial cancel", sb.getSellOrdersRef().count(0.8567) == 1);
        check("Remaining order is order B",               level.size() == 1);
        check("Remaining order has correct ID",           level.front().getId() == idB);
    }

    // Cancel an order that does not exist — book must be unaffected
    {
        Order o("USD/CHF", 0.8891, 400, OrderType::SPOT_SELL, &cp);
        om.processNewOrder(o);
        SubBook& sb = om.getSubBook("USD/CHF");

        om.processCancelOrder(999999);   // bogus ID — stderr warning expected
        check("Book unaffected by invalid cancel",  sb.getSellOrdersRef().count(0.8891) == 1);
    }

    // ── 6. Counterparty tracking ──────────────────────────────────────────────
    section("Counterparty Tracking");

    // Orders placed → IDs appear in counterparty; order pointer matches
    {
        Counterparty trader("Trader A");

        Order x("SGD/USD", 0.7400, 500,  OrderType::SPOT_BUY,  &trader);
        Order y("SGD/USD", 0.7400, 1000, OrderType::SPOT_SELL, &trader);
        Order z("SGD/USD", 0.7410, 750,  OrderType::SPOT_BUY,  &trader);
        long idX = x.getId(), idY = y.getId(), idZ = z.getId();

        om.processNewOrder(x);
        om.processNewOrder(y);
        om.processNewOrder(z);

        const auto& ids = trader.getOrderIds();
        check("3 orders tracked after 3 submissions",  ids.size() == 3);
        check("First order ID present",                ids[0] == idX);
        check("Second order ID present",               ids[1] == idY);
        check("Third order ID present",                ids[2] == idZ);
        check("getCounterparty() returns correct ptr", x.getCounterparty() == &trader);
    }

    // Two counterparties track their orders independently
    {
        Counterparty alpha("Alpha Fund");
        Counterparty beta("Beta Fund");

        Order a("HKD/USD", 0.1280, 200, OrderType::SPOT_BUY,  &alpha);
        Order b("HKD/USD", 0.1280, 300, OrderType::SPOT_SELL, &beta);
        Order c("HKD/USD", 0.1285, 400, OrderType::SPOT_BUY,  &alpha);

        om.processNewOrder(a);
        om.processNewOrder(b);
        om.processNewOrder(c);

        check("Alpha has 2 orders",        alpha.getOrderIds().size() == 2);
        check("Beta has 1 order",          beta.getOrderIds().size()  == 1);
        check("Beta's order ID is correct",beta.getOrderIds()[0] == b.getId());
    }

    // Cancel removes order ID from counterparty
    {
        Counterparty seller("Sell Desk");

        Order p("CAD/USD", 0.7350, 600, OrderType::SPOT_SELL, &seller);
        Order q("CAD/USD", 0.7360, 800, OrderType::SPOT_SELL, &seller);
        long idP = p.getId(), idQ = q.getId();

        om.processNewOrder(p);
        om.processNewOrder(q);
        check("2 orders before cancel",       seller.getOrderIds().size() == 2);

        om.processCancelOrder(idP);
        const auto& ids = seller.getOrderIds();
        check("1 order remains after cancel", ids.size() == 1);
        check("Remaining ID is order Q",      ids[0] == idQ);
        check("Cancelled ID is gone",         ids[0] != idP);
    }

    // Invalid cancel leaves counterparty unchanged
    {
        Counterparty desk("Risk Desk");

        Order r("MXN/USD", 0.0580, 1000, OrderType::SPOT_BUY, &desk);
        om.processNewOrder(r);
        check("1 order before bogus cancel",  desk.getOrderIds().size() == 1);

        om.processCancelOrder(888888);   // bogus ID
        check("Counterparty unchanged",       desk.getOrderIds().size() == 1);
    }

    // ── Summary ───────────────────────────────────────────────────────────────
    std::cout << "\n" << std::string(45, '=') << "\n";
    std::cout << "Results: " << passed << " passed, " << failed << " failed\n";
    return failed > 0 ? 1 : 0;
}
