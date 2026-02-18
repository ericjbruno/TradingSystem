# TradingSystem Architecture Document

## Overview

TradingSystem is a C++ forex trading system designed to process and manage trading orders. The system provides order management, order book maintenance, and trade execution checking capabilities.

## Project Structure

```
TradingSystem/
├── Source Files
│   ├── TradingSystem.cpp    # Main entry point
│   ├── Counterparty.cpp     # Counterparty identity and order tracking
│   ├── Order.cpp            # Order implementation
│   ├── OrderManager.cpp     # Order processing logic
│   ├── OrderBook.cpp        # Order book storage
│   ├── SubBook.cpp          # Buy/sell order containers
│   ├── MarketPrice.cpp      # Market price data
│   └── MarketManager.cpp    # Market data management
│
├── Header Files
│   ├── Counterparty.h
│   ├── Order.h
│   ├── OrderType.h          # Order type enum definitions
│   ├── OrderManager.h
│   ├── OrderBook.h
│   ├── SubBook.h
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
│  │  - Console Output                                     │  │
│  └───────────────────────────────────────────────────────┘  │
└────────────────────────────┬────────────────────────────────┘
                             │
                             ▼
┌─────────────────────────────────────────────────────────────┐
│                    BUSINESS LOGIC LAYER                      │
│  ┌─────────────────────┐    ┌─────────────────────────┐     │
│  │    OrderManager     │    │     MarketManager       │     │
│  │  - processNewOrder  │    │  - Market data feeds    │     │
│  │  - checkForTrade    │    │  - Price management     │     │
│  │  - cancelOrder      │    │                         │     │
│  └──────────┬──────────┘    └────────────┬────────────┘     │
└─────────────┼────────────────────────────┼──────────────────┘
              │                            │
              └──────────┬─────────────────┘
                         ▼
┌─────────────────────────────────────────────────────────────┐
│                      DATA STORAGE LAYER                      │
│  ┌───────────────────────────────────────────────────────┐  │
│  │                     OrderBook                         │  │
│  │  unordered_map<Symbol, SubBook>   (O(1) lookup)      │  │
│  │  unordered_map<id, OrderLocation> (O(1) cancel)      │  │
│  │  ┌─────────────────────────────────────────────────┐  │  │
│  │  │  EUR/USD → SubBook                              │  │  │
│  │  │   ├── buyOrders:  map<price, list<Order>>       │  │  │
│  │  │   │               best bid = rbegin() O(1)      │  │  │
│  │  │   └── sellOrders: map<price, list<Order>>       │  │  │
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
│  ┌──────────────┐                   │  SWAP_SELL   │        │
│  │ Counterparty │                   └──────────────┘        │
│  │  - id        │                                           │
│  │  - name      │                                           │
│  │  - orderIds  │                                           │
│  └──────────────┘                                           │
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
| quantity | long | Number of units |
| type | OrderType | Type of order (market, limit, etc.) |
| active | bool | Whether order is active |
| counterparty | `Counterparty*` | Non-owning observer pointer to the submitting counterparty |

**Key Methods:**
- `comesBefore(Order&)` - Determines price priority ordering
- `isBuyOrder()` / `isSellOrder()` - Order side checks
- `isLimitOrder()` - Order type check
- `getCounterparty()` - Returns the non-owning counterparty pointer

**Price Priority Logic:**
- Buy orders: Higher price comes first (best bid)
- Sell orders: Lower price comes first (best ask)

### 2. Counterparty

**Purpose:** Represents a trading entity that submits orders. Maintains a live list of its open order IDs.

**Attributes:**
| Attribute | Type | Description |
|-----------|------|-------------|
| id | long | Auto-increment ID (thread-safe `std::atomic<long>`) |
| name | string | Display name (e.g., "Goldman Sachs") |
| orderIds | `vector<long>` | IDs of currently open orders submitted by this counterparty |

**Key Methods:**
- `addOrderId(id)` - Called by `OrderManager` after a successful order insertion
- `removeOrderId(id)` - Called by `OrderManager` after a successful cancellation
- `getOrderIds()` - Returns the current open order ID list

**Ownership model:** `Counterparty` objects are owned and managed by the caller (e.g., `TradingSystem.cpp`). `Order` holds a non-owning raw pointer — it observes the counterparty, it does not control its lifetime. This follows the same pattern as `MarketManager*` in `OrderManager`.

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

**Design Note:** Uses even/odd numbering convention:
- Even numbers = Buy orders
- Odd numbers = Sell orders

### 4. SubBook

**Purpose:** Container for buy and sell orders of a single symbol.

**Attributes:**
- `buyOrders` (`PriceLevelMap`) - Price-sorted map of buy orders: `std::map<double, std::list<Order>>`
- `sellOrders` (`PriceLevelMap`) - Price-sorted map of sell orders: `std::map<double, std::list<Order>>`

`rbegin()` on buyOrders yields the best bid (highest price); `begin()` on sellOrders yields the best ask (lowest price) — both O(1).

**Design Pattern:** Composite pattern - groups orders by side.

### 5. OrderBook

**Purpose:** Central registry for all symbol order books.

**Attributes:**
- `books` (`std::unordered_map<string, SubBook>`) - O(1) symbol to SubBook lookup; lazy-initializes SubBook on first access
- `orderIndex` (`std::unordered_map<long, OrderLocation>`) - O(1) order ID to location index for cancellation

`OrderLocation` stores `{ PriceLevelMap*, double price, std::list<Order>::iterator }` — enough to erase any order in O(1) without knowing the symbol.

**Key Methods:**
- `get(symbol)` - Returns SubBook reference (lazy initialization via `operator[]`)
- `put(symbol, subBook)` - Stores/updates SubBook
- `indexOrder(id, loc)` - Registers an order in the cancellation index
- `cancel(id)` - Removes order from its price level and the index in O(1)
- `getOrderCounterparty(id)` - Returns the `Counterparty*` for an order ID via the cancel index; must be called **before** `cancel()` since the order is erased by cancellation

**Design Pattern:** Repository pattern with lazy initialization.

### 6. OrderManager

**Purpose:** Orchestrates order processing and trade checking.

**Attributes:**
- `orderBook` (`std::unique_ptr<OrderBook>`) - Owned order storage; automatically freed on destruction
- `marketManager` (`MarketManager*`) - Non-owning pointer to market data (caller retains ownership)

**Key Methods:**
- `processNewOrder(Order)` - Inserts order into price-level map, registers in cancel index
- `processCancelOrder(orderId)` - Cancels existing order via O(1) index lookup
- `checkForTrade(Order, marketPrice)` - Checks execution conditions

**Trade Execution Logic:**
```
Buy Order:  Execute if order.price >= marketPrice
Sell Order: Execute if order.price <= marketPrice
```

### 7. MarketManager

**Purpose:** Manages market data and pricing information.

**Status:** Stub implementation - placeholder for future market data integration.

### 8. MarketPrice

**Purpose:** Represents market pricing information.

**Attributes:**
| Attribute | Type | Description |
|-----------|------|-------------|
| symbol | string | Trading pair |
| price | double | Current market price |
| quantity | long | Available quantity |
| timestamp | string | Price quote time |

## Data Flow

### Order Processing Flow

```
1. CSV File (forex_orders.csv)
   │
   ▼
