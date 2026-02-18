# Suggested Optimizations

Performance and safety improvements. Items marked **[Done]** have been implemented.
Remaining items are not yet implemented.

---

## 2. Integer price representation

**File:** `SubBook.h`, `Order.h`, `OrderBook.h`

`PriceLevelMap` uses `double` as the price key. Floating-point arithmetic can cause two
prices that should be equal to land on different map keys (e.g. `1.0842 + 0.0001 ≠ 1.0843`
exactly in IEEE 754). Real trading systems represent prices as integers in the smallest
tick unit (e.g. price × 10000 stored as `long`).

```cpp
// Instead of:
using PriceLevelMap = std::map<double, std::list<Order>>;

// Use integer ticks:
using PriceLevelMap = std::map<long, std::list<Order>>;  // price * 10000

// Order stores:
long priceTicks;  // e.g. 1.0842 → 10842
```

---

## 5. `push_back` → `emplace_back`

**File:** `OrderManager.cpp:28`

Every `processNewOrder` call copies the `Order` into the list via `push_back`.
`emplace_back` constructs in place, eliminating the copy.

```cpp
// Instead of:
priceLevel.push_back(newOrder);

// Use:
priceLevel.emplace_back(newOrder);
// Or with forwarding if Order gains a move constructor:
priceLevel.emplace_back(std::move(newOrder));
```

---

## 6. Double map lookup in `OrderBook::get()`

**File:** `OrderBook.cpp:33-39`

`get()` calls `books.find(symbol)` then `books[symbol]` — two hash/tree traversals.
`try_emplace` does both in a single lookup.

```cpp
SubBook& OrderBook::get(const std::string& symbol) {
    auto [it, inserted] = books.try_emplace(symbol);
    if (inserted) {
        std::cout << "Creating order book for symbol: " << symbol << std::endl;
    }
    return it->second;
}
```

---

## 7. Order constructor takes `std::string` by value

**File:** `Order.h:20`, `Order.cpp:5`

The constructor parameter `std::string symbol` is passed by value, causing a copy
on every call. Use `const std::string&` to avoid the copy, or accept by value and
move it in.

```cpp
// Option A — const ref:
Order(const std::string& symbol, double price, int quantity, OrderType type);

// Option B — move (best for rvalue callers):
Order(std::string symbol, double price, int quantity, OrderType type)
    : symbol(std::move(symbol)) { ... }
```

---

## 8. `nextId` not thread-safe **[Done]**

**File:** `Order.h:17`, `Order.cpp:3`

`static long nextId` is a race condition if orders are created from multiple threads.
Replace with `std::atomic<long>`.

```cpp
// Order.h:
#include <atomic>
static std::atomic<long> nextId;

// Order.cpp:
std::atomic<long> Order::nextId{1};
this->id = nextId.fetch_add(1, std::memory_order_relaxed);
```

---

## 9. Raw pointers → smart pointers **[Done]**

**File:** `OrderManager.h`, `OrderManager.cpp`

`OrderManager` owns `OrderBook*` and `MarketManager*` via raw `new` with no
corresponding `delete` in the destructor. Use `std::unique_ptr` to eliminate
the leak and make ownership explicit.

```cpp
// OrderManager.h:
#include <memory>
std::unique_ptr<OrderBook> orderBook;
std::unique_ptr<MarketManager> marketManager;  // if owned

// OrderManager.cpp constructor:
orderBook = std::make_unique<OrderBook>();
```

---

## 10. `std::cout` in `OrderBook::get()` hot path **[Done]**

**File:** `OrderBook.cpp:34`

Stdout I/O inside `get()` means every new symbol hits a system call during order
processing. Remove the log line or guard it behind a debug flag.

```cpp
#ifdef DEBUG_ORDERBOOK
    std::cout << "Creating order book for symbol: " << symbol << std::endl;
#endif
```
