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

        check("3 price levels in buy book",           bids.size() == 3);
        check("Best bid (begin) is 1.0856",           bids.begin()->first  == 1.0856);
        check("Lowest buy level (rbegin) is 1.0835",  bids.rbegin()->first == 1.0835);
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

        Order x("SGD/USD", 0.7400, 500,  OrderType::SWAP_BUY,  &trader);
        Order y("SGD/USD", 0.7400, 1000, OrderType::SWAP_SELL, &trader);
        Order z("SGD/USD", 0.7410, 750,  OrderType::SWAP_BUY,  &trader);
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

        Order a("HKD/USD", 0.1280, 200, OrderType::SWAP_BUY,  &alpha);
        Order b("HKD/USD", 0.1280, 300, OrderType::SWAP_SELL, &beta);
        Order c("HKD/USD", 0.1285, 400, OrderType::SWAP_BUY,  &alpha);

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

    // ── 7. SPOT Matching Engine ───────────────────────────────────────────────
    section("SPOT Matching Engine");

    // Use symbols prefixed with "MATCH/" to avoid interference from prior tests

    // 7a. Exact match — standing bid fully consumed by incoming ask
    {
        Counterparty buyer("Buyer.A"), seller("Seller.A");
        Order bid("MATCH/EXACT", 1.1000, 100, OrderType::SPOT_BUY,  &buyer);
        Order ask("MATCH/EXACT", 1.1000, 100, OrderType::SPOT_SELL, &seller);
        long bidId = bid.getId();

        om.processNewOrder(bid);   // queued; buyer now tracks bidId
        om.processNewOrder(ask);   // matches bid; both fully consumed

        SubBook& sb = om.getSubBook("MATCH/EXACT");
        check("Exact: buy side empty",           sb.getBuyOrdersRef().empty());
        check("Exact: sell side empty",          sb.getSellOrdersRef().empty());
        check("Exact: buyer notified (1 trade)", buyer.getTrades().size()  == 1);
        check("Exact: seller notified (1 trade)",seller.getTrades().size() == 1);
        check("Exact: trade qty is 100",         buyer.getTrades()[0].quantity == 100);
        check("Exact: buyer sees wasBuy=true",   buyer.getTrades()[0].wasBuy  == true);
        check("Exact: seller sees wasBuy=false", seller.getTrades()[0].wasBuy == false);
        check("Exact: buyer's orderId removed",  buyer.getOrderIds().empty());
        check("Exact: seller never queued",      seller.getOrderIds().empty());
        check("Exact: seller's counterparty name seen by buyer",
              buyer.getTrades()[0].counterpartyName == "Seller.A");
    }

    // 7b. Incoming buy larger than standing ask — ask consumed, buy remainder queued
    {
        Counterparty buyer("Buyer.B"), seller("Seller.B");
        Order ask("MATCH/BUY.PARTIAL", 1.2500, 200, OrderType::SPOT_SELL, &seller);
        Order bid("MATCH/BUY.PARTIAL", 1.2500, 500, OrderType::SPOT_BUY,  &buyer);

        om.processNewOrder(ask);  // queued (200 qty)
        om.processNewOrder(bid);  // matches 200; 300 remainder queued

        SubBook& sb = om.getSubBook("MATCH/BUY.PARTIAL");
        check("Buy partial: ask side empty",         sb.getSellOrdersRef().empty());
        check("Buy partial: buy side has remainder", !sb.getBuyOrdersRef().empty());
        check("Buy partial: remaining buy qty=300",
              sb.getBuyOrdersRef().begin()->second.front().getQuantity() == 300);
        check("Buy partial: buyer notified",         buyer.getTrades().size()  == 1);
        check("Buy partial: seller notified",        seller.getTrades().size() == 1);
        check("Buy partial: trade qty is 200",       buyer.getTrades()[0].quantity == 200);
        check("Buy partial: buyer tracks remainder", buyer.getOrderIds().size() == 1);
        check("Buy partial: seller orderId removed", seller.getOrderIds().empty());
    }

    // 7c. Incoming buy smaller than standing ask — buy fully consumed, ask reduced
    {
        Counterparty buyer("Buyer.C"), seller("Seller.C");
        Order ask("MATCH/ASK.PARTIAL", 1.3600, 500, OrderType::SPOT_SELL, &seller);
        Order bid("MATCH/ASK.PARTIAL", 1.3600, 200, OrderType::SPOT_BUY,  &buyer);

        om.processNewOrder(ask);  // queued (500 qty)
        om.processNewOrder(bid);  // matches 200; ask reduced to 300

        SubBook& sb = om.getSubBook("MATCH/ASK.PARTIAL");
        check("Ask partial: buy side empty",     sb.getBuyOrdersRef().empty());
        check("Ask partial: ask still in book",  !sb.getSellOrdersRef().empty());
        check("Ask partial: ask qty reduced to 300",
              sb.getSellOrdersRef().begin()->second.front().getQuantity() == 300);
        check("Ask partial: buyer notified",     buyer.getTrades().size()  == 1);
        check("Ask partial: seller notified",    seller.getTrades().size() == 1);
        check("Ask partial: trade qty is 200",   buyer.getTrades()[0].quantity == 200);
        check("Ask partial: buyer not queued",   buyer.getOrderIds().empty());
        check("Ask partial: seller still queued",seller.getOrderIds().size() == 1);
    }

    // 7d. Multi-level sweep — incoming buy sweeps two ask price levels
    {
        Counterparty buyer("Buyer.D"), sellerLo("Seller.DLo"), sellerHi("Seller.DHi");
        Order askLo("MATCH/BUY.SWEEP", 1.0700, 300, OrderType::SPOT_SELL, &sellerLo);
        Order askHi("MATCH/BUY.SWEEP", 1.0720, 300, OrderType::SPOT_SELL, &sellerHi);
        Order bid  ("MATCH/BUY.SWEEP", 1.0750, 600, OrderType::SPOT_BUY,  &buyer);

        om.processNewOrder(askLo);
        om.processNewOrder(askHi);
        om.processNewOrder(bid);   // sweeps both levels

        SubBook& sb = om.getSubBook("MATCH/BUY.SWEEP");
        check("Sweep: buy fully filled",        sb.getBuyOrdersRef().empty());
        check("Sweep: sell side empty",         sb.getSellOrdersRef().empty());
        check("Sweep: buyer got 2 trade fills", buyer.getTrades().size()   == 2);
        check("Sweep: low seller notified",     sellerLo.getTrades().size() == 1);
        check("Sweep: high seller notified",    sellerHi.getTrades().size() == 1);
        check("Sweep: first fill at 1.0700",    buyer.getTrades()[0].price == 1.0700);
        check("Sweep: second fill at 1.0720",   buyer.getTrades()[1].price == 1.0720);
    }

    // 7e. Multi-level sweep — incoming sell sweeps two bid price levels
    {
        Counterparty seller("Seller.E"), buyerHi("Buyer.EHi"), buyerLo("Buyer.ELo");
        Order bidHi("MATCH/SELL.SWEEP", 1.0850, 200, OrderType::SPOT_BUY,  &buyerHi);
        Order bidLo("MATCH/SELL.SWEEP", 1.0830, 200, OrderType::SPOT_BUY,  &buyerLo);
        Order ask  ("MATCH/SELL.SWEEP", 1.0820, 400, OrderType::SPOT_SELL, &seller);

        om.processNewOrder(bidHi);
        om.processNewOrder(bidLo);
        om.processNewOrder(ask);   // sweeps both bid levels

        SubBook& sb = om.getSubBook("MATCH/SELL.SWEEP");
        check("Sell sweep: sell fully filled",       sb.getSellOrdersRef().empty());
        check("Sell sweep: buy side empty",          sb.getBuyOrdersRef().empty());
        check("Sell sweep: seller got 2 fills",      seller.getTrades().size()  == 2);
        check("Sell sweep: high buyer notified",     buyerHi.getTrades().size() == 1);
        check("Sell sweep: low buyer notified",      buyerLo.getTrades().size() == 1);
        check("Sell sweep: first fill at 1.0850",   seller.getTrades()[0].price == 1.0850);
        check("Sell sweep: second fill at 1.0830",  seller.getTrades()[1].price == 1.0830);
    }

    // 7f. No match — bid price below best ask
    {
        Counterparty buyer("Buyer.F"), seller("Seller.F");
        Order ask("MATCH/NOMATCH", 1.0600, 100, OrderType::SPOT_SELL, &seller);
        Order bid("MATCH/NOMATCH", 1.0550, 100, OrderType::SPOT_BUY,  &buyer);

        om.processNewOrder(ask);   // queued
        om.processNewOrder(bid);   // bid (1.0550) < ask (1.0600) — no trade

        SubBook& sb = om.getSubBook("MATCH/NOMATCH");
        check("No match: bid in book",             !sb.getBuyOrdersRef().empty());
        check("No match: ask in book",             !sb.getSellOrdersRef().empty());
        check("No match: buyer not notified",      buyer.getTrades().empty());
        check("No match: seller not notified",     seller.getTrades().empty());
        check("No match: buyer tracks its order",  buyer.getOrderIds().size()  == 1);
        check("No match: seller tracks its order", seller.getOrderIds().size() == 1);
    }

    // 7g. Incoming sell matches standing bid exactly (mirror of 7a)
    {
        Counterparty buyer("Buyer.G"), seller("Seller.G");
        Order bid("MATCH/SELL.EXACT", 17.2500, 400, OrderType::SPOT_BUY,  &buyer);
        Order ask("MATCH/SELL.EXACT", 17.2500, 400, OrderType::SPOT_SELL, &seller);

        om.processNewOrder(bid);   // queued
        om.processNewOrder(ask);   // matches bid exactly

        SubBook& sb = om.getSubBook("MATCH/SELL.EXACT");
        check("Sell exact: buy side empty",           sb.getBuyOrdersRef().empty());
        check("Sell exact: sell side empty",          sb.getSellOrdersRef().empty());
        check("Sell exact: buyer notified",           buyer.getTrades().size()  == 1);
        check("Sell exact: seller notified",          seller.getTrades().size() == 1);
        check("Sell exact: trade qty is 400",         buyer.getTrades()[0].quantity == 400);
        check("Sell exact: buyer sees wasBuy=true",   buyer.getTrades()[0].wasBuy  == true);
        check("Sell exact: seller sees wasBuy=false", seller.getTrades()[0].wasBuy == false);
        check("Sell exact: buyer's orderId removed",  buyer.getOrderIds().empty());
        check("Sell exact: seller never queued",      seller.getOrderIds().empty());
        check("Sell exact: buyer's counterparty name seen by seller",
              seller.getTrades()[0].counterpartyName == "Buyer.G");
    }

    // ── Summary ───────────────────────────────────────────────────────────────
    std::cout << "\n" << std::string(45, '=') << "\n";
    std::cout << "Results: " << passed << " passed, " << failed << " failed\n";
    return failed > 0 ? 1 : 0;
}
