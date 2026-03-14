import { useEffect, useRef, useState } from 'react'

function fmtPrice(price) {
  if (price == null) return '—'
  if (price >= 100) return price.toFixed(2)
  if (price >= 10)  return price.toFixed(4)
  return price.toFixed(5)
}

function fmtQty(n) {
  if (n == null) return '—'
  return n.toLocaleString()
}

function fmtTime(ms) {
  if (!ms) return '—'
  return new Date(ms).toLocaleTimeString('en-US', {
    hour12: false, hour: '2-digit', minute: '2-digit', second: '2-digit'
  })
}

// Returns { sym: lastPrice } map from trades array (newest-first)
function buildLastTrades(trades) {
  const result = {}
  for (const t of trades) {
    if (!(t.symbol in result)) result[t.symbol] = t.price
  }
  return result
}

export default function MarketSummary({ symbols, books, trades, clearKey }) {
  const [flashing, setFlashing]   = useState({})
  const [tradeTimes, setTradeTimes] = useState({})  // sym → ms timestamp of last trade
  const prevRef   = useRef({})   // { sym: { bid, ask, bidQty, lastTrade } }
  const timersRef = useRef({})

  // Flash helper — starts 2 s gold timer for a key
  function triggerFlash(key) {
    if (timersRef.current[key]) clearTimeout(timersRef.current[key])
    setFlashing(f => ({ ...f, [key]: true }))
    timersRef.current[key] = setTimeout(() => {
      setFlashing(f => { const n = { ...f }; delete n[key]; return n })
      delete timersRef.current[key]
    }, 2000)
  }

  // Watch book changes (bid, ask, qty)
  useEffect(() => {
    for (const sym of symbols) {
      const book   = books[sym]
      if (!book) continue
      const bid    = book.bids?.[0]?.price
      const ask    = book.asks?.[0]?.price
      const bidQty = book.bids?.[0]?.quantity
      const prev   = prevRef.current[sym] || {}

      if (bid    !== undefined && bid    !== prev.bid)    triggerFlash(`${sym}.bid`)
      if (ask    !== undefined && ask    !== prev.ask)    triggerFlash(`${sym}.ask`)
      if (bidQty !== undefined && bidQty !== prev.bidQty) triggerFlash(`${sym}.qty`)

      prevRef.current[sym] = { ...prev, bid, ask, bidQty }
    }
  }, [books, symbols])

  // Watch trade changes (lastTrade)
  useEffect(() => {
    const latest = buildLastTrades(trades)
    for (const sym of symbols) {
      const price = latest[sym]
      const prev  = prevRef.current[sym] || {}
      if (price !== undefined && price !== prev.lastTrade) {
        triggerFlash(`${sym}.last`)
        // Save old price as prevLastTrade BEFORE overwriting lastTrade,
        // so the render can compare them even after this effect has run.
        prevRef.current[sym] = {
          ...prevRef.current[sym],
          prevLastTrade: prev.lastTrade,
          lastTrade: price,
        }
        setTradeTimes(t => ({ ...t, [sym]: Date.now() }))
      }
    }
  }, [trades, symbols])

  // Reset trade columns when Clear is pressed
  useEffect(() => {
    if (clearKey === 0) return
    setTradeTimes({})
    for (const sym of Object.keys(prevRef.current)) {
      prevRef.current[sym] = { ...prevRef.current[sym], lastTrade: undefined, prevLastTrade: undefined }
    }
  }, [clearKey])

  useEffect(() => {
    return () => Object.values(timersRef.current).forEach(clearTimeout)
  }, [])

  const flash = (key) => flashing[key] ? 'flash' : ''

  const lastTrades = buildLastTrades(trades)

  return (
    <section className="market-summary">
      <div className="ms-header">
        <span>Name</span>
        <span className="ms-col-num">Best Bid</span>
        <span className="ms-col-num">Best Ask</span>
        <span className="ms-col-num">Quantity</span>
        <span className="ms-col-num">Last Trade</span>
        <span className="ms-col-time">Time</span>
      </div>
      <div className="ms-body">
        {symbols.map(sym => {
          const book      = books[sym]
          const bid       = book?.bids?.[0]?.price
          const ask       = book?.asks?.[0]?.price
          const bidQty    = book?.bids?.[0]?.quantity
          const lastPrice = lastTrades[sym]
          const prevLast  = prevRef.current[sym]?.prevLastTrade

          let arrow = null
          if (lastPrice != null && prevLast != null && lastPrice !== prevLast) {
            arrow = lastPrice > prevLast
              ? <span className="ms-arrow up">▲</span>
              : <span className="ms-arrow down">▼</span>
          }

          return (
            <div key={sym} className="ms-row">
              <span className="ms-name">{sym}</span>
              <span className={`ms-cell ms-col-num ${flash(`${sym}.bid`)}`}>{fmtPrice(bid)}</span>
              <span className={`ms-cell ms-col-num ${flash(`${sym}.ask`)}`}>{fmtPrice(ask)}</span>
              <span className={`ms-cell ms-col-num ${flash(`${sym}.qty`)}`}>{fmtQty(bidQty)}</span>
              <span className={`ms-cell ms-col-num ms-last ${flash(`${sym}.last`)}`}>
                {fmtPrice(lastPrice)}{arrow}
              </span>
              <span className="ms-cell ms-col-time ms-time">{fmtTime(tradeTimes[sym])}</span>
            </div>
          )
        })}
        {symbols.length === 0 && <div className="ms-empty">No instruments</div>}
      </div>
    </section>
  )
}
