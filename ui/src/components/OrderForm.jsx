import { useState, useEffect } from 'react'
import useStore from '../store/tradingStore'

const COUNTERPARTIES = ['Goldman Sachs', 'JP Morgan', 'Deutsche Bank']

export default function OrderForm({ api }) {
  const { symbols, selectedSymbol } = useStore()

  const [form, setForm] = useState({
    symbol:       '',
    price:        '',
    quantity:     '',
    side:         'BUY',
    counterparty: 'Goldman Sachs',
  })
  const [status, setStatus] = useState(null)  // null | { ok, msg }
  const [cancelId, setCancelId] = useState('')

  // Keep symbol in sync with the global selection
  useEffect(() => {
    if (selectedSymbol) setForm(f => ({ ...f, symbol: selectedSymbol }))
  }, [selectedSymbol])

  function set(field, value) {
    setForm(f => ({ ...f, [field]: value }))
  }

  async function submitOrder() {
    const price    = parseFloat(form.price)
    const quantity = parseInt(form.quantity, 10)

    if (!form.symbol || isNaN(price) || isNaN(quantity) || quantity <= 0) {
      setStatus({ ok: false, msg: 'Fill in symbol, price, and quantity.' })
      return
    }

    try {
      const res = await fetch(`${api}/orders`, {
        method:  'POST',
        headers: { 'Content-Type': 'application/json' },
        body:    JSON.stringify({
          symbol:       form.symbol,
          price,
          quantity,
          side:         form.side,
          counterparty: form.counterparty,
        }),
      })
      const json = await res.json()
      if (json.success) {
        setStatus({ ok: true, msg: `Order #${json.orderId} submitted` })
        set('price', '')
        set('quantity', '')
      } else {
        setStatus({ ok: false, msg: json.error || 'Server error' })
      }
    } catch (e) {
      setStatus({ ok: false, msg: 'Cannot reach server' })
    }

    setTimeout(() => setStatus(null), 3000)
  }

  async function cancelOrder() {
    const id = parseInt(cancelId, 10)
    if (isNaN(id) || id <= 0) {
      setStatus({ ok: false, msg: 'Enter a valid order ID to cancel.' })
      return
    }
    try {
      await fetch(`${api}/orders/${id}`, { method: 'DELETE' })
      setStatus({ ok: true, msg: `Order #${id} cancelled` })
      setCancelId('')
    } catch {
      setStatus({ ok: false, msg: 'Cannot reach server' })
    }
    setTimeout(() => setStatus(null), 3000)
  }

  return (
    <div className="order-form">
      <div className="form-title">Submit Order</div>

      <div className="form-row">
        <label>Symbol</label>
        <select value={form.symbol} onChange={e => set('symbol', e.target.value)}>
          {symbols.map(s => <option key={s} value={s}>{s}</option>)}
        </select>

        <label>Price</label>
        <input
          type="number"
          step="0.00001"
          placeholder="e.g. 1.08420"
          value={form.price}
          onChange={e => set('price', e.target.value)}
        />

        <label>Quantity</label>
        <input
          type="number"
          step="1"
          placeholder="e.g. 10000"
          value={form.quantity}
          onChange={e => set('quantity', e.target.value)}
        />

        <label>Side</label>
        <select value={form.side} onChange={e => set('side', e.target.value)}>
          <option value="BUY">BUY</option>
          <option value="SELL">SELL</option>
        </select>

        <label>Counterparty</label>
        <select value={form.counterparty} onChange={e => set('counterparty', e.target.value)}>
          {COUNTERPARTIES.map(cp => <option key={cp} value={cp}>{cp}</option>)}
        </select>

        <button
          className={`btn submit-btn ${form.side === 'BUY' ? 'buy' : 'sell'}`}
          onClick={submitOrder}
        >
          {form.side === 'BUY' ? 'Buy' : 'Sell'}
        </button>
      </div>

      <div className="form-row cancel-row">
        <label>Cancel Order #</label>
        <input
          type="number"
          placeholder="order ID"
          value={cancelId}
          onChange={e => setCancelId(e.target.value)}
        />
        <button className="btn cancel-btn" onClick={cancelOrder}>Cancel</button>
      </div>

      {status && (
        <div className={`form-status ${status.ok ? 'ok' : 'err'}`}>
          {status.msg}
        </div>
      )}
    </div>
  )
}
