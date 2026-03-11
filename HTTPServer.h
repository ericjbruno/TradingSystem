#ifndef HTTPSERVER_H
#define HTTPSERVER_H

#include <map>
#include <mutex>
#include <string>
#include "Counterparty.h"
#include "EventBus.h"
#include "httplib.h"

class OrderManager;

// REST + SSE HTTP server for the trading UI.
//
// Endpoints:
//   GET  /symbols             — list of symbols with active orders
//   GET  /book/:symbol        — full bid/ask snapshot for one symbol
//   GET  /books               — snapshots for every symbol (initial load)
//   GET  /trades              — recent trades (up to 100)
//   GET  /counterparties      — available counterparty names
//   POST /orders              — submit a new order
//   DELETE /orders/:id        — cancel an order by ID
//   GET  /events              — SSE stream (trade and book_update events)
//
// Thread safety: all OrderManager access is serialised through mu_.
// The SSE handler runs in its own httplib thread, waiting on the EventBus
// connection queue — it does NOT hold mu_ while waiting.
class HTTPServer {
public:
    HTTPServer(OrderManager& om, EventBus& bus);
    void start(int port);   // blocks until stop() is called
    void stop();

private:
    httplib::Server svr_;
    OrderManager&   om_;
    EventBus&       bus_;
    std::mutex      mu_;    // guards all OrderManager access from HTTP threads

    // Counterparties owned by this server for HTTP-submitted orders
    std::map<std::string, Counterparty> counterparties_;

    void setupRoutes();

    // Build book JSON object for one symbol (no SSE prefix)
    std::string bookJson(const std::string& symbol);

    // Append CORS headers to every response
    static void addCors(httplib::Response& res);
};

#endif
