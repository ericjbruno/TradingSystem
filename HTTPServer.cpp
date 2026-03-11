#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>
#include "EventBus.h"
#include "HTTPServer.h"
#include "Order.h"
#include "OrderManager.h"
#include "OrderType.h"
#include "SubBook.h"

// ── JSON helpers ──────────────────────────────────────────────────────────────

// Escape a string for JSON output (handles " and \)
static std::string jsonStr(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 2);
    out += '"';
    for (char c : s) {
        if (c == '"')  { out += "\\\""; }
        else if (c == '\\') { out += "\\\\"; }
        else           { out += c; }
    }
    out += '"';
    return out;
}

// Serialise one side of the book (works with BidMap or AskMap via template)
template<typename MapT>
static std::string levelsJson(const MapT& map) {
    std::ostringstream j;
    j << std::fixed << std::setprecision(6);
    j << "[";
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
    j << "]";
    return j.str();
}

// Simple field extractors for the POST /orders JSON body
static std::string extractStr(const std::string& body, const std::string& key) {
    auto pos = body.find("\"" + key + "\"");
    if (pos == std::string::npos) return "";
    pos = body.find(':', pos);
    if (pos == std::string::npos) return "";
    ++pos;
    while (pos < body.size() && (body[pos] == ' ' || body[pos] == '\t')) ++pos;
    if (pos >= body.size() || body[pos] != '"') return "";
    ++pos;
    auto end = body.find('"', pos);
    if (end == std::string::npos) return "";
    return body.substr(pos, end - pos);
}

static double extractDouble(const std::string& body, const std::string& key) {
    auto pos = body.find("\"" + key + "\"");
    if (pos == std::string::npos) return 0.0;
    pos = body.find(':', pos);
    if (pos == std::string::npos) return 0.0;
    ++pos;
    while (pos < body.size() && (body[pos] == ' ' || body[pos] == '\t')) ++pos;
    try { return std::stod(body.substr(pos)); } catch (...) { return 0.0; }
}

static long extractLong(const std::string& body, const std::string& key) {
    auto pos = body.find("\"" + key + "\"");
    if (pos == std::string::npos) return 0;
    pos = body.find(':', pos);
    if (pos == std::string::npos) return 0;
    ++pos;
    while (pos < body.size() && (body[pos] == ' ' || body[pos] == '\t')) ++pos;
    try { return std::stol(body.substr(pos)); } catch (...) { return 0; }
}

// ── HTTPServer ────────────────────────────────────────────────────────────────

HTTPServer::HTTPServer(OrderManager& om, EventBus& bus)
    : om_(om), bus_(bus) {
    counterparties_.emplace("Goldman Sachs", Counterparty("Goldman Sachs"));
    counterparties_.emplace("JP Morgan",     Counterparty("JP Morgan"));
    counterparties_.emplace("Deutsche Bank", Counterparty("Deutsche Bank"));
    setupRoutes();
}

void HTTPServer::addCors(httplib::Response& res) {
    res.set_header("Access-Control-Allow-Origin",  "*");
    res.set_header("Access-Control-Allow-Methods", "GET, POST, DELETE, OPTIONS");
    res.set_header("Access-Control-Allow-Headers", "Content-Type");
}

std::string HTTPServer::bookJson(const std::string& symbol) {
    SubBook& sb = om_.getSubBook(symbol);
    std::ostringstream j;
    j << "{\"symbol\":" << jsonStr(symbol)
      << ",\"bids\":"   << levelsJson(sb.getBuyOrdersRef())
      << ",\"asks\":"   << levelsJson(sb.getSellOrdersRef())
      << "}";
    return j.str();
}

