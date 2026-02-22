# TradingSystem

A C++ forex trading system that processes orders, maintains a live price-time-priority order book, executes SPOT trades via a continuous matching engine, and notifies counterparties of fills.

---

## Overview

TradingSystem reads forex orders from a CSV file (or accepts them programmatically), attempts to match each incoming SPOT order against the standing book, queues any unfilled remainder, and prints an ASCII snapshot of the resulting order book.

The system is designed around a clean layered architecture — presentation, business logic, and data storage — with no external dependencies beyond the C++ standard library.

---

## Features

- **SPOT matching engine** — incoming SPOT orders are matched against the opposite side before queuing; supports partial fills, multi-level sweeps, and counterparty fill notifications
- **Price-time priority** — bids sorted highest-first (`BidMap`), asks sorted lowest-first (`AskMap`); `begin()` always returns the best price on both sides in O(1)
- **Per-symbol SubBook** — `BidMap` (`std::map` with `std::greater`) and `AskMap` (`std::map` default ascending) replace the old single-type `PriceLevelMap`
- **O(1) cancellation** — `OrderLocation` stores a `std::list` iterator and a type-erased `eraseLevel` lambda, enabling stable O(1) removal regardless of map type
- **Counterparty tracking** — each order carries a non-owning pointer to its counterparty; counterparties maintain a live list of open order IDs and a fill history (`vector<TradeNotification>`)
- **Trade logging** — every fill is printed to stdout with symbol, quantity, price, and both sides' names and order IDs
- All 10 order types (Market, Limit, Stop, Spot, Swap × Buy/Sell) route correctly using the even/odd enum convention
- Lazy-initialized order book — SubBooks are created on demand per symbol
- Pure C++ standard library, no third-party dependencies
- 109-test suite covering routing, price priority, time priority, cancellation, counterparty tracking, and 7 SPOT matching scenarios

---

## Project Structure

```
TradingSystem/
├── TradingSystem.cpp      # Entry point — CSV parser, order factory, printOrderBook()
├── Counterparty.cpp / .h  # Counterparty identity, open-order tracking, TradeNotification
├── Order.cpp / .h         # Order value object with auto-increment ID and counterparty ref
├── OrderType.h            # Order type enum (even=buy, odd=sell)
├── OrderBook.cpp / .h     # Central registry: symbol→SubBook map + OrderLocation cancel index
├── SubBook.cpp / .h       # BidMap and AskMap type aliases + SubBook container
├── OrderManager.cpp / .h  # Order lifecycle — matching, queuing, cancellation
├── TradeManager.cpp / .h  # Matching engine, fill logging, Trade and TradeManager
├── MarketPrice.cpp / .h   # Market price value object
├── MarketManager.cpp / .h # Market data stub (future integration)
├── tests.cpp              # Test suite (109 tests)
├── forex_orders.csv       # Sample input data
├── ARCHITECTURE.md        # Detailed architecture documentation
└── OPTIMIZATIONS.md       # Suggested future performance improvements
```

---

## Architecture

### Layers

```
┌─────────────────────────────────────────┐
│  TradingSystem.cpp                      │  Presentation
│  CSV parser · Order factory             │
│  printOrderBook() ASCII display         │
└────────────────────┬────────────────────┘
                     │
┌────────────────────▼────────────────────┐
│  OrderManager                           │  Business Logic
│  processNewOrder() · cancel()           │
│  ┌─────────────────────────────────┐    │
│  │  TradeManager                   │    │
│  │  matchSpotOrders()              │    │
│  │  logAndNotify()  pricesMatch()  │    │
│  └─────────────────────────────────┘    │
│  MarketManager (stub)                   │
└────────────────────┬────────────────────┘
                     │
┌────────────────────▼────────────────────┐
│  OrderBook                              │  Data Storage
│  SubBook per symbol (BidMap + AskMap)   │
└─────────────────────────────────────────┘
```

### OrderBook and SubBook Structure

```
OrderBook
│
├── books: unordered_map<string, SubBook>          O(1) lookup
│   ├── "EUR/USD" ──► SubBook
│   │                 ├── buyOrders  (BidMap — descending, begin()=best bid)
│   │                 └── sellOrders (AskMap — ascending,  begin()=best ask)
│   └── ...
│
└── orderIndex: unordered_map<long, OrderLocation>  O(1) cancel
    ├── id=1  ──► { list<Order>*, price=1.0842, iterator, eraseLevel }
    └── id=26 ──► { list<Order>*, price=1.0842, iterator, eraseLevel }
```

### Price Level Structure

Each map entry holds a `std::list<Order>` in strict FIFO arrival order:

```
EUR/USD  buyOrders  (BidMap — highest price first)
──────────────────────────────────────────────────
 1.0856  │  [ Order{id=76, qty=9912} ]           ← begin() = best bid
 1.0842  │  [ Order{id=1,  qty=9847} ─ Order{id=26, qty=10123} ]
 1.0835  │  [ Order{id=51, qty=10789} ]

EUR/USD  sellOrders  (AskMap — lowest price first)
──────────────────────────────────────────────────
 1.0863  │  [ Order{id=52, qty=11045} ]           ← begin() = best ask
 1.0870  │  [ Order{id=77, qty=8834} ]
```

### SPOT Matching Flow