2. TradingSystem::main() parses CSV
   │  - Extract: symbol, price, quantity, side
   │
   ▼
3. Assign counterparty + create Order object
   │  - Map "BUY"/"SELL" to OrderType
   │  - Pass Counterparty* (non-owning) to Order constructor
   │
   ▼
4. OrderManager::processNewOrder(order)
   │
   ▼
5. OrderBook::get(symbol)
   │  - Lazy init SubBook if needed
   │
   ▼
6. Select appropriate PriceLevelMap (buyOrders/sellOrders)
   │
   ▼
7. Insert order at price level — O(log n)
   │  - map<price, list<Order>> maintains sorted order
   │
   ▼
8. Index order for O(1) cancellation
   │  - orderIndex[id] = { priceMap*, price, list::iterator }
   │
   ▼
9. Register with counterparty
      counterparty->addOrderId(order.getId())
```

### Cancellation Flow

```
OrderManager::processCancelOrder(id)
   │
   ▼
1. getOrderCounterparty(id)      O(1) — read Counterparty* via cancel index
   │                             (must happen before erasure)
   ▼
2. OrderBook::cancel(id)
   │  a. orderIndex.find(id)     O(1) hash lookup → OrderLocation
   │  b. list::erase(iterator)   O(1) removal from price-level list
   │  c. if level empty → priceMap->erase(price)
   │  d. orderIndex.erase(id)
   │
   ▼
