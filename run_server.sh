#!/usr/bin/env bash
set -e

cd "$(dirname "$0")"

echo "Building TradingSystem..."
g++ -std=c++17 -fdiagnostics-color=always -g \
    Counterparty.cpp Order.cpp OrderBook.cpp OrderManager.cpp SubBook.cpp \
    MarketPrice.cpp MarketManager.cpp TradeManager.cpp \
    EventBus.cpp HTTPServer.cpp TradingSystem.cpp \
    -lpthread -o TradingSystem

echo "Starting server..."
./TradingSystem
