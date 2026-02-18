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
- O(log n) order insertion via price-level map (`std::map<double, std::list<Order>>`)
- O(1) best bid/ask access — highest buy via `rbegin()`, lowest sell via `begin()`
- O(1) order cancellation by ID via `unordered_map` index into price-level iterators
- Supports 5 order types: Market, Limit, Stop, Spot, Swap (buy and sell variants)
- Trade execution condition checking for both securities and spot orders
- Lazy-initialized order book — SubBooks are created on demand per symbol
- Pure C++ standard library, no third-party dependencies

---

## Project Structure

```
TradingSystem/
├── TradingSystem.cpp    # Entry point — CSV parser and order factory
├── Order.cpp / .h       # Order value object
├── OrderType.h          # Order type enum with helper functions
├── OrderBook.cpp / .h   # Central order registry (map of symbol → SubBook)
├── SubBook.cpp / .h     # Per-symbol buy/sell order containers
├── OrderManager.cpp / .h# Core business logic — order processing and trade checks
├── TradeManager.cpp / .h# Trade execution condition evaluation
├── MarketPrice.cpp / .h # Market price value object
├── MarketManager.cpp / .h # Market data stub (future integration)
├── forex_orders.csv     # Sample input data
├── build                # Build script
└── ARCHITECTURE.md      # Detailed architecture documentation
```

---

## Architecture

The system is organized into three layers:

```
[ TradingSystem.cpp ]   ← Parses CSV, creates Orders
        │
        ▼
[ OrderManager ]        ← Processes orders, checks trade conditions
        │
        ▼
[ OrderBook ]           ← map<symbol, SubBook> + unordered_map<orderId, OrderLocation>
  [ SubBook ]           ← buyOrders (PriceLevelMap) + sellOrders (PriceLevelMap)
```

### Order Book Organization

Orders are stored per currency pair in a `SubBook`, each holding two `PriceLevelMap` structures (`std::map<double, std::list<Order>>`):

- **Buy orders** — keyed by price ascending; best bid accessed via `rbegin()` — O(1)
- **Sell orders** — keyed by price ascending; best ask accessed via `begin()` — O(1)
- **Insertion** — map insertion at the correct price level — O(log n)

### Cancellation Index

`OrderBook` maintains an `unordered_map<long, OrderLocation>` keyed by order ID. Each `OrderLocation` stores a pointer to the price-level map, the price key, and a `std::list<Order>::iterator` directly to the order. Cancellation uses `list::erase(iterator)` — O(1) — and cleans up empty price levels automatically.

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
g++ -fdiagnostics-color=always -g *.cpp -o trading_system
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
Total orders processed: 50
```

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