3. counterparty->removeOrderId(id)   update counterparty tracking
```

### Trade Check Flow

```
Order + Market Price
        │
        ▼
OrderManager::checkForTrade()
        │
        ├──► BUY: price >= marketPrice → EXECUTE
        │
        └──► SELL: price <= marketPrice → EXECUTE
```

## Design Patterns

| Pattern | Component | Purpose |
|---------|-----------|---------|
| Repository | OrderBook | Centralized storage with lazy init |
| Composite | SubBook | Groups buy/sell orders |
| Facade | OrderManager | Simplified interface to subsystems |
| Value Object | Order, MarketPrice | Immutable data containers |
| Strategy | OrderType | Type-based behavior classification |
| Observer (non-owning) | Counterparty ↔ Order | Order observes its counterparty via raw pointer; counterparty tracks open order IDs |

## Dependencies

### Standard Library
- `<string>` - String handling
- `<vector>` - Open order ID list in `Counterparty`
- `<list>` - Order queues within each price level
- `<map>` - Price-level map (`PriceLevelMap`) — sorted for O(1) best bid/ask
- `<unordered_map>` - Symbol lookup and cancellation index — O(1) average
- `<atomic>` - Thread-safe ID generation for `Order` and `Counterparty` (`std::atomic<long>`)
- `<memory>` - Smart pointer ownership (`std::unique_ptr`)
- `<algorithm>` - `std::remove` for counterparty order ID removal
- `<iostream>` - Console I/O
- `<fstream>` - File I/O for CSV reading
- `<sstream>` - CSV parsing

### External Dependencies
- None - pure C++ standard library implementation

## Build Configuration

### Compiler
- GCC (g++)
- Debug symbols enabled (`-g`)

### Build Command
```bash
g++ -fdiagnostics-color=always -g \
    Counterparty.cpp Order.cpp OrderBook.cpp OrderManager.cpp SubBook.cpp \
    MarketPrice.cpp MarketManager.cpp TradeManager.cpp \
    TradingSystem.cpp -o trading_system
```

(`tests.cpp` has its own `main` and is excluded; use the `build_tests` script for the test binary.)

## Input Format

### CSV Structure
```csv
Symbol,Price,Quantity,Side
EUR/USD,1.0842,9847,BUY
GBP/USD,1.2634,10523,SELL
```

### Supported Currency Pairs
EUR, GBP, USD, JPY, CHF, AUD, CAD, NZD, SGD, HKD, CNY, MXN, ZAR combinations

## Current Limitations

1. **MarketManager** - Stub implementation only
2. **Trade execution** - Condition checking only, no actual fill
3. **Persistence** - No database integration
4. **Multi-threading** - Single-threaded processing (ID generation is atomic-safe; book access is not)

## Future Development Areas

- Real-time market data integration
- Full trade matching engine
- Order cancellation/modification
- Position and portfolio tracking
- Fee/commission calculation
- Risk management features
- Database persistence layer
- Multi-threaded order processing
- WebSocket/FIX protocol support
