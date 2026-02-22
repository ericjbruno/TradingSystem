# TradingSystem Architecture Document

## Overview

TradingSystem is a C++ forex trading system that processes orders, maintains a live order book with price-time priority, executes SPOT trades via a continuous matching engine, and notifies counterparties of fills. The system is designed around a clean layered architecture with no external dependencies.

## Project Structure

```
TradingSystem/
├── Source Files
│   ├── TradingSystem.cpp    # Main entry point — CSV parser, order factory, book display
│   ├── Counterparty.cpp     # Counterparty identity, order tracking, trade notifications
│   ├── Order.cpp            # Order implementation
│   ├── OrderManager.cpp     # Order processing — matching, queuing, cancellation
│   ├── OrderBook.cpp        # Order book storage and cancellation index
│   ├── SubBook.cpp          # Buy/sell order containers (BidMap + AskMap)
│   ├── TradeManager.cpp     # Matching engine, price logic, fill logging
│   ├── MarketPrice.cpp      # Market price data
│   └── MarketManager.cpp    # Market data management (stub)
│
├── Header Files
│   ├── Counterparty.h       # TradeNotification struct + Counterparty class
│   ├── Order.h
│   ├── OrderType.h          # Order type enum (even=buy, odd=sell)
│   ├── OrderManager.h
│   ├── OrderBook.h          # OrderLocation struct + OrderBook class
│   ├── SubBook.h            # BidMap and AskMap type aliases
│   ├── TradeManager.h       # Trade struct + TradeManager class
│   ├── MarketPrice.h
│   └── MarketManager.h
│
├── Data Files
│   └── forex_orders.csv     # Sample forex order data
│
└── Build Configuration
    └── .vscode/
        ├── tasks.json       # GCC compilation config
        └── launch.json      # Debug config
```

## Architecture Diagram

```
┌─────────────────────────────────────────────────────────────┐
│                      PRESENTATION LAYER                      │
│  ┌───────────────────────────────────────────────────────┐  │
│  │  TradingSystem.cpp (main)                             │  │
│  │  - CSV Parser                                         │  │
│  │  - Order Factory                                      │  │
│  │  - printOrderBook() ASCII display                     │  │
│  └───────────────────────────────────────────────────────┘  │
└────────────────────────────┬────────────────────────────────┘
                             │
                             ▼
┌─────────────────────────────────────────────────────────────┐
│                    BUSINESS LOGIC LAYER                      │
│  ┌─────────────────────┐    ┌─────────────────────────┐     │
│  │    OrderManager     │    │     MarketManager       │     │
│  │  - processNewOrder  │◄──►│  - Market data feeds    │     │
│  │  - processCancelOrder    │  - Price management     │     │
│  │  - queueOrder       │    │                         │     │
│  └──────────┬──────────┘    └─────────────────────────┘     │
│             │                                               │
│             ▼                                               │
│  ┌─────────────────────┐                                    │
│  │    TradeManager     │                                    │
│  │  - matchSpotOrders  │  ← matching engine lives here      │
│  │  - logAndNotify     │                                    │
│  │  - pricesMatch      │                                    │
│  └─────────────────────┘                                    │
└─────────────────────────────────────────────────────────────┘
                             │
                             ▼
┌─────────────────────────────────────────────────────────────┐
│                      DATA STORAGE LAYER                      │
│  ┌───────────────────────────────────────────────────────┐  │
│  │                     OrderBook                         │  │
│  │  unordered_map<Symbol, SubBook>   (O(1) lookup)      │  │
│  │  unordered_map<id, OrderLocation> (O(1) cancel)      │  │
│  │  ┌─────────────────────────────────────────────────┐  │  │
│  │  │  EUR/USD → SubBook                              │  │  │
│  │  │   ├── buyOrders:  BidMap (descending)           │  │  │
│  │  │   │               best bid = begin()  O(1)      │  │  │
│  │  │   └── sellOrders: AskMap (ascending)            │  │  │
│  │  │                   best ask = begin()  O(1)      │  │  │
│  │  └─────────────────────────────────────────────────┘  │  │
│  └───────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
                             │
                             ▼
┌─────────────────────────────────────────────────────────────┐
│                      VALUE OBJECTS                           │
│  ┌──────────────┐  ┌─────────────┐  ┌──────────────┐        │
│  │    Order     │  │ MarketPrice │  │  OrderType   │        │
│  │  - id        │  │  - symbol   │  │  (enum)      │        │
│  │  - symbol    │  │  - price    │  │  MARKET_BUY  │        │
│  │  - price     │  │  - quantity │  │  MARKET_SELL │        │
│  │  - quantity  │  │  - timestamp│  │  LIMIT_BUY   │        │
│  │  - type      │  │             │  │  LIMIT_SELL  │        │
│  │  - active    │  └─────────────┘  │  STOP_BUY    │        │
│  │  - cp*       │                   │  STOP_SELL   │        │
│  └──────┬───────┘                   │  SPOT_BUY    │        │
│         │ (non-owning)              │  SPOT_SELL   │        │
│         ▼                           │  SWAP_BUY    │        │
│  ┌─────────────────┐               │  SWAP_SELL   │        │
│  │  Counterparty   │               └──────────────┘        │
│  │  - id           │                                        │
│  │  - name         │  ┌─────────────────────────┐          │
│  │  - orderIds     │  │   TradeNotification      │          │
│  │  - trades       │◄─┤  - orderId               │          │
│  └─────────────────┘  │  - price / quantity      │          │
│                        │  - wasBuy                │          │
│                        │  - counterpartyName      │          │
│                        └─────────────────────────┘          │
└─────────────────────────────────────────────────────────────┘
```

