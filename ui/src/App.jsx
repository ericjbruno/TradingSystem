import { useEffect, useState } from 'react'
import useStore from './store/tradingStore'
import SymbolSelector from './components/SymbolSelector'
import MarketSummary from './components/MarketSummary'
import OrderBook from './components/OrderBook'
import TradeFeed from './components/TradeFeed'
import OrderForm from './components/OrderForm'

const API = 'http://localhost:9090'

export default function App() {
  const {
    selectedSymbol, symbols, books, bookTimes, trades, connected,
    setSymbols, updateBook, addTrade, setConnected, initTrades, setSymbol, clearTrades
  } = useStore()

  const [clearKey, setClearKey] = useState(0)

  function collectIds(allBooks) {
    return Object.values(allBooks).flatMap(b => [
      ...(b.bids || []).flatMap(level => level.orderIds || []),
      ...(b.asks || []).flatMap(level => level.orderIds || []),
    ])
  }

  async function handleClear() {
    try {
      // Cancel in up to 3 passes to catch any orders added mid-cancel
      for (let pass = 0; pass < 3; pass++) {
        const allBooks = await fetch(`${API}/books`).then(r => r.json())
        const ids = collectIds(allBooks)
        if (ids.length === 0) break
        for (const id of ids) {
          await fetch(`${API}/orders/${id}`, { method: 'DELETE' })
        }
      }
    } catch (e) {
      console.error('Clear failed:', e)
    }
    clearTrades()
    setClearKey(k => k + 1)
  }

  // Initial data fetch + SSE setup
  useEffect(() => {
    // Fetch all symbols
    fetch(`${API}/symbols`)
      .then(r => r.json())
      .then(ss => {
        setSymbols(ss)
        if (ss.length > 0) setSymbol(ss[0])
      })
      .catch(console.error)

    // Fetch all current book states in one shot
    fetch(`${API}/books`)
      .then(r => r.json())
      .then(allBooks => {
        Object.values(allBooks).forEach(b => updateBook(b))
      })
      .catch(console.error)

    // Fetch recent trade history
    fetch(`${API}/trades`)
      .then(r => r.json())
      .then(ts => initTrades(ts))
      .catch(console.error)

    // Set up SSE stream
    const es = new EventSource(`${API}/events`)
    es.onopen  = () => setConnected(true)
    es.onerror = () => setConnected(false)

    es.addEventListener('trade', e => {
      addTrade(JSON.parse(e.data))
    })
    es.addEventListener('book_update', e => {
      updateBook(JSON.parse(e.data))
    })

    return () => es.close()
  }, [])

  return (
    <div className="app">
      <header className="header">
        <h1 className="title">TradingSystem</h1>
        <div className={`status ${connected ? 'connected' : 'disconnected'}`}>
          <span className="dot">{connected ? '●' : '○'}</span>
          {connected ? 'Live' : 'Disconnected'}
        </div>
        <button className="btn clear-btn" onClick={handleClear}>Clear</button>
      </header>

      <MarketSummary symbols={symbols} books={books} trades={trades} clearKey={clearKey} />

      <main className="main">
        <section className="panel book-panel">
          <div className="panel-title">
            <span>Order Book</span>
            <SymbolSelector />
          </div>
          <OrderBook book={selectedSymbol ? books[selectedSymbol] : null} />
        </section>

        <section className="panel trades-panel">
          <div className="panel-title">Recent Trades</div>
          <TradeFeed trades={trades} />
        </section>
      </main>

      <footer className="form-footer">
        <OrderForm api={API} />
      </footer>
    </div>
  )
}
