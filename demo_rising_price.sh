#!/usr/bin/env bash
# Demonstrates two consecutive trades with a rising last price,
# triggering the up-arrow (▲) in the Last Trade column.
#
#   Trade 1: Goldman Sachs bids EUR/USD @ 1.08450, JP Morgan sells @ 1.08450
#   Trade 2: Deutsche Bank bids EUR/USD @ 1.08520, JP Morgan sells @ 1.08520

set -euo pipefail

API="http://localhost:9090"

submit() {
    local label="$1" side="$2" price="$3" cp="$4"
    local response
    response=$(curl -s -X POST "$API/orders" \
        -H "Content-Type: application/json" \
        -d "{\"symbol\":\"EUR/USD\",\"price\":$price,\"quantity\":1000000,\"side\":\"$side\",\"counterparty\":\"$cp\"}")
    local id
    id=$(echo "$response" | grep -o '"orderId":[0-9]*' | grep -o '[0-9]*')
    echo "  $label → order #$id"
}

echo "── Trade 1 ──────────────────────────────────────────"
submit "Goldman Sachs  BUY  1,000,000 EUR/USD @ 1.08450" "BUY"  1.08450 "Goldman Sachs"

echo "  Waiting 3 seconds..."
sleep 3

submit "JP Morgan      SELL 1,000,000 EUR/USD @ 1.08450" "SELL" 1.08450 "JP Morgan"
echo "  Trade 1 should have executed @ 1.08450"

echo ""
echo "── Trade 2 (higher price) ───────────────────────────"
echo "  Waiting 3 seconds..."
sleep 3

submit "Deutsche Bank  BUY  1,000,000 EUR/USD @ 1.08520" "BUY"  1.08520 "Deutsche Bank"

echo "  Waiting 3 seconds..."
sleep 3

submit "JP Morgan      SELL 1,000,000 EUR/USD @ 1.08520" "SELL" 1.08520 "JP Morgan"
echo "  Trade 2 should have executed @ 1.08520  (▲ up arrow expected)"