void HTTPServer::setupRoutes() {

    // ── CORS preflight ───────────────────────────────────────────────────────
    svr_.Options(".*", [](const httplib::Request&, httplib::Response& res) {
        addCors(res);
        res.status = 204;
    });

    // ── GET /symbols ─────────────────────────────────────────────────────────
    svr_.Get("/symbols", [this](const httplib::Request&, httplib::Response& res) {
        std::vector<std::string> syms;
        {
            std::lock_guard<std::mutex> lk(mu_);
            syms = om_.getSymbols();
        }
        std::sort(syms.begin(), syms.end());
        std::ostringstream j;
        j << "[";
        bool first = true;
        for (const auto& s : syms) {
            if (!first) j << ",";
            first = false;
            j << jsonStr(s);
        }
        j << "]";
        addCors(res);
        res.set_content(j.str(), "application/json");
    });

    // ── GET /book/:symbol ────────────────────────────────────────────────────
    svr_.Get(R"(/book/(.+))", [this](const httplib::Request& req, httplib::Response& res) {
        const std::string symbol = req.matches[1];
        std::string json;
        {
            std::lock_guard<std::mutex> lk(mu_);
            json = bookJson(symbol);
        }
        addCors(res);
        res.set_content(json, "application/json");
    });

    // ── GET /books ───────────────────────────────────────────────────────────
    svr_.Get("/books", [this](const httplib::Request&, httplib::Response& res) {
        std::vector<std::string> syms;
        std::ostringstream j;
        {
            std::lock_guard<std::mutex> lk(mu_);
            syms = om_.getSymbols();
            j << "{";
            bool first = true;
            for (const auto& sym : syms) {
                if (!first) j << ",";
                first = false;
                j << jsonStr(sym) << ":" << bookJson(sym);
            }
            j << "}";
        }
        addCors(res);
        res.set_content(j.str(), "application/json");
    });

    // ── GET /trades ──────────────────────────────────────────────────────────
    svr_.Get("/trades", [this](const httplib::Request&, httplib::Response& res) {
        std::ostringstream j;
        {
            std::lock_guard<std::mutex> lk(mu_);
            const auto& trades = om_.getRecentTrades();
            j << std::fixed << std::setprecision(6);
            j << "[";
            bool first = true;
            for (const auto& t : trades) {
                if (!first) j << ",";
                first = false;
                j << "{\"symbol\":"      << jsonStr(t.symbol)
                  << ",\"price\":"       << t.price
                  << ",\"quantity\":"    << t.quantity
                  << ",\"buyOrderId\":"  << t.buyOrderId
                  << ",\"sellOrderId\":" << t.sellOrderId
                  << ",\"buyer\":"       << jsonStr(t.buyer  ? t.buyer->getName()  : "")
                  << ",\"seller\":"      << jsonStr(t.seller ? t.seller->getName() : "")
                  << "}";
            }
            j << "]";
        }
        addCors(res);
        res.set_content(j.str(), "application/json");
    });

    // ── GET /counterparties ──────────────────────────────────────────────────
    svr_.Get("/counterparties", [this](const httplib::Request&, httplib::Response& res) {
        std::ostringstream j;
        j << "[";
        bool first = true;
        for (const auto& [name, _] : counterparties_) {
            if (!first) j << ",";
            first = false;
            j << jsonStr(name);
        }
        j << "]";
        addCors(res);
        res.set_content(j.str(), "application/json");
    });

    // ── POST /orders ─────────────────────────────────────────────────────────
    svr_.Post("/orders", [this](const httplib::Request& req, httplib::Response& res) {
        const std::string& body = req.body;

        std::string symbol      = extractStr(body, "symbol");
        double      price       = extractDouble(body, "price");
        long        quantity    = extractLong(body, "quantity");
        std::string side        = extractStr(body, "side");
        std::string cpName      = extractStr(body, "counterparty");

        if (symbol.empty() || quantity <= 0 || (side != "BUY" && side != "SELL")) {
            addCors(res);
            res.status = 400;
            res.set_content("{\"success\":false,\"error\":\"invalid request\"}", "application/json");
            return;
        }

        // Find or create counterparty
        Counterparty* cp = nullptr;
        {
            auto it = counterparties_.find(cpName);
            if (it != counterparties_.end()) {
                cp = &it->second;
            } else {
                // Unknown name — use first available
                cp = &counterparties_.begin()->second;
            }
        }

        OrderType orderType = (side == "BUY") ? OrderType::SPOT_BUY : OrderType::SPOT_SELL;
        Order order(symbol, price, static_cast<int>(quantity), orderType, cp);
        long newId = order.getId();

        {
            std::lock_guard<std::mutex> lk(mu_);
            om_.processNewOrder(order);
        }

        std::ostringstream j;
        j << "{\"success\":true,\"orderId\":" << newId << "}";
        addCors(res);
        res.set_content(j.str(), "application/json");
    });

    // ── DELETE /orders/:id ───────────────────────────────────────────────────
    svr_.Delete(R"(/orders/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
        long id = std::stol(req.matches[1]);
        {
            std::lock_guard<std::mutex> lk(mu_);
            om_.processCancelOrder(id);
        }
        addCors(res);
        res.set_content("{\"success\":true}", "application/json");
    });

    // ── GET /events (SSE) ────────────────────────────────────────────────────
    svr_.Get("/events", [this](const httplib::Request&, httplib::Response& res) {
        auto conn = bus_.subscribe();

        res.set_header("Cache-Control",                "no-cache");
        res.set_header("Connection",                   "keep-alive");
        res.set_header("Access-Control-Allow-Origin",  "*");

        res.set_chunked_content_provider(
            "text/event-stream",
            [conn](size_t, httplib::DataSink& sink) -> bool {
                std::unique_lock<std::mutex> lk(conn->mu);
                // Block up to 20 s; wake on new event or disconnect
                conn->cv.wait_for(lk, std::chrono::seconds(20), [&] {
                    return !conn->queue.empty() || conn->closed;
                });

                if (conn->closed) {
                    sink.done();
                    return false;
                }

                if (conn->queue.empty()) {
                    // Keepalive comment keeps the connection alive through proxies
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
            [this, conn](bool) {
                bus_.unsubscribe(conn);
            }
        );
    });
}

void HTTPServer::start(int port) {
    std::cout << "HTTP server listening on http://localhost:" << port << "\n";
    svr_.listen("0.0.0.0", port);
}

void HTTPServer::stop() {
    svr_.stop();
}
