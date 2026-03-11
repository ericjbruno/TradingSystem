#include <iostream>
#include <string>
#include "Counterparty.h"
#include "OrderManager.h"
#include "MarketManager.h"
#include "Order.h"
#include "OrderType.h"
#include "SubBook.h"

// ─── Minimal test framework ───────────────────────────────────────────────────
//
// Run all sections:          ./tests
// Run one section by name:   ./tests "Trade Pricing"
// (substring match, case-sensitive)

static int  passed = 0, failed = 0;
static bool sectionActive = true;   // true when current section passes the filter
static std::string testFilter;      // empty = run all sections

void check(const std::string& name, bool result) {
    if (!sectionActive) return;
    if (result) {
        std::cout << "  [PASS] " << name << "\n";
        ++passed;
    } else {
        std::cout << "  [FAIL] " << name << "\n";
        ++failed;
    }
}

void section(const std::string& name) {
    sectionActive = testFilter.empty() ||
                    name.find(testFilter) != std::string::npos;
    if (sectionActive)
        std::cout << "\n" << name << "\n" << std::string(name.size(), '-') << "\n";
}

// ─── Tests ────────────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    if (argc > 1) testFilter = argv[1];

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

        om.processNewOrder(bid);   // queued; buyer now tracks bid ID
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

    // ── 8. Trade Pricing — execution at the maker (standing-order) price ─────
    section("Trade Pricing");

    // 8a. Incoming BUY with bid > ask: fills at the ask price, not the bid
    {
        Counterparty buyer("TP.Buyer.A"), seller("TP.Seller.A");
        Order ask("TP/A", 1.1000, 100, OrderType::SPOT_SELL, &seller);
        Order bid("TP/A", 1.1050, 100, OrderType::SPOT_BUY,  &buyer);

        om.processNewOrder(ask);
        om.processNewOrder(bid);

        check("TP 8a: fills at ask price 1.1000, not bid 1.1050",
              buyer.getTrades()[0].price == 1.1000);
    }

    // 8b. Incoming SELL with ask < bid: fills at the bid price, not the ask
    {
        Counterparty buyer("TP.Buyer.B"), seller("TP.Seller.B");
        Order bid("TP/B", 1.2000, 100, OrderType::SPOT_BUY,  &buyer);
        Order ask("TP/B", 1.1950, 100, OrderType::SPOT_SELL, &seller);

        om.processNewOrder(bid);
        om.processNewOrder(ask);

        check("TP 8b: fills at bid price 1.2000, not ask 1.1950",
              seller.getTrades()[0].price == 1.2000);
    }

    // 8c. Sweep across two price levels: each fill executes at its own level's price
    {
        Counterparty buyer("TP.Buyer.C"), sellerLo("TP.Seller.CLo"), sellerHi("TP.Seller.CHi");
        Order askLo("TP/C", 1.0500, 100, OrderType::SPOT_SELL, &sellerLo);
        Order askHi("TP/C", 1.0520, 100, OrderType::SPOT_SELL, &sellerHi);
        Order bid  ("TP/C", 1.0600, 200, OrderType::SPOT_BUY,  &buyer);

        om.processNewOrder(askLo);
        om.processNewOrder(askHi);
        om.processNewOrder(bid);

        check("TP 8c: first fill at lower ask level 1.0500",  buyer.getTrades()[0].price == 1.0500);
        check("TP 8c: second fill at higher ask level 1.0520", buyer.getTrades()[1].price == 1.0520);
    }

    // ── 9. FIFO Fill Order Within a Price Level ───────────────────────────────
    section("FIFO Fill Order");

    // 9a. Two asks at the same price — the first one placed is filled first
    {
        Counterparty buyer("FF.Buyer.A"), sellerFirst("FF.Seller.First"), sellerSecond("FF.Seller.Second");
        Order askFirst ("FF/A", 1.0000, 100, OrderType::SPOT_SELL, &sellerFirst);
        Order askSecond("FF/A", 1.0000, 100, OrderType::SPOT_SELL, &sellerSecond);
        Order bid      ("FF/A", 1.0000, 100, OrderType::SPOT_BUY,  &buyer);

        om.processNewOrder(askFirst);
        om.processNewOrder(askSecond);
        om.processNewOrder(bid);  // exactly fills askFirst; askSecond untouched

        SubBook& sb = om.getSubBook("FF/A");
        auto&    asks = sb.getSellOrdersRef();
        check("FF 9a: one ask level remains",              asks.size() == 1);
        check("FF 9a: remaining order is askSecond",       asks.begin()->second.front().getId() == askSecond.getId());
        check("FF 9a: first seller was notified",          sellerFirst.getTrades().size()  == 1);
        check("FF 9a: second seller was NOT notified",     sellerSecond.getTrades().empty());
        check("FF 9a: buyer was notified",                 buyer.getTrades().size() == 1);
    }

    // 9b. Two bids at the same price — the first one placed is filled first
    {
        Counterparty seller("FF.Seller.B"), buyerFirst("FF.Buyer.First"), buyerSecond("FF.Buyer.Second");
        Order bidFirst ("FF/B", 1.5000, 100, OrderType::SPOT_BUY,  &buyerFirst);
        Order bidSecond("FF/B", 1.5000, 100, OrderType::SPOT_BUY,  &buyerSecond);
        Order ask      ("FF/B", 1.5000, 100, OrderType::SPOT_SELL, &seller);

        om.processNewOrder(bidFirst);
        om.processNewOrder(bidSecond);
        om.processNewOrder(ask);  // exactly fills bidFirst; bidSecond untouched

        SubBook& sb   = om.getSubBook("FF/B");
        auto&    bids = sb.getBuyOrdersRef();
        check("FF 9b: one bid level remains",              bids.size() == 1);
        check("FF 9b: remaining order is bidSecond",       bids.begin()->second.front().getId() == bidSecond.getId());
        check("FF 9b: first buyer was notified",           buyerFirst.getTrades().size()  == 1);
        check("FF 9b: second buyer was NOT notified",      buyerSecond.getTrades().empty());
    }

    // 9c. Large incoming buy consumes first ask entirely, then partially fills second
    {
        Counterparty buyer("FF.Buyer.C"), sellerA("FF.Seller.CA"), sellerB("FF.Seller.CB");
        Order askA("FF/C", 2.0000, 100, OrderType::SPOT_SELL, &sellerA);
        Order askB("FF/C", 2.0000, 200, OrderType::SPOT_SELL, &sellerB);
        Order bid ("FF/C", 2.0000, 250, OrderType::SPOT_BUY,  &buyer);

        om.processNewOrder(askA);
        om.processNewOrder(askB);
        om.processNewOrder(bid);  // consumes all 100 of askA, then 150 of askB

        SubBook& sb = om.getSubBook("FF/C");
        check("FF 9c: buyer got 2 trade fills",             buyer.getTrades().size() == 2);
        check("FF 9c: first fill qty=100 (all of askA)",    buyer.getTrades()[0].quantity == 100);
        check("FF 9c: second fill qty=150 (partial askB)",  buyer.getTrades()[1].quantity == 150);
        check("FF 9c: sellerA fully consumed",              sellerA.getTrades()[0].quantity == 100);
        check("FF 9c: sellerB partially filled (qty=150)",  sellerB.getTrades()[0].quantity == 150);
        check("FF 9c: askB remainder in book = 50",
              sb.getSellOrdersRef().begin()->second.front().getQuantity() == 50);
    }

    // ── 10. Trade Notification Details ───────────────────────────────────────
    section("Trade Notification Details");

    // 10a. Order IDs in notifications are correct for both sides
    {
        Counterparty buyer("ND.Buyer.A"), seller("ND.Seller.A");
        Order bid("ND/A", 1.0000, 100, OrderType::SPOT_BUY,  &buyer);
        Order ask("ND/A", 1.0000, 100, OrderType::SPOT_SELL, &seller);

        om.processNewOrder(bid);
        om.processNewOrder(ask);

        check("ND 10a: buyer's notification has buyer's order ID",
              buyer.getTrades()[0].orderId  == bid.getId());
        check("ND 10a: seller's notification has seller's order ID",
              seller.getTrades()[0].orderId == ask.getId());
    }

    // 10b. Both sides see the same execution price
    {
        Counterparty buyer("ND.Buyer.B"), seller("ND.Seller.B");
        Order bid("ND/B", 1.3000, 200, OrderType::SPOT_BUY,  &buyer);
        Order ask("ND/B", 1.2950, 200, OrderType::SPOT_SELL, &seller);

        om.processNewOrder(bid);
        om.processNewOrder(ask);

        check("ND 10b: buyer and seller see same price",
              buyer.getTrades()[0].price == seller.getTrades()[0].price);
        check("ND 10b: execution price is the maker bid price 1.3000",
              buyer.getTrades()[0].price == 1.3000);
    }

    // 10c. Counterparty names cross-reference correctly
    {
        Counterparty buyer("ND.Buyer.C"), seller("ND.Seller.C");
        Order bid("ND/C", 1.0000, 50, OrderType::SPOT_BUY,  &buyer);
        Order ask("ND/C", 1.0000, 50, OrderType::SPOT_SELL, &seller);

        om.processNewOrder(bid);
        om.processNewOrder(ask);

        check("ND 10c: buyer sees seller's name",
              buyer.getTrades()[0].counterpartyName  == "ND.Seller.C");
        check("ND 10c: seller sees buyer's name",
              seller.getTrades()[0].counterpartyName == "ND.Buyer.C");
    }

    // 10d. Partial fill: notification quantity is the fill qty, not the original order qty
    {
        Counterparty buyer("ND.Buyer.D"), seller("ND.Seller.D");
        Order ask("ND/D", 1.0000, 500, OrderType::SPOT_SELL, &seller);
        Order bid("ND/D", 1.0000, 150, OrderType::SPOT_BUY,  &buyer);

        om.processNewOrder(ask);  // 500 qty standing
        om.processNewOrder(bid);  // fills 150

        check("ND 10d: buyer notification qty = 150 (fill), not 150 (same here)",
              buyer.getTrades()[0].quantity  == 150);
        check("ND 10d: seller notification qty = 150 (fill), not 500 (original)",
              seller.getTrades()[0].quantity == 150);
        check("ND 10d: seller's ask remainder in book = 350",
              om.getSubBook("ND/D").getSellOrdersRef().begin()->second.front().getQuantity() == 350);
    }

    // ── 11. Cascade Fills — remainder of one trade matches a later order ──────
    section("Cascade Fills");

    // 11a. Buy partially fills ask; later sell matches the remaining buy remainder
    {
        Counterparty buyer("CF.Buyer.A"), seller1("CF.Seller.A1"), seller2("CF.Seller.A2");
        Order ask1("CF/A", 1.0000, 100, OrderType::SPOT_SELL, &seller1);
        Order bid ("CF/A", 1.0000, 300, OrderType::SPOT_BUY,  &buyer);   // fills 100, 200 remainder queued
        Order ask2("CF/A", 1.0000, 200, OrderType::SPOT_SELL, &seller2); // matches the 200 remainder

        om.processNewOrder(ask1);
        om.processNewOrder(bid);   // trade 1: buyer(300) vs seller1(100) → 100 filled, 200 remainder
        om.processNewOrder(ask2);  // trade 2: buyer's remainder(200) vs seller2(200) → fully consumed

        SubBook& sb = om.getSubBook("CF/A");
        check("CF 11a: book empty after cascade",           sb.getBuyOrdersRef().empty() && sb.getSellOrdersRef().empty());
        check("CF 11a: buyer received 2 trade notifications", buyer.getTrades().size()  == 2);
        check("CF 11a: seller1 received 1 notification",     seller1.getTrades().size() == 1);
        check("CF 11a: seller2 received 1 notification",     seller2.getTrades().size() == 1);
        check("CF 11a: first fill qty=100",                  buyer.getTrades()[0].quantity == 100);
        check("CF 11a: second fill qty=200",                 buyer.getTrades()[1].quantity == 200);
        check("CF 11a: buyer order ID removed after full fill", buyer.getOrderIds().empty());
    }

    // 11b. Sell partially fills bid; later buy matches the remaining sell remainder
    {
        Counterparty seller("CF.Seller.B"), buyer1("CF.Buyer.B1"), buyer2("CF.Buyer.B2");
        Order bid1("CF/B", 1.0000, 100, OrderType::SPOT_BUY,  &buyer1);
        Order ask ("CF/B", 1.0000, 300, OrderType::SPOT_SELL, &seller);  // fills 100, 200 remainder queued
        Order bid2("CF/B", 1.0000, 200, OrderType::SPOT_BUY,  &buyer2); // matches the 200 remainder

        om.processNewOrder(bid1);
        om.processNewOrder(ask);   // trade 1: seller(300) vs buyer1(100) → 100 filled, 200 remainder
        om.processNewOrder(bid2);  // trade 2: seller's remainder(200) vs buyer2(200) → fully consumed

        SubBook& sb = om.getSubBook("CF/B");
        check("CF 11b: book empty after cascade",             sb.getBuyOrdersRef().empty() && sb.getSellOrdersRef().empty());
        check("CF 11b: seller received 2 trade notifications", seller.getTrades().size()  == 2);
        check("CF 11b: buyer1 received 1 notification",        buyer1.getTrades().size() == 1);
        check("CF 11b: buyer2 received 1 notification",        buyer2.getTrades().size() == 1);
        check("CF 11b: second fill qty=200",                   seller.getTrades()[1].quantity == 200);
        check("CF 11b: seller order ID removed after full fill", seller.getOrderIds().empty());
    }

    // 11c. Three-order chain: ask partially filled → remainder matched by bid → bid remainder queued
    {
        Counterparty buyerA("CF.Buyer.CA"), sellerB("CF.Seller.CB"), buyerC("CF.Buyer.CC");
        Order askB("CF/C", 1.0000, 500, OrderType::SPOT_SELL, &sellerB);  // standing ask
        Order bidA("CF/C", 1.0000, 200, OrderType::SPOT_BUY,  &buyerA);  // fills 200 of askB
        Order bidC("CF/C", 1.0000, 400, OrderType::SPOT_BUY,  &buyerC);  // fills remaining 300 of askB, 100 left over

        om.processNewOrder(askB);
        om.processNewOrder(bidA);  // trade: askB(500) vs bidA(200) → askB has 300 remaining
        om.processNewOrder(bidC);  // trade: askB's 300 remainder vs bidC(400) → bidC has 100 remaining, askB consumed

        SubBook& sb = om.getSubBook("CF/C");
        check("CF 11c: ask side empty (askB fully consumed)",  sb.getSellOrdersRef().empty());
        check("CF 11c: bid side has bidC remainder of 100",
              !sb.getBuyOrdersRef().empty() &&
              sb.getBuyOrdersRef().begin()->second.front().getQuantity() == 100);
        check("CF 11c: sellerB got 2 fills (200 + 300)",       sellerB.getTrades().size() == 2);
        check("CF 11c: buyerA got 1 fill of 200",              buyerA.getTrades().size()  == 1);
        check("CF 11c: buyerC got 1 fill of 300",              buyerC.getTrades().size()  == 1);
        check("CF 11c: buyerC's fill qty = 300 (not 400)",     buyerC.getTrades()[0].quantity == 300);
        check("CF 11c: buyerC still tracks its remainder",     buyerC.getOrderIds().size() == 1);
    }

    // ── 12. Price Boundary Conditions ────────────────────────────────────────
    section("Price Boundary Conditions");

    // 12a. Bid exactly equals ask → trade executes (>= is inclusive)
    {
        Counterparty buyer("PB.Buyer.A"), seller("PB.Seller.A");
        Order ask("PB/A", 1.0500, 100, OrderType::SPOT_SELL, &seller);
        Order bid("PB/A", 1.0500, 100, OrderType::SPOT_BUY,  &buyer);

        om.processNewOrder(ask);
        om.processNewOrder(bid);

        check("PB 12a: bid == ask triggers a trade",           buyer.getTrades().size()  == 1);
        check("PB 12a: book is empty after exact match",       om.getSubBook("PB/A").getBuyOrdersRef().empty() &&
                                                               om.getSubBook("PB/A").getSellOrdersRef().empty());
    }

    // 12b. Bid one pip below ask → no trade; both sides queued
    {
        Counterparty buyer("PB.Buyer.B"), seller("PB.Seller.B");
        Order ask("PB/B", 1.0500,  100, OrderType::SPOT_SELL, &seller);
        Order bid("PB/B", 1.0499,  100, OrderType::SPOT_BUY,  &buyer);

        om.processNewOrder(ask);
        om.processNewOrder(bid);

        SubBook& sb = om.getSubBook("PB/B");
        check("PB 12b: bid one pip below ask → no trade",  buyer.getTrades().empty());
        check("PB 12b: ask stays in book",                 !sb.getSellOrdersRef().empty());
        check("PB 12b: bid stays in book",                 !sb.getBuyOrdersRef().empty());
    }

    // 12c. Incoming sell at exactly the standing bid price → trade executes
    {
        Counterparty buyer("PB.Buyer.C"), seller("PB.Seller.C");
        Order bid("PB/C", 1.1000, 100, OrderType::SPOT_BUY,  &buyer);
        Order ask("PB/C", 1.1000, 100, OrderType::SPOT_SELL, &seller);

        om.processNewOrder(bid);
        om.processNewOrder(ask);  // ask == bid → should trade

        check("PB 12c: ask == bid triggers a trade",        seller.getTrades().size() == 1);
        check("PB 12c: execution price equals bid 1.1000",  seller.getTrades()[0].price == 1.1000);
    }

    // 12d. Incoming sell one pip above standing bid → no trade
    {
        Counterparty buyer("PB.Buyer.D"), seller("PB.Seller.D");
        Order bid("PB/D", 1.1000, 100, OrderType::SPOT_BUY,  &buyer);
        Order ask("PB/D", 1.1001, 100, OrderType::SPOT_SELL, &seller);

        om.processNewOrder(bid);
        om.processNewOrder(ask);

        check("PB 12d: ask one pip above bid → no trade",   seller.getTrades().empty());
        check("PB 12d: bid stays in book",                  !om.getSubBook("PB/D").getBuyOrdersRef().empty());
        check("PB 12d: ask stays in book",                  !om.getSubBook("PB/D").getSellOrdersRef().empty());
    }

    // ── Summary ───────────────────────────────────────────────────────────────
    std::cout << "\n" << std::string(45, '=') << "\n";
    std::cout << "Results: " << passed << " passed, " << failed << " failed\n";
    return failed > 0 ? 1 : 0;
}
