#!/usr/bin/env bash
# Demonstrates a single trade:
#   1. Goldman Sachs bids 1,000,000 EUR/USD @ 1.08500
#   2. (3-second pause)
#   3. JP Morgan offers 1,000,000 EUR/USD @ 1.08500 — crosses the bid, trade executes

set -euo pipefail

API="http://localhost:9090"

echo "Step 1: Goldman Sachs BUY 1,000,000 EUR/USD @ 1.08500"
response=$(curl -s -X POST "$API/orders" \
  -H "Content-Type: application/json" \
  -d '{"symbol":"EUR/USD","price":1.08500,"quantity":1000000,"side":"BUY","counterparty":"Goldman Sachs"}')
echo "  → $response"

echo ""
echo "Waiting 3 seconds..."
sleep 3

echo ""
echo "Step 2: JP Morgan SELL 1,000,000 EUR/USD @ 1.08500 (crosses the bid)"
response=$(curl -s -X POST "$API/orders" \
  -H "Content-Type: application/json" \
  -d '{"symbol":"EUR/USD","price":1.08500,"quantity":1000000,"side":"SELL","counterparty":"JP Morgan"}')
echo "  → $response"

echo ""
echo "Trade should have executed. Check the UI or GET $API/trades"
