import { useEffect, useRef, useState } from 'react'
import useStore from '../store/tradingStore'

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

export default function MarketSummary() {
  const { symbols, books, bookTimes } = useStore()
  const [flashing, setFlashing] = useState({})
  const prevRef   = useRef({})
  const timersRef = useRef({})

  useEffect(() => {
    for (const sym of symbols) {
      const book   = books[sym]
      if (!book) continue
      const bid    = book.bids?.[0]?.price
      const ask    = book.asks?.[0]?.price
      const bidQty = book.bids?.[0]?.quantity
      const prev   = prevRef.current[sym] || {}

      const changed = []
      if (bid    !== undefined && bid    !== prev.bid)    changed.push(`${sym}.bid`)
      if (ask    !== undefined && ask    !== prev.ask)    changed.push(`${sym}.ask`)
      if (bidQty !== undefined && bidQty !== prev.bidQty) changed.push(`${sym}.qty`)

      prevRef.current[sym] = { bid, ask, bidQty }

      for (const key of changed) {
        if (timersRef.current[key]) clearTimeout(timersRef.current[key])
        setFlashing(f => ({ ...f, [key]: true }))
        timersRef.current[key] = setTimeout(() => {
          setFlashing(f => { const n = { ...f }; delete n[key]; return n })
          delete timersRef.current[key]
        }, 2000)
      }
    }
  }, [books, symbols])

  useEffect(() => {
    return () => Object.values(timersRef.current).forEach(clearTimeout)
  }, [])

  const flash = (key) => flashing[key] ? 'flash' : ''

  return (
    <section className="market-summary">
      <div className="ms-header">
        <span>Name</span>
        <span className="ms-col-num">Best Bid</span>
        <span className="ms-col-num">Best Ask</span>
        <span className="ms-col-num">Quantity</span>
        <span className="ms-col-time">Time</span>
      </div>
      <div className="ms-body">
        {symbols.map(sym => {
          const book   = books[sym]
          const bid    = book?.bids?.[0]?.price
          const ask    = book?.asks?.[0]?.price
          const bidQty = book?.bids?.[0]?.quantity
          return (
            <div key={sym} className="ms-row">
              <span className="ms-name">{sym}</span>
              <span className={`ms-cell ms-col-num ${flash(`${sym}.bid`)}`}>{fmtPrice(bid)}</span>
              <span className={`ms-cell ms-col-num ${flash(`${sym}.ask`)}`}>{fmtPrice(ask)}</span>
              <span className={`ms-cell ms-col-num ${flash(`${sym}.qty`)}`}>{fmtQty(bidQty)}</span>
              <span className="ms-cell ms-col-time ms-time">{fmtTime(bookTimes?.[sym])}</span>
            </div>
          )
        })}
        {symbols.length === 0 && <div className="ms-empty">No instruments</div>}
      </div>
    </section>
  )
}
