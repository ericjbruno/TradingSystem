# TradingSystem Architecture Document

## Overview

TradingSystem is a C++ forex trading system designed to process and manage trading orders. The system provides order management, order book maintenance, and trade execution checking capabilities.

## Project Structure

```
TradingSystem/
├── Source Files
│   ├── TradingSystem.cpp    # Main entry point
│   ├── Order.cpp            # Order implementation
│   ├── OrderManager.cpp     # Order processing logic
│   ├── OrderBook.cpp        # Order book storage
│   ├── SubBook.cpp          # Buy/sell order containers
│   ├── MarketPrice.cpp      # Market price data
│   └── MarketManager.cpp    # Market data management
│
├── Header Files
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
│  │  Map<Symbol, SubBook>                                 │  │
│  │  ┌─────────────────────────────────────────────────┐  │  │
│  │  │  EUR/USD → SubBook                              │  │  │
│  │  │           ├── buyOrders: [Order, Order, ...]    │  │  │
│  │  │           └── sellOrders: [Order, Order, ...]   │  │  │
│  │  │  GBP/USD → SubBook                              │  │  │
│  │  │           ├── buyOrders: [...]                  │  │  │
│  │  │           └── sellOrders: [...]                 │  │  │
│  │  └─────────────────────────────────────────────────┘  │  │
│  └───────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
                         │
                         ▼
┌─────────────────────────────────────────────────────────────┐
│                      VALUE OBJECTS                           │
│  ┌──────────────┐  ┌─────────────┐  ┌──────────────┐        │
│  │    Order     │  │ MarketPrice │  │  OrderType   │        │
│  │  - symbol    │  │  - symbol   │  │  (enum)      │        │
│  │  - price     │  │  - price    │  │  MARKET_BUY  │        │
│  │  - quantity  │  │  - quantity │  │  MARKET_SELL │        │
│  │  - type      │  │  - timestamp│  │  LIMIT_BUY   │        │
│  │  - active    │  │             │  │  LIMIT_SELL  │        │
│  └──────────────┘  └─────────────┘  │  STOP_BUY    │        │
│                                     │  STOP_SELL   │        │
│                                     │  SPOT_BUY    │        │
│                                     │  SPOT_SELL   │        │
│                                     │  SWAP_BUY    │        │
│                                     │  SWAP_SELL   │        │
│                                     └──────────────┘        │
└─────────────────────────────────────────────────────────────┘
```

## Core Components

### 1. Order

**Purpose:** Represents a single trading order.

**Attributes:**
| Attribute | Type | Description |
|-----------|------|-------------|
| symbol | string | Trading pair (e.g., "EUR/USD") |
| price | double | Order price |
| quantity | long | Number of units |
| type | OrderType | Type of order (market, limit, etc.) |
| active | bool | Whether order is active |

**Key Methods:**
- `comesBefore(Order&)` - Determines price priority ordering
- `isBuyOrder()` / `isSellOrder()` - Order side checks
- `isLimitOrder()` - Order type check

**Price Priority Logic:**
- Buy orders: Higher price comes first (best bid)
- Sell orders: Lower price comes first (best ask)

### 2. OrderType

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

### 3. SubBook

**Purpose:** Container for buy and sell orders of a single symbol.

**Attributes:**
- `buyOrders` (std::list<Order>) - Sorted list of buy orders
- `sellOrders` (std::list<Order>) - Sorted list of sell orders

**Design Pattern:** Composite pattern - groups orders by side.

### 4. OrderBook

**Purpose:** Central registry for all symbol order books.

**Attributes:**
- `books` (std::map<string, SubBook>) - Symbol to SubBook mapping

**Key Methods:**
- `get(symbol)` - Returns SubBook reference (lazy initialization)
- `put(symbol, subBook)` - Stores/updates SubBook

**Design Pattern:** Repository pattern with lazy initialization.

### 5. OrderManager

**Purpose:** Orchestrates order processing and trade checking.

**Attributes:**
- `orderBook` (OrderBook*) - Pointer to order storage
- `marketManager` (MarketManager*) - Pointer to market data

**Key Methods:**
- `processNewOrder(Order)` - Inserts order in sorted position
- `processCancelOrder(orderId)` - Cancels existing order
- `checkForTrade(Order, marketPrice)` - Checks execution conditions

**Trade Execution Logic:**
```
Buy Order:  Execute if order.price >= marketPrice
Sell Order: Execute if order.price <= marketPrice
```

### 6. MarketManager

**Purpose:** Manages market data and pricing information.

**Status:** Stub implementation - placeholder for future market data integration.

### 7. MarketPrice

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
3. Create Order object
   │  - Map "BUY"/"SELL" to OrderType
   │
   ▼
4. OrderManager::processNewOrder(order)
   │
   ▼
5. OrderBook::get(symbol)
   │  - Lazy init SubBook if needed
   │
   ▼
6. Select appropriate list (buyOrders/sellOrders)
   │
   ▼
7. Insert order in sorted position
   │  - Maintain price priority
   │
   ▼
8. Order stored in SubBook
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

## Dependencies

### Standard Library
- `<string>` - String handling
- `<list>` - Doubly-linked lists for order storage
- `<map>` - Symbol to SubBook mapping
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
g++ -fdiagnostics-color=always -g *.cpp -o trading_system
```

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
2. **Order cancellation** - Not fully implemented
3. **Trade execution** - Condition checking only, no actual execution
4. **Persistence** - No database integration
5. **Multi-threading** - Single-threaded processing

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