## Core Components

### 1. Order

**Purpose:** Represents a single trading order.

**Attributes:**
| Attribute | Type | Description |
|-----------|------|-------------|
| id | long | Auto-increment ID (thread-safe `std::atomic<long>`) |
| symbol | string | Trading pair (e.g., "EUR/USD") |
| price | double | Order price |
| quantity | long | Number of units; reduced in-place by matching engine on partial fills |
| type | OrderType | Type of order (market, limit, etc.) |
| active | bool | Whether order is active |
| counterparty | `Counterparty*` | Non-owning observer pointer to the submitting counterparty |

**Key Methods:**
- `isBuyOrder()` / `isSellOrder()` — side classification via even/odd enum
- `setQuantity(qty)` — used by the matching engine to record partial fill remainders
- `getCounterparty()` — returns the non-owning counterparty pointer

### 2. Counterparty

**Purpose:** Represents a trading entity that submits orders. Tracks open order IDs and receives fill notifications.

**Attributes:**
| Attribute | Type | Description |
|-----------|------|-------------|
| id | long | Auto-increment ID (thread-safe `std::atomic<long>`) |
| name | string | Display name (e.g., "Goldman Sachs") |
| orderIds | `vector<long>` | IDs of currently open orders |
| trades | `vector<TradeNotification>` | Fill history for this counterparty |

**Key Methods:**
- `addOrderId(id)` — called by `OrderManager` after queuing an order
- `removeOrderId(id)` — called after cancellation or when the matching engine fully consumes a standing order
- `onTrade(TradeNotification)` — called by `TradeManager::logAndNotify()` on each fill
- `getOrderIds()` / `getTrades()` — read access to tracking state

**TradeNotification struct:**
```cpp
struct TradeNotification {
    long        orderId;           // this counterparty's order involved
    double      price;             // execution price
    long        quantity;          // fill quantity
    bool        wasBuy;            // true = this side was the buyer
    std::string counterpartyName;  // name of the other side
};
```

**Ownership model:** `Counterparty` objects are owned by the caller (e.g., `TradingSystem.cpp`). `Order` holds a non-owning raw pointer.

### 3. OrderType

**Purpose:** Enumeration defining supported order types.

```cpp
enum OrderType {
    MARKET_BUY  = 0,    MARKET_SELL = 1,
    LIMIT_BUY   = 2,    LIMIT_SELL  = 3,
    STOP_BUY    = 4,    STOP_SELL   = 5,
    SPOT_BUY    = 6,    SPOT_SELL   = 7,
    SWAP_BUY    = 8,    SWAP_SELL   = 9
};
```

**Design Note:** Even numbers = Buy, Odd numbers = Sell. `isBuyOrder()` / `isSellOrder()` rely on this convention.

### 4. SubBook

**Purpose:** Container for buy and sell orders of a single trading symbol.

**Type Aliases (defined in SubBook.h):**
```cpp
// Sell (ask) side: ascending — begin() == best ask (lowest price)
using AskMap = std::map<double, std::list<Order>>;

// Buy (bid) side: descending — begin() == best bid (highest price)
using BidMap = std::map<double, std::list<Order>, std::greater<double>>;
```

Both maps expose their best price at `begin()` in O(1), eliminating the asymmetry of the old single-type `PriceLevelMap` that required `rbegin()` for bids.

Each price level holds a `std::list<Order>` maintaining strict FIFO arrival order within that price.

### 5. OrderBook

**Purpose:** Central registry for all symbol order books.

**Attributes:**
- `books` — `std::unordered_map<string, SubBook>` — O(1) symbol lookup; lazily initializes SubBook on first access
- `orderIndex` — `std::unordered_map<long, OrderLocation>` — O(1) order ID → location for cancellation

**OrderLocation struct:**

