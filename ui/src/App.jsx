import { useEffect } from 'react'
import useStore from './store/tradingStore'
import SymbolSelector from './components/SymbolSelector'
import MarketSummary from './components/MarketSummary'
import OrderBook from './components/OrderBook'
import TradeFeed from './components/TradeFeed'
import OrderForm from './components/OrderForm'

const API = 'http://localhost:9090'

export default function App() {
  const {
    selectedSymbol, books, trades, connected,
    setSymbols, updateBook, addTrade, setConnected, initTrades, setSymbol
  } = useStore()

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
        <SymbolSelector />
      </header>

      <MarketSummary />

      <main className="main">
        <section className="panel book-panel">
          <div className="panel-title">
            Order Book — <span className="symbol-label">{selectedSymbol || '—'}</span>
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
