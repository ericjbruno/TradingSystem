export default function TradeFeed({ trades }) {
  if (trades.length === 0) {
    return <div className="feed-empty">No trades yet</div>
  }

  return (
    <div className="trade-feed">
      {trades.map((t, i) => (
        <div key={i} className="trade-row">
          <span className="trade-symbol">{t.symbol}</span>
          <span className="trade-qty">{t.quantity.toLocaleString()}</span>
          <span className="trade-at">@</span>
          <span className="trade-price">{t.price.toFixed(5)}</span>
          <span className="trade-parties">
            <span className="buyer">{t.buyer}</span>
            <span className="vs"> / </span>
            <span className="seller">{t.seller}</span>
          </span>
          <span className="trade-ids">
            #{t.buyOrderId} / #{t.sellOrderId}
          </span>
        </div>
      ))}
    </div>
  )
}