Because `BidMap` and `AskMap` are different C++ types, a single raw map pointer cannot cover both. `OrderLocation` uses type erasure:

```cpp
struct OrderLocation {
    std::list<Order>*           priceList;   // pointer to the list at this price level
    double                      price;       // price level key in the map
    std::list<Order>::iterator  it;          // iterator to this order in the list
    std::function<void(double)> eraseLevel;  // removes the price level from its map
};
```

`eraseLevel` is a lambda captured at insert time that holds the map pointer by value. `cancel()` calls `eraseLevel(price)` when a price level becomes empty without needing to know whether it is a `BidMap` or `AskMap`.

**Key Methods:**
- `get(symbol)` — returns SubBook reference (lazy init via `operator[]`)
- `indexOrder(id, loc)` — registers an order in the cancellation index
- `cancel(id)` — removes order from price-level list and index in O(1)
- `removeFromIndex(id)` — strips the index entry only; used by the matching engine when it has already erased the order from the list itself
- `getOrderCounterparty(id)` — returns `Counterparty*` via index; must be called **before** `cancel()` since the order is gone after cancellation

### 6. TradeManager

**Purpose:** Contains all trade matching logic — price crossing check, matching engine execution, fill logging, and counterparty notification.

**Trade struct:**
```cpp
struct Trade {
    std::string   symbol;
    double        price;        // execution price (standing order's price)
    long          quantity;     // fill quantity
    long          buyOrderId;
    long          sellOrderId;
    Counterparty* buyer;        // non-owning
    Counterparty* seller;       // non-owning
};
```

**Key Methods:**
- `matchSpotOrders(Order& incoming, SubBook& sb, OrderBook& book)` — the matching engine (see Matching Engine section below)
- `logAndNotify(const Trade&)` — logs fill to stdout and calls `onTrade()` on both counterparties
- `pricesMatch(bid, ask)` — returns `bid >= ask`; used as the crossing condition

### 7. OrderManager

**Purpose:** Orchestrates order lifecycle — matching, queuing, and cancellation.

**Attributes:**
- `orderBook` (`std::unique_ptr<OrderBook>`) — owned order storage
- `tradeManager` (`std::unique_ptr<TradeManager>`) — owned matching engine
- `marketManager` (`MarketManager*`) — non-owning pointer to market data

**Key Methods:**
- `processNewOrder(Order)` — for SPOT orders: runs matching first, then queues any unfilled remainder; for all other types: queues directly
- `processCancelOrder(orderId)` — cancels existing order via O(1) index lookup
- `queueOrder(Order, SubBook&)` — private helper; inserts into bid/ask map, indexes for cancellation, notifies counterparty

### 8. MarketManager / MarketPrice

**Purpose:** Manages market data and pricing information.

**Status:** Stub implementation — placeholder for future market data integration.

---

## Matching Engine

### Overview

When a SPOT_BUY or SPOT_SELL order arrives, `OrderManager::processNewOrder` passes a mutable copy to `TradeManager::matchSpotOrders` before queuing. The engine walks the opposite side of the book from the best price inward, executing fills for as long as prices cross and quantity remains.

```
Incoming SPOT_BUY
    │
    ▼
matchSpotOrders(incoming, sb, book)
    │
    ├── Iterate AskMap from begin() (lowest ask = best ask)
    │     while incoming.qty > 0 AND ask.price <= incoming.price:
    │
    │     For each Order in price-level list (FIFO):
    │       fillQty = min(incoming.qty, standing.qty)
    │       logAndNotify(Trade{...})          ← log + notify both sides
    │       incoming.qty -= fillQty
    │
    │       if standing fully consumed:
    │         level.erase(it)                ← O(1) list erase
    │         book.removeFromIndex(id)       ← clean up cancel index
    │         cp->removeOrderId(id)          ← update counterparty
    │       else:
    │         standing.setQuantity(remaining) ← partial fill in-place
    │
    │       if level now empty → asks.erase(price)
    │
    └── return (incoming.qty == 0)
           true  → fully filled, do not queue
           false → queue remainder with reduced quantity
```

The same logic applies symmetrically for SPOT_SELL, iterating BidMap from `begin()` (highest bid).

### Iterator Safety

`std::list::erase(it)` only invalidates the erased node's iterator. All other iterators in the list (and the map) remain valid. The engine advances the iterator before erasing so the inner loop remains correct even when consuming multiple consecutive orders at the same price level.

### Execution Price

Trades execute at the **standing order's price** (price-time priority). An aggressive incoming order always gets the price that was resting in the book.

---

## Data Flow

### SPOT Order Processing Flow

