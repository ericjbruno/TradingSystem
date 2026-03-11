import useStore from '../store/tradingStore'

export default function SymbolSelector() {
  const { symbols, selectedSymbol, setSymbol } = useStore()

  return (
    <select
      className="symbol-select"
      value={selectedSymbol || ''}
      onChange={e => setSymbol(e.target.value)}
    >
      {symbols.length === 0 && <option value="">Loading…</option>}
      {symbols.map(s => (
        <option key={s} value={s}>{s}</option>
      ))}
    </select>
  )
}
