function fmt(price) {
  return price.toFixed(5)
}

function fmtQty(qty) {
  return qty.toLocaleString()
}

function PriceLevel({ level, side, isBest }) {
  return (
    <div className={`level ${side}${isBest ? ' best' : ''}`}>
      <span className="level-price">{fmt(level.price)}</span>
      <span className="level-qty">{fmtQty(level.quantity)}</span>
      <span className="level-ids">
        {level.orderIds.map(id => `#${id}`).join(' ')}
      </span>
      {isBest && <span className="best-tag">best {side}</span>}
    </div>
  )
}

export default function OrderBook({ book }) {
  if (!book) {
    return <div className="book-empty">Select a symbol to view the order book</div>
  }

  const { bids, asks } = book

  // Asks display: lowest ask at bottom (nearest to spread), so reverse for display
  const asksDisplay = [...asks].reverse()

  return (
    <div className="order-book">
      <div className="book-header">
        <span>Price</span>
        <span>Quantity</span>
        <span>Orders</span>
      </div>

      <div className="asks-section">
        {asks.length === 0
          ? <div className="side-empty">No asks</div>
          : asksDisplay.map((level, i) => (
              <PriceLevel
                key={level.price}
                level={level}
                side="ask"
                isBest={i === asksDisplay.length - 1}
              />
            ))
        }
      </div>

      <div className="spread-bar">— SPREAD —</div>

      <div className="bids-section">
        {bids.length === 0
          ? <div className="side-empty">No bids</div>
          : bids.map((level, i) => (
              <PriceLevel
                key={level.price}
                level={level}
                side="bid"
                isBest={i === 0}
              />
            ))
        }
      </div>
    </div>
  )
}