```
1. CSV File / API input
   │
   ▼
2. Create Order object (same ID used throughout)
   │
   ▼
3. OrderManager::processNewOrder(order)
   │
   ▼
4. OrderBook::get(symbol)        ← lazy-init SubBook if needed
   │
   ▼
5. matchSpotOrders(mutableCopy, sb, *orderBook)
   │
   ├── [prices cross] → execute fills, notify counterparties
   │     fully filled → RETURN (do not queue)
   │     partially filled → continue with reduced qty
   │
   └── [no crossing price] → skip matching
   │
   ▼
6. queueOrder(order, sb)
   │  a. (*BidMap/AskMap)[price].push_back(order)   O(log n)
   │  b. orderIndex[id] = OrderLocation{list*, price, iter, eraseLevel}
   │  c. counterparty->addOrderId(id)
```

### Non-SPOT Order Processing Flow

```
processNewOrder(order)
    │
    └─► queueOrder(order, sb)   (steps 6a–6c above; no matching)
```

### Cancellation Flow

```
processCancelOrder(id)
    │
    ▼
getOrderCounterparty(id)          O(1) — read Counterparty* before erasure
    │
    ▼
OrderBook::cancel(id)
    │  a. orderIndex.find(id)     O(1) hash lookup → OrderLocation
    │  b. list::erase(it)         O(1) removal from price-level list
    │  c. if level empty → eraseLevel(price)   remove from BidMap or AskMap
    │  d. orderIndex.erase(id)
    │
    ▼
counterparty->removeOrderId(id)   update counterparty tracking
```

---

## Design Patterns

| Pattern | Component | Purpose |
|---------|-----------|---------|
| Repository | OrderBook | Centralized storage with lazy init |
| Composite | SubBook | Groups buy/sell orders under one symbol |
| Facade | OrderManager | Single entry point for order lifecycle |
| Strategy | OrderType | Type-based behavior (matching vs. queuing) |
| Observer (non-owning) | Counterparty ↔ Order | Order observes its counterparty via raw pointer |
| Type Erasure | OrderLocation.eraseLevel | Lambda hides BidMap/AskMap type difference |

---

## Dependencies

### Standard Library
- `<string>` — string handling
- `<vector>` — open order ID and trade notification lists in `Counterparty`
- `<list>` — order queues within each price level (stable iterators for O(1) cancel)
- `<map>` — `BidMap` (descending) and `AskMap` (ascending) — sorted for O(1) best bid/ask
- `<unordered_map>` — symbol lookup and cancellation index — O(1) average
- `<functional>` — `std::function<void(double)>` for type-erased `eraseLevel` lambda
- `<algorithm>` — `std::min` for fill quantity; `std::remove` for counterparty ID removal
- `<atomic>` — thread-safe ID generation for `Order` and `Counterparty`
- `<memory>` — `std::unique_ptr` ownership of `OrderBook` and `TradeManager`
- `<iostream>` — trade logging and console output
- `<fstream>` / `<sstream>` — CSV reading and parsing
- `<iomanip>` — price formatting in trade log output

### External Dependencies
- None — pure C++ standard library

---

## Build Configuration

### Compiler
- GCC (g++)
- Debug symbols enabled (`-g`)

### Build Commands

**Main binary:**
```bash
g++ -fdiagnostics-color=always -g \
    Counterparty.cpp Order.cpp OrderBook.cpp OrderManager.cpp SubBook.cpp \
    MarketPrice.cpp MarketManager.cpp TradeManager.cpp \
    TradingSystem.cpp -o TradingSystem
```

**Test binary (`tests.cpp` has its own `main` and is excluded from above):**
```bash
g++ -fdiagnostics-color=always -g \
    Counterparty.cpp Order.cpp OrderBook.cpp OrderManager.cpp SubBook.cpp \
    MarketPrice.cpp MarketManager.cpp TradeManager.cpp \
    tests.cpp -o run_tests
```

---

## Input Format

### CSV Structure
```csv
Symbol,Price,Quantity,Side
EUR/USD,1.0842,9847,BUY
GBP/USD,1.2634,10523,SELL
```

### Supported Currency Pairs
EUR, GBP, USD, JPY, CHF, AUD, CAD, NZD, SGD, HKD, CNY, MXN, ZAR combinations

---

## Current Limitations

1. **MarketManager** — stub implementation only; no live data feeds
2. **Non-SPOT matching** — MARKET, LIMIT, STOP, and SWAP orders are queued but not matched against each other
3. **Persistence** — no database integration; all state is in-memory
4. **Multi-threading** — single-threaded processing (ID generation is atomic-safe; book and index access are not)

---

## Future Development Areas

- Real-time market data integration (WebSocket / FIX protocol)
- Matching engine for LIMIT, STOP, and SWAP order types
- Order modification (price / quantity amendment)
- Position and portfolio tracking
- Fee / commission calculation
- Risk management and limits
- Database persistence layer
- Multi-threaded order processing