```
processNewOrder(SPOT_BUY, price=1.0870, qty=500)
        │
        ▼
matchSpotOrders(incoming, sb, book)
        │
        ├── asks.begin() → price=1.0863  (1.0870 >= 1.0863 ✓ cross)
        │     fillQty = min(500, 11045) = 500
        │     logAndNotify(Trade{qty=500, @1.0863, ...})
        │     incoming.qty = 0  →  ask qty reduced to 10545
        │
        └── incoming.qty == 0 → fully filled, do not queue
```

### Cancellation Flow (O(1))

```
processCancelOrder(id=1)
        │
        ▼
getOrderCounterparty(1)          read Counterparty* before erasure
        │
        ▼
orderIndex.find(1) → OrderLocation { list*, price=1.0842, it, eraseLevel }
        │
        ▼
list::erase(it)                  O(1) — removes only this node; other iterators unaffected
if list empty → eraseLevel(1.0842)    removes price level from BidMap or AskMap
orderIndex.erase(1)
counterparty->removeOrderId(1)
```

See [ARCHITECTURE.md](ARCHITECTURE.md) for full component details, data flow diagrams, and design patterns.

---

## Order Types

| Value | Type         | Side | Matching |
|-------|--------------|------|---------|
| 0     | MARKET_BUY   | Buy  | queued only |
| 1     | MARKET_SELL  | Sell | queued only |
| 2     | LIMIT_BUY    | Buy  | queued only |
| 3     | LIMIT_SELL   | Sell | queued only |
| 4     | STOP_BUY     | Buy  | queued only |
| 5     | STOP_SELL    | Sell | queued only |
| 6     | SPOT_BUY     | Buy  | **matched + queued** |
| 7     | SPOT_SELL    | Sell | **matched + queued** |
| 8     | SWAP_BUY     | Buy  | queued only |
| 9     | SWAP_SELL    | Sell | queued only |

Even values = Buy, Odd values = Sell.

---

## Input Format

Orders are read from `forex_orders.csv`:

```csv
Symbol,Price,Quantity,Side
EUR/USD,1.0842,9847,BUY
GBP/USD,1.2634,10523,SELL
USD/JPY,149.82,5000,BUY
```

### Supported Currency Pairs

Any combination of: EUR, GBP, USD, JPY, CHF, AUD, CAD, NZD, SGD, HKD, CNY, MXN, ZAR

---

## Build & Run

**Build main binary:**
```bash
g++ -fdiagnostics-color=always -g \
    Counterparty.cpp Order.cpp OrderBook.cpp OrderManager.cpp SubBook.cpp \
    MarketPrice.cpp MarketManager.cpp TradeManager.cpp \
    TradingSystem.cpp -o TradingSystem
```

**Run:**
```bash
./TradingSystem
```

**Expected output (excerpt):**
```
Processed order #1: BUY 9847 EUR/USD @ 1.0842  [Goldman Sachs]
[TRADE] EUR/USD  qty=500  @ 1.0842  | Buy#3 (Deutsche Bank)  Sell#1 (Goldman Sachs)
Processed order #2: SELL 10523 GBP/USD @ 1.2634  [JP Morgan]
...
Total orders processed: 100

================================================================
                         ORDER BOOK
================================================================

  EUR/USD
  SELL (Ask)
      1.0849      9,912    [#76]  <- best ask
      1.0856     10,123    [#26]
  ................................................................
  BUY  (Bid)
      1.0842      9,847    [#1]   <- best bid
      1.0835     10,789    [#51]
  ================================================================
```

---

## Testing

The test suite lives in `tests.cpp` and uses a minimal pass/fail framework with no external dependencies.

**Build and run:**
```bash
g++ -fdiagnostics-color=always -g \
    Counterparty.cpp Order.cpp OrderBook.cpp OrderManager.cpp SubBook.cpp \
    MarketPrice.cpp MarketManager.cpp TradeManager.cpp \
    tests.cpp -o run_tests

./run_tests
```

**Test sections (109 tests total):**

| Section | Tests | What is verified |
|---------|-------|-----------------|
| Order Routing | 20 | All 10 order types route to the correct side |
| Buy-Side Price Priority | 3 | `begin()` returns best bid (highest); `rbegin()` returns lowest |
| Sell-Side Price Priority | 3 | `begin()` returns best ask (lowest); `rbegin()` returns highest |
| Time Priority (FIFO) | 7 | Orders at the same price level maintain strict insertion order |
| Order Cancellation | 7 | Price level removed on last cancel; preserved on partial cancel; invalid ID is a no-op |
| Counterparty Tracking | 12 | IDs accumulate on submit, removed on cancel; two counterparties track independently; bogus cancel is a no-op |
| SPOT Matching Engine | 57 | Exact match, partial buy/ask fills, buy/sell multi-level sweeps, no-match guard, sell-exact mirror; counterparty name in notifications |

---

## Current Status

- SPOT order matching is fully implemented with partial fills and counterparty notifications
- `MarketManager` remains a stub (no live data feeds)
- MARKET, LIMIT, STOP, and SWAP orders are queued but not yet matched against each other
- No persistence layer — all state is in-memory
- Single-threaded only

---

## Planned Features

- Matching engine for LIMIT, STOP, and SWAP order types
- Real-time market data integration (WebSocket / FIX protocol)
- Order modification (price / quantity amendment)
- Trade and order history / audit trail
- Position and portfolio management
- Risk management and limits
- Database persistence
- Multi-threaded order processing
