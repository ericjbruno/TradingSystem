import { create } from 'zustand'

const useStore = create((set) => ({
  selectedSymbol: null,
  symbols: [],
  books: {},      // symbol → { symbol, bids: [{price, quantity, orderIds}], asks: [...] }
  bookTimes: {},  // symbol → timestamp (ms) of last update
  trades: [],     // array of trade objects, newest first
  connected: false,

  setSymbol:    (s)  => set({ selectedSymbol: s }),
  setSymbols:   (ss) => set({ symbols: ss }),
  setConnected: (c)  => set({ connected: c }),

  updateBook: (b) => set((state) => ({
    books:     { ...state.books,     [b.symbol]: b },
    bookTimes: { ...state.bookTimes, [b.symbol]: Date.now() },
  })),

  addTrade: (t) => set((state) => ({
    trades: [t, ...state.trades].slice(0, 100)
  })),

  initTrades: (ts) => set({
    // REST returns oldest-first (deque order); reverse so newest is first
    trades: [...ts].reverse()
  }),

  clearTrades: () => set({ trades: [] }),
}))

export default useStore
