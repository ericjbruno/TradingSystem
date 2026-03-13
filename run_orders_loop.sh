#!/usr/bin/env bash
# Repeatedly submits all orders from forex_orders.csv via the REST API,
# then cancels every open order, then starts over.
# Press Ctrl+C to stop.

set -euo pipefail

API="http://localhost:9090"
CSV="$(dirname "$0")/forex_orders.csv"
DELAY=2        # seconds to pause between loops
ORDER_DELAY=0.05  # seconds between individual order submissions

COUNTERPARTIES=("Goldman Sachs" "JP Morgan" "Deutsche Bank")

cancel_all_open_orders() {
    local ids
    ids=$(curl -s "$API/books" | jq -r '
        to_entries[].value
        | (.bids[], .asks[])
        | .orderIds[]
    ' 2>/dev/null)

    if [ -z "$ids" ]; then
        echo "  (no open orders to cancel)"
        return
    fi

    local count=0
    while IFS= read -r id; do
        [ -z "$id" ] && continue
        curl -s -X DELETE "$API/orders/$id" > /dev/null
        count=$((count + 1))
    done <<< "$ids"

    echo "  Cancelled $count open orders"
}

loop=1
trap 'echo; echo "Stopped after $((loop-1)) loop(s)."; exit 0' INT

while true; do
    echo ""
    echo "════════════════════════════════════════"
    echo "  Loop $loop — submitting orders"
    echo "════════════════════════════════════════"

    submitted=0
    cp_index=0

    # Skip CSV header line
    while IFS=',' read -r symbol price quantity side; do
        [ "$symbol" = "Symbol" ] && continue   # header row
        [ -z "$symbol" ] && continue

        cp="${COUNTERPARTIES[$((cp_index % 3))]}"
        cp_index=$((cp_index + 1))

        response=$(curl -s -X POST "$API/orders" \
            -H "Content-Type: application/json" \
            -d "{\"symbol\":\"$symbol\",\"price\":$price,\"quantity\":$quantity,\"side\":\"$side\",\"counterparty\":\"$cp\"}")

        id=$(echo "$response" | jq -r '.orderId // empty')
        if [ -n "$id" ]; then
            echo "  Submitted $side $quantity $symbol @ $price  →  #$id  [$cp]"
            submitted=$((submitted + 1))
        else
            echo "  FAILED: $side $quantity $symbol @ $price  ($response)"
        fi

        sleep "$ORDER_DELAY"
    done < "$CSV"

    echo "  ── $submitted orders submitted"

    echo "  Cancelling all open orders..."
    cancel_all_open_orders

    echo "  Waiting ${DELAY}s before next loop..."
    sleep "$DELAY"
    loop=$((loop + 1))
done
