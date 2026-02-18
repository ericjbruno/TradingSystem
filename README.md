# TradingSystem

A C++ forex order management system that processes trading orders, maintains a live order book, and checks trade execution conditions across multiple currency pairs.

---

## Overview

TradingSystem reads forex orders from a CSV file, builds a sorted order book organized by currency pair and side (buy/sell), and evaluates each order against market prices to determine if execution conditions are met.

The system is designed around a clean layered architecture — presentation, business logic, and data storage — with no external dependencies beyond the C++ standard library.

---

## Features

- Parses forex orders from CSV input
- Maintains a per-symbol order book with price-priority sorting
- O(1) symbol lookup via `unordered_map<string, SubBook>`
- O(log n) order insertion via price-level map (`std::map<double, std::list<Order>>`)
- O(1) best bid/ask access — highest buy via `rbegin()`, lowest sell via `begin()`
- O(1) order cancellation by ID via `unordered_map` index into price-level iterators
- All 10 order types (Market, Limit, Stop, Spot, Swap × Buy/Sell) route correctly using even/odd enum convention
- Trade execution condition checking for both securities and spot orders
- Lazy-initialized order book — SubBooks are created on demand per symbol
- Pure C++ standard library, no third-party dependencies
- 39-test suite covering routing, price priority, time priority, and cancellation

---

## Project Structure

```
TradingSystem/
├── TradingSystem.cpp      # Entry point — CSV parser and order factory
├── Order.cpp / .h         # Order value object with auto-increment ID
├── OrderType.h            # Order type enum with even/odd buy/sell convention
├── OrderBook.cpp / .h     # Central registry: unordered_map<symbol, SubBook> + cancel index
├── SubBook.cpp / .h       # Per-symbol PriceLevelMap containers (buy + sell)
├── OrderManager.cpp / .h  # Core business logic — order processing and cancellation
├── TradeManager.cpp / .h  # Trade execution condition evaluation
├── MarketPrice.cpp / .h   # Market price value object
├── MarketManager.cpp / .h # Market data stub (future integration)
├── tests.cpp              # Test suite (39 tests)
├── forex_orders.csv       # Sample input data
├── build                  # Build the main binary
├── build_tests            # Build and link the test binary
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
└────────────────────┬────────────────────┘
                     │
┌────────────────────▼────────────────────┐
│  OrderManager                           │  Business Logic
│  processNewOrder() · cancel()           │
│  TradeManager · MarketManager           │
└────────────────────┬────────────────────┘
                     │
┌────────────────────▼────────────────────┐
│  OrderBook                              │  Data Storage
│  SubBook (per symbol)                   │
└─────────────────────────────────────────┘
```

### OrderBook Structure

```
OrderBook
│
├── books: std::unordered_map<string, SubBook>   O(1) lookup
│   ├── "EUR/USD" ──► SubBook
│   │                 ├── buyOrders  ──► PriceLevelMap
│   │                 └── sellOrders ──► PriceLevelMap
│   ├── "GBP/USD" ──► SubBook
│   │                 ├── buyOrders  ──► PriceLevelMap
│   │                 └── sellOrders ──► PriceLevelMap
│   └── ...
│
└── orderIndex: std::unordered_map<long, OrderLocation>
    ├── id=1  ──► { priceMap*, price=1.0842, list::iterator }
    ├── id=26 ──► { priceMap*, price=1.0842, list::iterator }
    └── id=51 ──► { priceMap*, price=1.0835, list::iterator }
```

### PriceLevelMap Structure

`PriceLevelMap` is `std::map<double, std::list<Order>>` — a sorted map of price levels, each holding a list of orders at that price.

