# TradingSystem
### A C++ Forex Order Matching Engine with Live React UI

> **Language:** C++17 &nbsp;|&nbsp; **Frontend:** React + Vite + Zustand &nbsp;|&nbsp; **Protocol:** REST + Server-Sent Events &nbsp;|&nbsp; **Dependencies:** zero external C++ libraries

---

## Table of Contents

1. [Introduction](#introduction)
2. [Features](#features)
3. [Structure](#structure)
4. [Architecture](#architecture)
5. [Order Entry](#order-entry)
6. [Trading Engine](#trading-engine)
7. [User Interface](#user-interface)
8. [Scripts](#scripts)
9. [Input Format](#input-format)
10. [Testing](#testing)
11. [Current Status](#current-status)
12. [Core Lessons](#core-lessons)
13. [Conclusion](#conclusion)

---

## Introduction

**TradingSystem** is a fully functional, in-memory foreign exchange order matching engine written in modern C++17. It reads forex orders from a CSV file to seed the initial book state, then serves a REST API and SSE stream so a React UI can submit new orders, cancel existing ones, and display the live order book and trade feed in real time.

The system accepts buy and sell orders for currency pairs, maintains a live order book with strict price-time priority, executes SPOT trades automatically when bid and ask prices cross, and delivers real-time notifications to connected web clients via Server-Sent Events (SSE). When a trade executes, both counterparties are notified immediately, the fill is recorded in a recent-trade ring buffer, and every connected browser receives a live update within milliseconds — all without polling.

The project is designed around a clean layered architecture — presentation, business logic, and data storage — with **no external C++ dependencies** beyond a single-header HTTP library. The matching engine, order book, event bus, and HTTP server are all hand-coded in standard C++. The React front-end uses Vite, Zustand for state management, and plain CSS — no UI framework.

---

## Features

- **SPOT matching engine** — incoming SPOT orders are matched against the opposite side before queuing; supports partial fills, multi-level sweeps, and counterparty fill notifications
- **Price-time priority** — bids sorted highest-first (`BidMap`), asks sorted lowest-first (`AskMap`); `begin()` always returns the best price on both sides in O(1)
- **Per-symbol SubBook** — `BidMap` (`std::map` with `std::greater`) and `AskMap` (`std::map` default ascending) give each symbol an independent, correctly-ordered book
- **O(1) cancellation** — `OrderLocation` stores a `std::list` iterator and a type-erased `eraseLevel` lambda, enabling stable O(1) removal regardless of map type
- **Counterparty tracking** — each order carries a non-owning pointer to its counterparty; counterparties maintain a live list of open order IDs and a fill history (`vector<TradeNotification>`)
- **Trade logging** — every fill is printed to stdout with symbol, quantity, price, and both sides' names and order IDs
- **REST API** — `HTTPServer` (cpp-httplib) exposes endpoints to submit/cancel orders and query the book and trade history
- **Real-time SSE** — `EventBus` pub/sub pushes `trade` and `book_update` events to all connected clients immediately after each fill or order change
- **React UI** — dark terminal-style interface showing a live bid/ask ladder, scrolling trade feed, market summary with last-trade direction arrows, and order entry form; built with Vite + Zustand
- All 10 order types (Market, Limit, Stop, Spot, Swap × Buy/Sell) route correctly using the even/odd enum convention
- Lazy-initialized order book — SubBooks are created on demand per symbol
- 167-test suite (12 sections) covering routing, price priority, FIFO ordering, cancellation, counterparty tracking, SPOT matching, trade pricing, cascade fills, notification details, and price boundary conditions

---

## Structure

The project is split into three layers: a C++ backend, a React frontend, and a set of shell scripts and data files for running and testing the system.

### File Tree

```
TradingSystem/
├── TradingSystem.cpp      # Entry point — CSV parser, order factory, printOrderBook(), HTTP server startup
├── Counterparty.cpp / .h  # Counterparty identity, open-order tracking, TradeNotification
├── Order.cpp / .h         # Order value object with auto-increment ID and counterparty ref
├── OrderType.h            # Order type enum (even=buy, odd=sell)
├── OrderBook.cpp / .h     # Central registry: symbol→SubBook map + OrderLocation cancel index
├── SubBook.cpp / .h       # BidMap and AskMap type aliases + SubBook container
├── OrderManager.cpp / .h  # Order lifecycle — matching, queuing, cancellation, book-update SSE events
├── TradeManager.cpp / .h  # Matching engine, fill logging, trade SSE events, recent trade history
├── EventBus.cpp / .h      # Thread-safe pub/sub for SSE streaming
├── HTTPServer.cpp / .h    # REST API + SSE /events endpoint (cpp-httplib)
├── httplib.h              # cpp-httplib single-header HTTP library (third-party)
├── MarketPrice.cpp / .h   # Market price value object
├── MarketManager.cpp / .h # Market data stub (future integration)
├── tests.cpp              # Test suite (167 tests across 12 sections)
├── forex_orders.csv       # 100 sample forex orders; 24 result in trades
├── run_server.sh          # Build C++ binary and start HTTP server on :9090
├── run_ui.sh              # Install npm deps (if needed) and start Vite dev server on :5173
├── demo_trade.sh          # Submit a single bid/offer pair resulting in a trade
├── demo_rising_price.sh   # Submit two trades at rising prices to show ▲ direction arrow
├── run_orders_loop.sh     # Stress-test loop: submit all CSV orders, cancel, repeat
├── ui/                    # React UI (Vite + Zustand)
│   ├── src/App.jsx        # Bootstrap, SSE setup, layout
│   ├── src/store/         # Zustand state (books, trades, symbols, connected)
│   └── src/components/    # OrderBook ladder, TradeFeed, OrderForm, MarketSummary, SymbolSelector
├── ARCHITECTURE.md        # Detailed architecture documentation with Mermaid sequence diagrams
└── DESCRIPTION.md         # This file
```

### C++ Backend

The backend is organised around ten classes, each with a focused responsibility:

#### `Order`

Represents a single resting or incoming order. Holds the trading symbol, price, quantity, order type, and a non-owning pointer to the counterparty that submitted it. A static `std::atomic<long>` counter provides thread-safe, auto-incrementing order IDs.

```cpp
class Order {
    long          id;
    bool          active;
    double        price;
    long          quantity;
    std::string   symbol;
    OrderType     type;
    Counterparty* counterparty;   // non-owning pointer
    static std::atomic<long> nextId;
public:
    long getId()          const;
    bool isBuyOrder()     const;
    bool isSellOrder()    const;
    void setQuantity(long qty);
    // ...
};
```

#### `OrderType`

An enum whose even values are buy-side types and odd values are sell-side types. `isBuyOrder()` and `isSellOrder()` exploit this arithmetic convention with a single modulo test, making side classification branch-free.

```cpp
enum class OrderType {
    MARKET_BUY = 0,  MARKET_SELL = 1,
    LIMIT_BUY  = 2,  LIMIT_SELL  = 3,
    STOP_BUY   = 4,  STOP_SELL   = 5,
    SPOT_BUY   = 6,  SPOT_SELL   = 7,
    SWAP_BUY   = 8,  SWAP_SELL   = 9,
};

inline bool isBuyOrder(OrderType type) {
    return static_cast<int>(type) % 2 == 0;
}
```

Only `SPOT_BUY` and `SPOT_SELL` are currently matched against the opposite side. All other types are queued immediately without matching.

| Value | Type | Side | Matching |
|---|---|---|---|
| 0 | `MARKET_BUY` | Buy | queued only |
| 1 | `MARKET_SELL` | Sell | queued only |
| 2 | `LIMIT_BUY` | Buy | queued only |
| 3 | `LIMIT_SELL` | Sell | queued only |
| 4 | `STOP_BUY` | Buy | queued only |
| 5 | `STOP_SELL` | Sell | queued only |
| **6** | **`SPOT_BUY`** | **Buy** | **matched + queued** |
| **7** | **`SPOT_SELL`** | **Sell** | **matched + queued** |
| 8 | `SWAP_BUY` | Buy | queued only |
| 9 | `SWAP_SELL` | Sell | queued only |

#### `Counterparty`

Represents a trading firm. Tracks its open order IDs and receives `TradeNotification` structs when its orders fill. Counterparty objects are owned by the caller; `Order` holds only a raw, non-owning pointer.

```cpp
struct TradeNotification {
    long        orderId;
    double      price;
    long        quantity;
    bool        wasBuy;            // true = this side was the buyer
    std::string counterpartyName;
};

class Counterparty {
    long id;
    std::string name;
    std::vector<long> orderIds;
    std::vector<TradeNotification> trades;
    static std::atomic<long> nextId;
public:
    void addOrderId(long orderId);
    void removeOrderId(long orderId);
    void onTrade(TradeNotification notification);
};
```

#### `SubBook`

The per-symbol order container. Holds two sorted maps — a `BidMap` (descending, so `begin()` is the highest bid) and an `AskMap` (ascending, so `begin()` is the lowest ask). Each price level stores a `std::list<Order>` in strict FIFO arrival order.

```cpp
// Sell (ask) map: ascending — begin() == best ask (lowest price)
using AskMap = std::map<double, std::list<Order>>;

// Buy (bid) map: descending — begin() == best bid (highest price)
using BidMap = std::map<double, std::list<Order>, std::greater<double>>;

class SubBook {
    BidMap buyOrders;
    AskMap sellOrders;
public:
    BidMap& getBuyOrdersRef()  { return buyOrders; }
    AskMap& getSellOrdersRef() { return sellOrders; }
};
```

#### `OrderBook`

The top-level registry. Maps trading symbols to `SubBook`s via `std::unordered_map` for O(1) symbol lookup, and maintains a second `unordered_map` from order ID to `OrderLocation` for O(1) cancellation without scanning the book.

```cpp
struct OrderLocation {
    std::list<Order>*           priceList;   // pointer to the level's list
    double                      price;       // key in the parent map
    std::list<Order>::iterator  it;          // direct iterator to the order
    std::function<void(double)> eraseLevel;  // type-erased map erase call
};

class OrderBook {
    std::unordered_map<std::string, SubBook> books;       // O(1) symbol lookup
    std::unordered_map<long, OrderLocation>  orderIndex;  // O(1) cancel lookup
public:
    SubBook& get(const std::string& symbol);
    bool     cancel(long orderId);
    void     indexOrder(long orderId, OrderLocation loc);
    void     removeFromIndex(long orderId);
};
```

#### `TradeManager`

Contains all matching logic. `matchSpotOrders` walks the opposite side of the book from the best price inward, executing fills for as long as prices cross and quantity remains. `logAndNotify` records each fill, publishes an SSE `trade` event, and calls `onTrade()` on both counterparties.

#### `OrderManager`

Orchestrates the order lifecycle. For SPOT orders it calls the matching engine first, then queues any unfilled remainder. For all other order types it queues immediately. After every state change it publishes a `book_update` SSE event.

#### `EventBus`

A thread-safe publish/subscribe bus. Each SSE connection receives a `shared_ptr<Connection>` that owns a message queue, a mutex, and a condition variable. `publish()` pushes a message to every live connection and wakes their waiting threads.

```cpp
class EventBus {
public:
    struct Connection {
        std::queue<std::string> queue;
        std::mutex              mu;
        std::condition_variable cv;
        bool                    closed{false};
    };
    std::shared_ptr<Connection> subscribe();
    void unsubscribe(std::shared_ptr<Connection> conn);
    void publish(const std::string& sseMsg);   // must end with "\n\n"
private:
    std::vector<std::shared_ptr<Connection>> conns_;
    std::mutex                               mu_;
};
```

#### `HTTPServer`

Wraps **cpp-httplib** to expose the REST API and the SSE `/events` endpoint. All `OrderManager` access is serialised through a single mutex. The SSE handler blocks on its connection's condition variable without holding that mutex, so live streams never stall incoming REST requests.

#### `MarketManager`

A stub placeholder for a future live market data feed.

---

### React Frontend (`ui/`)

The UI is a single-page React application built with Vite:

| File | Role |
|---|---|
| `App.jsx` | Top-level component. Fetches initial state on mount, opens the SSE `EventSource`, distributes live data as props. |
| `tradingStore.js` | Zustand store holding `books`, `trades`, `symbols`, and connection state. |
| `MarketSummary.jsx` | Horizontal summary grid. Best bid, best ask, quantity, last trade price with direction arrow ▲▼, and time of last trade per symbol. Cells flash gold on change. |
| `OrderBook.jsx` | Price ladder showing all bid and ask levels for the selected symbol. |
| `TradeFeed.jsx` | Scrolling log of recent fills. |
| `OrderForm.jsx` | Form for submitting new orders and cancelling by ID. |

### Shell Scripts and Data

| File | Purpose |
|---|---|
| `run_server.sh` | Compiles the C++ binary and starts the HTTP server. |
| `run_ui.sh` | Installs npm dependencies and starts the Vite dev server. |
| `run_orders_loop.sh` | Submits all 100 CSV orders via REST in a loop, cancelling all open orders between iterations. |
| `demo_trade.sh` | Submits a single bid/offer pair that results in a trade. |
| `demo_rising_price.sh` | Submits two consecutive trades at increasing prices, demonstrating the ▲ direction arrow in the UI. |
| `forex_orders.csv` | 100 sample forex orders across 25 currency pairs. |

---

## Architecture

The central design decision is the **separation of the order book data structure from the matching logic**. The book exists purely to store and retrieve orders efficiently; the matching engine operates on it from the outside.

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

### Order Book Data Structures

Each symbol has its own `SubBook` containing two sorted maps:

```
BidMap — std::map<double, std::list<Order>, std::greater<double>>
AskMap — std::map<double, std::list<Order>>
```

Using `std::greater` for bids means the map's `begin()` iterator always points to the **highest bid**. The `AskMap`'s `begin()` always points to the **lowest ask**. In both cases the best price is available in O(1) without any search or reverse iteration.

Within each price level, orders are stored in a `std::list<Order>`. `std::list` provides O(1) insertion at the back (preserving arrival order) and, critically, O(1) erasure of any node given its iterator — without invalidating any other iterator in the list. This property is essential for cancellation.

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

### O(1) Cancellation via `OrderLocation`

When an order is queued, an `OrderLocation` struct is created and stored in a second `unordered_map<long, OrderLocation>` keyed by order ID. Cancelling an order is then four steps, each O(1):

1. Hash-map lookup of the `OrderLocation` by order ID
2. `list::erase(it)` to remove the order node
3. If the list is now empty, call `eraseLevel(price)` to remove the price-level entry from the map
4. Erase the `OrderLocation` from the index

```cpp
bool OrderBook::cancel(long orderId) {
    auto indexIt = orderIndex.find(orderId);
    if (indexIt == orderIndex.end()) return false;

    OrderLocation& location = indexIt->second;
    location.priceList->erase(location.it);       // O(1) list erasure

    if (location.priceList->empty())
        location.eraseLevel(location.price);      // remove empty price level

    orderIndex.erase(indexIt);
    return true;
}
```

The `eraseLevel` field is a lambda captured at insert time. Because `BidMap` and `AskMap` are different C++ types, a single raw pointer cannot erase from both. The lambda closes over the map by reference and hides the type difference behind a `std::function<void(double)>`. This is a deliberate use of **type erasure** to keep `OrderLocation` generic.

### Event Flow

State changes flow outward via the `EventBus`. After every order queue or cancel, `OrderManager` serialises the current `SubBook` to JSON and calls `EventBus::publish("event: book_update\ndata: {...}\n\n")`. After every fill, `TradeManager` calls `EventBus::publish("event: trade\ndata: {...}\n\n")`.

```cpp
void EventBus::publish(const std::string& sseMsg) {
    std::lock_guard<std::mutex> lk(mu_);
    for (auto& c : conns_) {
        std::lock_guard<std::mutex> clk(c->mu);
        c->queue.push(sseMsg);
        c->cv.notify_one();   // wake the waiting SSE handler thread
    }
}
```

### Thread Safety

The HTTP server uses a single coarse-grained mutex around all `OrderManager` calls. The SSE handler acquires only the **per-connection** mutex while draining its queue, not the global mutex, so it never blocks order processing.

---

## Order Entry

The journey of an order from submission to the book passes through four layers.

### 1. HTTP Receipt

The React UI posts JSON to `POST /orders`. `HTTPServer` parses the body with simple string-search extractors (no JSON library dependency), resolves the named counterparty from an internal map, constructs an `Order` object, and calls `OrderManager::processNewOrder()` under the global mutex.

```cpp
// From HTTPServer::setupRoutes()
svr_.Post("/orders", [this](const httplib::Request& req, httplib::Response& res) {
    std::string symbol   = extractStr(body, "symbol");
    double      price    = extractDouble(body, "price");
    long        quantity = extractLong(body, "quantity");
    std::string side     = extractStr(body, "side");
    std::string cpName   = extractStr(body, "counterparty");

    OrderType orderType = (side == "BUY") ? OrderType::SPOT_BUY
                                          : OrderType::SPOT_SELL;
    Order order(symbol, price, quantity, orderType, cp);

    std::lock_guard<std::mutex> lk(mu_);
    om_.processNewOrder(order);
});
```

### 2. Matching Attempt

For `SPOT_BUY` and `SPOT_SELL` orders, `processNewOrder` immediately calls `TradeManager::matchSpotOrders()` with a mutable copy of the order. If the order is fully filled, `matchSpotOrders` returns `true` and the order is not queued.

```cpp
void OrderManager::processNewOrder(const Order& newOrder) {
    SubBook& sb = orderBook->get(newOrder.getSymbol());

    if (newOrder.getType() == OrderType::SPOT_BUY ||
        newOrder.getType() == OrderType::SPOT_SELL) {

        Order order = newOrder;  // mutable copy — matching decrements quantity

        if (tradeManager->matchSpotOrders(order, sb, *orderBook)) {
            publishBookUpdate(sym);
            return;              // fully filled — nothing left to queue
        }

        queueOrder(order, sb);  // queue the unfilled remainder
        publishBookUpdate(sym);
        return;
    }

    queueOrder(newOrder, sb);   // non-SPOT orders: queue directly, no matching
    publishBookUpdate(sym);
}
```

### 3. Queuing

If any quantity remains after matching (or the order type is non-SPOT), `queueOrder` inserts it into the appropriate price-level list and registers it in the cancellation index:

```cpp
void OrderManager::queueOrder(const Order& order, SubBook& sb) {
    std::list<Order>*           priceListPtr;
    std::function<void(double)> eraseLevel;

    if (order.isBuyOrder()) {
        BidMap* bids = &sb.getBuyOrdersRef();
        auto&   pl   = (*bids)[order.getPrice()];  // O(log n), or O(1) if level exists
        pl.push_back(order);
        priceListPtr = &pl;
        eraseLevel   = [bids](double price) { bids->erase(price); };
    } else {
        AskMap* asks = &sb.getSellOrdersRef();
        auto&   pl   = (*asks)[order.getPrice()];
        pl.push_back(order);
        priceListPtr = &pl;
        eraseLevel   = [asks](double price) { asks->erase(price); };
    }

    auto it = std::prev(priceListPtr->end());
    orderBook->indexOrder(order.getId(),
                          {priceListPtr, order.getPrice(), it, eraseLevel});

    if (Counterparty* cp = order.getCounterparty())
        cp->addOrderId(order.getId());
}
```

### 4. Book Update Event

After queuing (or after a full fill), `publishBookUpdate` serialises the entire `SubBook` to JSON and pushes it to every connected browser via SSE:

```cpp
void OrderManager::publishBookUpdate(const std::string& symbol) {
    SubBook& sb = orderBook->get(symbol);
    std::ostringstream j;
    j << "event: book_update\ndata: {\"symbol\":\"" << symbol << "\",\"bids\":[";
    appendPriceLevels(j, sb.getBuyOrdersRef());
    j << "],\"asks\":[";
    appendPriceLevels(j, sb.getSellOrdersRef());
    j << "]}\n\n";
    eventBus_->publish(j.str());
}
```

### Order Entry Sequence (no matching trade)

```text
  Client           HTTPServer        OrderBook         TradeManager       EventBus
    |                   |                |                   |                |
    |  POST /orders     |                |                   |                |
    |------------------>|                |                   |                |
    |                   |  addOrder()    |                   |                |
    |                   |--------------->|                   |                |
    |                   |                |  matchSpotOrders()|                |
    |                   |                |------------------>|                |
    |                   |                |                   | (no match)     |
    |                   |                |<------------------|                |
    |                   |                |  queue in SubBook |                |
    |                   |                |  update index     |                |
    |                   |                |  publishBookUpdate|                |
    |                   |                |---------------------------------->|
    |                   |                |                   |  SSE: book_update
    |                   |                |                   |                |---> browsers
    |  201 Created      |                |                   |                |
    |<------------------|                |                   |                |
```

### Order Cancellation

Cancellation follows a simpler path: `getOrderSymbol` and `getOrderCounterparty` read from the existing index entry before erasure, `OrderBook::cancel` removes the order in O(1), `removeOrderId` cleans up the counterparty, and another `book_update` event is published.

```cpp
void OrderManager::processCancelOrder(long orderId) {
    const std::string sym = orderBook->getOrderSymbol(orderId);
    Counterparty* cp      = orderBook->getOrderCounterparty(orderId);

    if (!orderBook->cancel(orderId)) {
        std::cerr << "Cancel failed: order " << orderId << " not found\n";
        return;
    }

    if (cp) cp->removeOrderId(orderId);
    if (!sym.empty()) publishBookUpdate(sym);
}
```

### Order Cancellation Sequence

```text
  Client           HTTPServer        OrderBook         Counterparty       EventBus
    |                   |                |                   |                |
    | DELETE /orders/id |                |                   |                |
    |------------------>|                |                   |                |
    |                   |  cancel(id)    |                   |                |
    |                   |--------------->|                   |                |
    |                   |                | look up index     |                |
    |                   |                | erase from list   |                |
    |                   |                | removeFromIndex() |                |
    |                   |                | removeOrderId()   |                |
    |                   |                |------------------>|                |
    |                   |                |                   |                |
    |                   |                | publishBookUpdate |                |
    |                   |                |---------------------------------->|
    |                   |                |                   |  SSE: book_update
    |                   |                |                   |                |---> browsers
    |  200 OK           |                |                   |                |
    |<------------------|                |                   |                |
```

---

## Trading Engine

The matching algorithm in `TradeManager::matchSpotOrders` implements **continuous, price-time priority matching** — the same fundamental model used in real foreign exchange markets.

### Price Crossing

A trade occurs when a buyer's limit price is at or above a seller's limit price. This inclusive condition means orders at exactly equal prices always execute.

```cpp
bool TradeManager::pricesMatch(double bidPrice, double askPrice) {
    return bidPrice >= askPrice;
}
```

### Walk from Best Price Inward

| Incoming side | Book side scanned | Start of iteration | Early exit condition |
|---|---|---|---|
| `SPOT_BUY` | `AskMap` (sells) | `begin()` = lowest ask | best ask > bid limit |
| `SPOT_SELL` | `BidMap` (buys) | `begin()` = highest bid | best bid < ask limit |

Within each price level, orders fill in strict **FIFO order** — the oldest resting order fills first.

Example matching flow for an incoming buy:

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

The outer matching loop for an incoming buy:

```cpp
AskMap& asks = sb.getSellOrdersRef();
auto    mapIt = asks.begin();

while (mapIt != asks.end() && incoming.getQuantity() > 0) {
    double askPrice = mapIt->first;
    if (!pricesMatch(incoming.getPrice(), askPrice)) break;  // no more fills possible

    std::list<Order>& level   = mapIt->second;
    auto              orderIt = level.begin();

    while (orderIt != level.end() && incoming.getQuantity() > 0) {
        Order& standing = *orderIt;
        long   fillQty  = std::min(incoming.getQuantity(), standing.getQuantity());

        Trade trade{ incoming.getSymbol(), askPrice, fillQty,
                     incoming.getId(), standing.getId(),
                     incoming.getCounterparty(), standing.getCounterparty() };
        logAndNotify(trade);

        incoming.setQuantity(incoming.getQuantity() - fillQty);

        if (fillQty == standing.getQuantity()) {
            // Standing order fully consumed — erase and advance
            long          standingId = standing.getId();
            Counterparty* cp         = standing.getCounterparty();
            orderIt = level.erase(orderIt);   // erase() returns next valid iterator
            book.removeFromIndex(standingId);
            if (cp) cp->removeOrderId(standingId);
        } else {
            // Standing order partially consumed — reduce quantity in-place
            standing.setQuantity(standing.getQuantity() - fillQty);
            ++orderIt;
        }
    }

    auto nextMapIt = std::next(mapIt);
    if (level.empty()) asks.erase(mapIt);  // remove now-empty price level
    mapIt = nextMapIt;
}
```

### Execution Price

Every trade executes at the **standing (resting) order's price**, not the incoming order's price. This is the maker/taker model: the order already in the book set the price; the incoming order accepts it.

### Partial and Cascade Fills

| Scenario | What happens |
|---|---|
| **Partial fill** (standing partially consumed) | `standing.setQuantity(remaining)` — in-place update, iterator and `OrderLocation` remain valid |
| **Full fill** (standing fully consumed) | `level.erase(orderIt)` returns next iterator; index and counterparty entries are cleaned up |
| **Empty price level** | `asks.erase(mapIt)` removes the map entry entirely |
| **Cascade** (incoming sweeps multiple levels) | The outer loop continues across price levels until quantity reaches zero or no crossing prices remain |

> The return value of `matchSpotOrders` is simply `incoming.getQuantity() == 0`. If `true`, the caller skips queuing. If `false`, the reduced-quantity remainder rests at its original limit price.

### Logging and Notification

After each fill, `logAndNotify` records the trade in a ring buffer, publishes an SSE `trade` event to all browsers, and calls `onTrade()` on both counterparties:

```cpp
void TradeManager::logAndNotify(const Trade& trade) {
    // stdout log
    std::cout << "[TRADE] " << trade.symbol
              << "  qty="   << trade.quantity
              << "  @ "     << trade.price
              << "  | Buy#" << trade.buyOrderId
              << " ("       << trade.buyer->getName()  << ")"
              << "  Sell#"  << trade.sellOrderId
              << " ("       << trade.seller->getName() << ")\n";

    // Ring buffer: newest at back, capped at 100 entries
    recentTrades_.push_back(trade);
    if (recentTrades_.size() > 100) recentTrades_.pop_front();

    // Publish SSE event to all connected browsers
    if (eventBus_) {
        std::ostringstream j;
        j << "event: trade\ndata: "
          << "{\"symbol\":\""  << trade.symbol              << "\""
          << ",\"price\":"     << trade.price
          << ",\"quantity\":"  << trade.quantity
          << ",\"buyer\":\""   << trade.buyer->getName()    << "\""
          << ",\"seller\":\""  << trade.seller->getName()   << "\""
          << "}\n\n";
        eventBus_->publish(j.str());
    }

    // Notify each counterparty
    trade.buyer->onTrade({trade.buyOrderId,  trade.price, trade.quantity,
                          true,  trade.seller->getName()});
    trade.seller->onTrade({trade.sellOrderId, trade.price, trade.quantity,
                          false, trade.buyer->getName()});
}
```

### Trade Execution Sequence

```text
  Client           HTTPServer        OrderBook         TradeManager       EventBus
    |                   |                |                   |                |
    |  POST /orders     |                |                   |                |
    |  (SPOT_BUY)       |                |                   |                |
    |------------------>|                |                   |                |
    |                   |  addOrder()    |                   |                |
    |                   |--------------->|                   |                |
    |                   |                | matchSpotOrders() |                |
    |                   |                |------------------>|                |
    |                   |                |                   |                |
    |                   |                |                   | walk AskMap    |
    |                   |                |                   | pricesMatch()  |
    |                   |                |                   | fillQty = min  |
    |                   |                |                   |   (bid,ask qty)|
    |                   |                |                   |                |
    |                   |                |     [repeat for each fill]         |
    |                   |                |                   | logAndNotify() |
    |                   |                |                   |--------------->|
    |                   |                |                   |  SSE: trade    |
    |                   |                |                   |                |---> browsers
    |                   |                |                   | buyer.onTrade()|
    |                   |                |                   | seller.onTrade()|
    |                   |                |                   |                |
    |                   |                |<------------------|                |
    |                   |                | (fully filled: do not queue)       |
    |                   |                | publishBookUpdate |                |
    |                   |                |---------------------------------->|
    |                   |                |                   |  SSE: book_update
    |                   |                |                   |                |---> browsers
    |  201 Created      |                |                   |                |
    |<------------------|                |                   |                |
```

---

## User Interface

The React UI is a single-page application served by Vite. It connects to the C++ server over REST and SSE.

### Layout

The page is a CSS grid with four rows: a fixed header bar, a scrollable market summary panel that spans full width, a two-column main area (order book on the left, trade feed on the right), and a fixed order-entry footer.

### Real-Time Updates

`App.jsx` opens a single `EventSource` to `GET /events` on mount. Incoming SSE events are routed to the Zustand store:

| SSE Event | Store action | Effect |
|---|---|---|
| `book_update` | `updateBook(parsed)` | Updates `books[symbol]` |
| `trade` | `addTrade(parsed)` | Prepends to `trades[]`, capped at 100 |

```jsx
// From App.jsx — dynamic hostname works locally and on the LAN
const API = `http://${window.location.hostname}:9090`

const es = new EventSource(`${API}/events`)
es.onopen  = () => setConnected(true)
es.onerror = () => setConnected(false)

es.addEventListener('trade',       e => addTrade(JSON.parse(e.data)))
es.addEventListener('book_update', e => updateBook(JSON.parse(e.data)))
```

All child components receive data as **props from App** rather than subscribing to the store independently. This ensures every component re-renders in the same React cycle when an event arrives, preventing visual inconsistency.

### Market Summary

The `MarketSummary` component tracks per-symbol best bid, best ask, quantity, last trade price, and time of last trade. It uses two `useRef` objects to avoid stale-closure issues:

- **`prevRef`** — stores the previous values of all tracked fields for each symbol; updated inside `useEffect` after renders, not during them
- **`timersRef`** — stores `setTimeout` handles for gold-flash expiry, one per cell key

When a value changes, `triggerFlash(key)` fires:

```jsx
function triggerFlash(key) {
    if (timersRef.current[key]) clearTimeout(timersRef.current[key])
    setFlashing(f => ({ ...f, [key]: true }))          // instant gold
    timersRef.current[key] = setTimeout(() => {
        setFlashing(f => { const n = { ...f }; delete n[key]; return n })
        delete timersRef.current[key]
    }, 2000)                                            // fade after 2 s
}
```

#### Direction Arrow (▲ / ▼)

The direction arrow is computed by comparing the current last trade price against `prevLastTrade` — the price from the _previous_ render. This value is stored separately from `lastTrade` in `prevRef` so the render always has access to the prior price even after the `useEffect` has overwritten it.

```jsx
// In the trades useEffect — save old price BEFORE overwriting lastTrade
useEffect(() => {
    const latest = buildLastTrades(trades)
    for (const sym of symbols) {
        const price = latest[sym]
        const prev  = prevRef.current[sym] || {}
        if (price !== undefined && price !== prev.lastTrade) {
            triggerFlash(`${sym}.last`)
            prevRef.current[sym] = {
                ...prevRef.current[sym],
                prevLastTrade: prev.lastTrade,  // KEY: preserve old value for render
                lastTrade: price,
            }
            setTradeTimes(t => ({ ...t, [sym]: Date.now() }))
        }
    }
}, [trades, symbols])

// In render — use prevLastTrade (not lastTrade) to determine direction
const prevLast = prevRef.current[sym]?.prevLastTrade
let arrow = null
if (lastPrice != null && prevLast != null && lastPrice !== prevLast) {
    arrow = lastPrice > prevLast
        ? <span className="ms-arrow up">▲</span>
        : <span className="ms-arrow down">▼</span>
}
```

### SSE Handler (C++ side)

The `/events` route uses cpp-httplib's chunked content provider. The lambda blocks on the connection's condition variable for up to 20 seconds, draining the queue whenever `publish()` wakes it. If 20 seconds elapse with no event, a keepalive comment is sent to prevent proxy timeouts.

```cpp
svr_.Get("/events", [this](const httplib::Request&, httplib::Response& res) {
    auto conn = bus_.subscribe();
    res.set_chunked_content_provider(
        "text/event-stream",
        [conn](size_t, httplib::DataSink& sink) -> bool {
            std::unique_lock<std::mutex> lk(conn->mu);
            conn->cv.wait_for(lk, std::chrono::seconds(20), [&] {
                return !conn->queue.empty() || conn->closed;
            });

            if (conn->closed) { sink.done(); return false; }

            if (conn->queue.empty()) {
                // 20 s elapsed with no event — send keepalive
                const std::string ka = ": keepalive\n\n";
                return sink.write(ka.data(), ka.size());
            }

            while (!conn->queue.empty()) {
                const std::string& msg = conn->queue.front();
                if (!sink.write(msg.data(), msg.size())) {
                    conn->closed = true;
                    return false;
                }
                conn->queue.pop();
            }
            return true;
        },
        [this, conn](bool) { bus_.unsubscribe(conn); }
    );
});
```

### Clear Button

The Clear button cancels all open orders sequentially (parallel requests overwhelm the server's thread pool), then resets the UI state:

```jsx
async function handleClear() {
    // Up to 3 passes — catches orders added mid-cancel
    for (let pass = 0; pass < 3; pass++) {
        const allBooks = await fetch(`${API}/books`).then(r => r.json())
        const ids = Object.values(allBooks).flatMap(b => [
            ...(b.bids || []).flatMap(level => level.orderIds || []),
            ...(b.asks || []).flatMap(level => level.orderIds || []),
        ])
        if (ids.length === 0) break
        for (const id of ids) {
            await fetch(`${API}/orders/${id}`, { method: 'DELETE' })
        }
    }
    clearTrades()          // clear Zustand store
    setClearKey(k => k + 1)  // signal MarketSummary to reset internal state
}
```

---

## Scripts

Five shell scripts provide ready-made ways to start, exercise, and demonstrate the system.

### `run_server.sh` — Build & Start

Compiles all C++ source files with `g++ -std=c++17` and immediately runs the resulting binary. A single command builds and starts the server on port 9090.

```bash
#!/usr/bin/env bash
set -e
g++ -std=c++17 -fdiagnostics-color=always -g \
    Counterparty.cpp Order.cpp OrderBook.cpp OrderManager.cpp SubBook.cpp \
    MarketPrice.cpp MarketManager.cpp TradeManager.cpp \
    EventBus.cpp HTTPServer.cpp TradingSystem.cpp \
    -lpthread -o TradingSystem
./TradingSystem
```

**Expected server output (excerpt):**

```
Processed order #1: BUY 9847 EUR/USD @ 1.0842  [Goldman Sachs]
[TRADE] EUR/USD  qty=500  @ 1.0842  | Buy#3 (Deutsche Bank)  Sell#1 (Goldman Sachs)
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

HTTP server listening on http://localhost:9090
```

### `run_ui.sh` — Start Frontend

Installs npm dependencies if `node_modules` is absent (first-run bootstrap), then starts the Vite development server. The UI is accessible locally at `http://localhost:5173` and from other machines on the LAN at `http://<host-ip>:5173`.

```bash
#!/usr/bin/env bash
set -e
cd "$(dirname "$0")/ui"
if [ ! -d node_modules ]; then
    npm install
fi
npm run dev
```

### Quick Start

```bash
# Terminal 1 — build C++ and start HTTP server on :9090
./run_server.sh

# Terminal 2 — start React dev server on :5173
./run_ui.sh
# Open http://localhost:5173
```

### `demo_trade.sh`

Demonstrates the simplest possible trade: one buyer, one seller, equal price, equal quantity. Goldman Sachs bids 1,000,000 EUR/USD @ 1.08500 and rests in the book. Three seconds later JP Morgan offers at the same price; the bid and ask cross and the matching engine executes the fill.

```bash
#!/usr/bin/env bash
API="http://localhost:9090"

echo "Step 1: Goldman Sachs BUY 1,000,000 EUR/USD @ 1.08500"
curl -s -X POST "$API/orders" \
  -H "Content-Type: application/json" \
  -d '{"symbol":"EUR/USD","price":1.08500,"quantity":1000000,
       "side":"BUY","counterparty":"Goldman Sachs"}'

sleep 3

echo "Step 2: JP Morgan SELL 1,000,000 EUR/USD @ 1.08500"
curl -s -X POST "$API/orders" \
  -H "Content-Type: application/json" \
  -d '{"symbol":"EUR/USD","price":1.08500,"quantity":1000000,
       "side":"SELL","counterparty":"JP Morgan"}'
```

### `demo_rising_price.sh`

Demonstrates two sequential trades with an increasing last price, triggering the **▲** arrow in the `MarketSummary` Last Trade column. Trade 1 executes @ 1.08450; Trade 2 executes @ 1.08520. The script uses a reusable `submit()` helper to keep the `curl` invocations concise.

```bash
#!/usr/bin/env bash
API="http://localhost:9090"

submit() {
    local label="$1" side="$2" price="$3" cp="$4"
    curl -s -X POST "$API/orders" \
        -H "Content-Type: application/json" \
        -d "{\"symbol\":\"EUR/USD\",\"price\":$price,
             \"quantity\":1000000,\"side\":\"$side\",
             \"counterparty\":\"$cp\"}"
}

# Trade 1 @ 1.08450
submit "Goldman Sachs BUY"  "BUY"  1.08450 "Goldman Sachs"
sleep 3
submit "JP Morgan SELL"     "SELL" 1.08450 "JP Morgan"

sleep 3

# Trade 2 @ 1.08520 — higher price triggers ▲ in the UI
submit "Deutsche Bank BUY" "BUY"  1.08520 "Deutsche Bank"
sleep 3
submit "JP Morgan SELL"    "SELL" 1.08520 "JP Morgan"
```

### `run_orders_loop.sh`

The stress-test script. Reads `forex_orders.csv` (100 orders across 25 currency pairs), submits each one to the REST API with a 50 ms delay, cycles the counterparty across Goldman Sachs, JP Morgan, and Deutsche Bank, then cancels every open order and starts over. It runs indefinitely until interrupted with Ctrl-C.

The cancel step uses `jq` to parse `GET /books` and extract all order IDs from every price level on both sides of every symbol:

```bash
cancel_all_open_orders() {
    local ids
    ids=$(curl -s "$API/books" | jq -r '
        to_entries[].value
        | (.bids[], .asks[])
        | .orderIds[]
    ')
    while IFS= read -r id; do
        curl -s -X DELETE "$API/orders/$id" > /dev/null
    done <<< "$ids"
}
```

Running this script while the UI is open provides a live demonstration of the order book updating in real time — bids and asks populating across multiple currency pairs, trades executing when prices cross, and the book clearing between loops.

---

## Input Format

Orders are read from `forex_orders.csv` on startup to seed the initial book state:

```csv
Symbol,Price,Quantity,Side
EUR/USD,1.0842,9847,BUY
GBP/USD,1.2634,10523,SELL
USD/JPY,149.82,5000,BUY
```

The CSV contains 100 sample orders, 24 of which result in SPOT trades when processed in order. The same format is accepted by `run_orders_loop.sh` for the stress-test loop.

### Supported Currency Pairs

Any combination of: EUR, GBP, USD, JPY, CHF, AUD, CAD, NZD, SGD, HKD, CNY, MXN, ZAR

---

## Testing

The test suite lives in `tests.cpp` and uses a minimal pass/fail framework with no external dependencies.

### Build and Run

```bash
g++ -std=c++17 -fdiagnostics-color=always -g \
    Counterparty.cpp Order.cpp OrderBook.cpp OrderManager.cpp SubBook.cpp \
    MarketPrice.cpp MarketManager.cpp TradeManager.cpp EventBus.cpp \
    tests.cpp -o run_tests

./run_tests                        # all 12 sections (167 checks)
./run_tests "Cascade Fills"        # one section in isolation
./run_tests "Trade Pricing"
./run_tests "Price Boundary"
```

Sections are filtered by a case-sensitive substring match on the section name. All orders still execute when a filter is active (so book state remains consistent), but only checks inside the matching section count toward the result.

### Test Sections (167 tests total)

| # | Section | Tests | What is verified |
|---|---|---|---|
| 1 | Order Routing | 20 | All 10 order types route to the correct side |
| 2 | Buy-Side Price Priority | 3 | `begin()` returns best bid (highest price) |
| 3 | Sell-Side Price Priority | 3 | `begin()` returns best ask (lowest price) |
| 4 | Time Priority (FIFO) | 7 | Orders at the same price level fill in strict insertion order |
| 5 | Order Cancellation | 6 | Level removed on last cancel; preserved on partial cancel; invalid ID is a no-op |
| 6 | Counterparty Tracking | 14 | IDs accumulate on submit, removed on cancel; two counterparties track independently |
| 7 | SPOT Matching Engine | 40 | Exact match, partial buy/ask fills, multi-level sweeps, no-match guard, sell-exact mirror |
| 8 | Trade Pricing | 4 | Execution always occurs at the maker's (standing order's) price, not the aggressor's |
| 9 | FIFO Fill Order | 12 | Multiple resting orders at the same price fill in arrival order |
| 10 | Trade Notification Details | 11 | Order IDs, prices, counterparty names, and fill quantities in `TradeNotification` |
| 11 | Cascade Fills | 20 | Partially-filled order remainder is matchable by a subsequent incoming order |
| 12 | Price Boundary Conditions | 11 | `bid >= ask` is inclusive; one pip below/above → no trade |

---

## Current Status

- SPOT order matching is fully implemented with partial fills and counterparty notifications
- REST API and SSE streaming are live — the React UI can submit/cancel orders and see real-time book and trade updates
- `MarketManager` remains a stub (no live data feeds)
- MARKET, LIMIT, STOP, and SWAP orders are queued but not yet matched against each other
- No persistence layer — all state is in-memory
- HTTP requests are serialised through a single mutex; the matching engine is not independently thread-safe

---

## Core Lessons

Several areas of the implementation showcase specific C++ and React patterns worth highlighting.

### Type Erasure with `std::function`

`BidMap` and `AskMap` are distinct C++ types (both are `std::map` but with different comparators). A single generic `OrderLocation` struct must be able to erase a price level from either without knowing which one it belongs to. The solution is to capture the erase operation at insert time as a `std::function<void(double)>` lambda:

```cpp
// For a buy order — closes over BidMap*
BidMap* bids = &sb.getBuyOrdersRef();
std::function<void(double)> eraseLevel = [bids](double p) {
    bids->erase(p);
};

// For a sell order — closes over AskMap*
AskMap* asks = &sb.getSellOrdersRef();
std::function<void(double)> eraseLevel = [asks](double p) {
    asks->erase(p);
};
```

`cancel()` calls `eraseLevel(price)` without any branching or `dynamic_cast`. The type difference is completely hidden behind the uniform `void(double)` signature.

### Iterator Stability of `std::list`

`std::list` guarantees that inserting or erasing any node does **not** invalidate iterators to other nodes. The matching engine relies on this: it stores a `std::list<Order>::iterator` in `OrderLocation` and calls `list::erase(it)` in O(1) during cancellation. Inside the matching loop it advances the iterator before erasing:

```cpp
orderIt = level.erase(orderIt);  // erase returns the next valid iterator
```

This ensures the loop variable is always valid regardless of how many nodes are consumed.

### Atomic ID Generation

`Order` and `Counterparty` each use a static `std::atomic<long>` counter. `memory_order_relaxed` is sufficient because the only requirement is uniqueness — no ordering guarantee relative to other operations is needed.

```cpp
std::atomic<long> Order::nextId{1};

// In the Order constructor:
this->id = nextId.fetch_add(1, std::memory_order_relaxed);
```

### Condition Variables for SSE Back-Pressure

Each SSE connection owns a `std::condition_variable`. The handler thread blocks on `cv.wait_for(20s)` rather than busy-waiting. `EventBus::publish()` wakes exactly the threads that have work via `cv.notify_one()`. The 20-second timeout doubles as a **keepalive heartbeat** — if no event arrives, a comment line `": keepalive\n\n"` is written to keep the connection alive through HTTP proxies.

### Move Semantics in Order Queuing

Orders are pushed into price-level lists by value. The compiler uses move semantics where possible, avoiding unnecessary copies of the `Order` struct when it is constructed in the HTTP handler and forwarded into the `std::list`.

### Non-Owning Pointers as Observer Pattern

`Order` holds a raw `Counterparty*` with no ownership semantics. This deliberately signals that `Order` does not manage the counterparty's lifetime — `Counterparty` objects are owned by the application shell and outlive any individual order. The pattern avoids `shared_ptr` reference-counting overhead while making ownership relationships explicit in the type system.

### Lazy Initialisation via `operator[]`

`OrderBook::get(symbol)` is a one-liner:

```cpp
SubBook& OrderBook::get(const std::string& symbol) {
    return books[symbol];
}
```

`std::unordered_map::operator[]` default-constructs the value if the key is absent. A `SubBook` default-constructs to empty `BidMap` and `AskMap`. No explicit "create if absent" logic is needed anywhere.

### Generic JSON Serialisation with a C++17 Generic Lambda

`OrderManager` serialises both `BidMap` and `AskMap` through a single generic lambda using C++17's `auto` parameter, which deduces the map type at compile time:

```cpp
static auto appendPriceLevels = [](std::ostringstream& j, auto& map) {
    bool first = true;
    for (const auto& [price, orders] : map) {
        if (!first) j << ",";
        first = false;
        long total = 0;
        std::string ids = "[";
        bool fst = true;
        for (const auto& o : orders) {
            total += o.getQuantity();
            if (!fst) ids += ",";
            fst = false;
            ids += std::to_string(o.getId());
        }
        ids += "]";
        j << "{\"price\":"    << price
          << ",\"quantity\":" << total
          << ",\"orderIds\":" << ids << "}";
    }
};
```

The `auto& map` parameter accepts either map type at compile time, eliminating the need for a separate serialisation function per side of the book.

---

## Conclusion

TradingSystem demonstrates that a correct, low-latency order matching engine with real-time UI streaming can be built in a few thousand lines of standard C++ and React, without any external C++ library beyond a single-header HTTP server.

### Planned Next Steps

| Area | Description |
|---|---|
| **Matching Engine Completeness** | Add LIMIT (fill-or-rest), STOP (trigger on breach), and SWAP order types, each with their own matching rules. |
| **Persistence** | Add a write-ahead log or database backend so the book survives restarts and supports historical replay. |
| **Market Data Integration** | Connect `MarketManager` to a live WebSocket or FIX feed for reference pricing and stop triggers. |
| **Order Amendment** | Allow modification of a resting order's price or quantity with appropriate queue-position rules. |
| **Fine-Grained Concurrency** | Replace the single global mutex with per-symbol locks or a lock-free structure to allow parallel symbol processing. |
| **Position & Risk Management** | Track net position per counterparty, enforce limits, and compute mark-to-market P&L. |
| **Unified Counterparty Registry** | Merge CSV-sourced and HTTP-submitted counterparties into a single shared registry for unified position tracking. |

The codebase is intentionally structured to make each of these extensions straightforward: the matching engine is isolated in `TradeManager`, the book storage is isolated in `OrderBook` and `SubBook`, and the HTTP interface is isolated in `HTTPServer`. New features can be added to any layer without touching the others.