```
EUR/USD  buyOrders
─────────────────────────────────────────────────────
 price   │  orders at this level
─────────┼───────────────────────────────────────────
 1.0835  │  [ Order{id=51, qty=10789} ]
 1.0842  │  [ Order{id=1,  qty=9847} ── Order{id=26, qty=10123} ]
 1.0856  │  [ Order{id=76, qty=9912} ]
─────────────────────────────────────────────────────
  rbegin() ──► price=1.0856  (best bid — highest buy)

EUR/USD  sellOrders
─────────────────────────────────────────────────────
 1.0849  │  [ Order{id=76, qty=9912} ]
 1.0856  │  [ Order{id=26, qty=10123} ]
 1.0863  │  [ Order{id=52, qty=11045} ]
─────────────────────────────────────────────────────
  begin() ───► price=1.0849  (best ask — lowest sell)
```

### Cancellation Flow (O(1))

```
processCancelOrder(id=1)
        │
        ▼
orderIndex.find(1)
        │
        ▼  O(1) hash lookup
OrderLocation { priceMap=&buyOrders, price=1.0842, it }
        │
        ▼
buyOrders[1.0842]: [ Order{id=1} ── Order{id=26} ]
                     list::erase(it)   ← O(1) iterator removal
                   [ Order{id=26} ]

if list empty ──► priceMap->erase(1.0842)   clean up price level
orderIndex.erase(1)                          clean up index
```

### Trade Execution Logic

```
Buy order:  execute if order.price >= market price
Sell order: execute if order.price <= market price
```

See [ARCHITECTURE.md](ARCHITECTURE.md) for full component details, data flow diagrams, and design patterns.

---

## Order Types

| Value | Type         | Side |
|-------|--------------|------|
| 0     | MARKET_BUY   | Buy  |
| 1     | MARKET_SELL  | Sell |
| 2     | LIMIT_BUY    | Buy  |
| 3     | LIMIT_SELL   | Sell |
| 4     | STOP_BUY     | Buy  |
| 5     | STOP_SELL    | Sell |
| 6     | SPOT_BUY     | Buy  |
| 7     | SPOT_SELL    | Sell |
| 8     | SWAP_BUY     | Buy  |
| 9     | SWAP_SELL    | Sell |

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

**Build:**
```bash
g++ -fdiagnostics-color=always -g \
    Order.cpp OrderBook.cpp OrderManager.cpp SubBook.cpp \
    MarketPrice.cpp MarketManager.cpp TradeManager.cpp \
    TradingSystem.cpp -o trading_system
```

Or use the included build script:
```bash
./build
```

**Run:**
```bash
./trading_system
```

**Expected output:**
```
Processed order #1: BUY 9847 EUR/USD @ 1.0842
Processed order #2: SELL 10523 GBP/USD @ 1.2634
...
Total orders processed: 100
```

---

## Testing

The test suite lives in `tests.cpp` and uses a simple pass/fail framework with no external dependencies.

**Build and run:**
```bash
./build_tests
./run_tests
```

**Test sections (39 tests total):**

| Section | What is verified |
|---------|-----------------|
| Order Routing | All 10 order types route to the correct side — MARKET/LIMIT/STOP/SPOT/SWAP × Buy/Sell |
| Buy-Side Price Priority | `rbegin()` returns best bid (highest price); `begin()` returns lowest buy level |
| Sell-Side Price Priority | `begin()` returns best ask (lowest price); `rbegin()` returns highest sell level |
| Time Priority (FIFO) | Orders at the same price level maintain strict insertion order |
| Order Cancellation | Price level removed when last order cancelled; preserved on partial cancel; invalid ID leaves book unchanged |

---

## Current Status

This is an early-stage implementation. The following are stubs or not yet implemented:

- `MarketManager` — placeholder for live market data feeds
- Trade execution — conditions are checked but orders are not actually filled
- No order history or audit trail
- No persistence layer
- Single-threaded only

---

## Planned Features

- Full trade matching engine
- Real-time market data integration (WebSocket / FIX protocol)
- Order modification
- Trade and order history tracking
- Position and portfolio management
- Risk management and limits
- Database persistence
- Multi-threaded order processing
