import { useState, useEffect, useRef } from 'react';
import { motion, AnimatePresence } from 'framer-motion';
import { useAuth } from '../contexts/AuthContext';
import { useNavigate } from 'react-router-dom';
import { cardsAPI, ordersAPI } from '../services/api';
import type { Card, OrderBookSnapshot, OrderType, OrderMode, Trade } from '../types/api';
import { useWebSocket } from '../contexts/WebSocketContext';
import AnalyticsStrip from '../components/AnalyticsStrip';
import CandlestickChart from '../components/CandlestickChart';
import FairValueIndicator from '../components/FairValueIndicator';
import DepthChart from '../components/DepthChart';
import RecentTradesFeed from '../components/RecentTradesFeed';
import TradeTickerBar from '../components/TradeTickerBar';
import RankBadge from '../components/RankBadge';

type RarityFilter = 'All' | 'common' | 'rare' | 'epic' | 'legendary' | 'champion';

const RARITY_TABS: RarityFilter[] = ['All', 'common', 'rare', 'epic', 'legendary', 'champion'];
const RARITY_COLORS: Record<string, string> = {
  common: 'text-slate-400 border-slate-400',
  rare: 'text-orange-400 border-orange-400',
  epic: 'text-purple-400 border-purple-400',
  legendary: 'text-yellow-400 border-yellow-400',
  champion: 'text-cyan-400 border-cyan-400',
};

export default function Trading() {
  const { user, logout, refreshUser } = useAuth();
  const navigate = useNavigate();
  const ws = useWebSocket();

  const [cards, setCards] = useState<Card[]>([]);
  const [selectedCard, setSelectedCard] = useState<Card | null>(null);
  const [orderBook, setOrderBook] = useState<OrderBookSnapshot | null>(null);
  const [recentTrades, setRecentTrades] = useState<Trade[]>([]);
  const [tickerTrades, setTickerTrades] = useState<Trade[]>([]);

  const [rarityFilter, setRarityFilter] = useState<RarityFilter>('All');
  const [cardSearch, setCardSearch] = useState('');
  const [orderBookView, setOrderBookView] = useState<'list' | 'depth'>('list');

  const [orderType, setOrderType] = useState<OrderType>('BUY');
  const [orderMode, setOrderMode] = useState<OrderMode>('MARKET');
  const [price, setPrice] = useState('');
  const [quantity, setQuantity] = useState('1');
  const [loading, setLoading] = useState(false);
  const [fillToast, setFillToast] = useState<string | null>(null);
  const [error, setError] = useState('');

  const tickerRef = useRef<Trade[]>([]);

  useEffect(() => { loadCards(); }, []);

  useEffect(() => {
    if (selectedCard) {
      loadOrderBook(selectedCard.card_id);
      loadRecentTrades(selectedCard.card_id);
    }
  }, [selectedCard]);

  // WebSocket: order book updates
  useEffect(() => {
    if (!selectedCard) return;
    const channel = `orderbook:${selectedCard.card_id}`;
    ws.subscribe(channel);
    const handler = (data: any) => {
      if (data.card_id === selectedCard.card_id) {
        setOrderBook({ card_id: data.card_id, bids: data.bids || [], asks: data.asks || [] });
      }
    };
    ws.on('orderbook_update', handler);
    return () => { ws.unsubscribe(channel); ws.off('orderbook_update', handler); };
  }, [selectedCard, ws]);

  // WebSocket: trade events → update ticker + recent trades + refresh user
  useEffect(() => {
    if (!user) return;
    const onTrade = (data: any) => {
      const trade: Trade = {
        trade_id: data.trade_id,
        card_id: data.card_id,
        buyer_id: data.buyer_id ?? '',
        seller_id: data.seller_id ?? '',
        price: data.price,
        quantity: data.quantity,
        total_value: data.total_value,
        buyer_order_id: '',
        seller_order_id: '',
        executed_at: new Date().toISOString(),
      };
      tickerRef.current = [trade, ...tickerRef.current].slice(0, 40);
      setTickerTrades([...tickerRef.current]);
      if (selectedCard && data.card_id === selectedCard.card_id) {
        setRecentTrades(prev => [trade, ...prev].slice(0, 20));
        loadOrderBook(selectedCard.card_id);
      }
    };
    const onPortfolio = () => { refreshUser(); };
    // Resync gold on every trade (catches counterparty updates) and on WS reconnect
    const onReconnect = () => { refreshUser(); };
    ws.on('trade_executed', onTrade);
    ws.on('portfolio_update', onPortfolio);
    ws.on('ws_connected', onReconnect);
    return () => {
      ws.off('trade_executed', onTrade);
      ws.off('portfolio_update', onPortfolio);
      ws.off('ws_connected', onReconnect);
    };
  }, [user, selectedCard, ws, refreshUser]);

  const loadCards = async () => {
    try {
      const data = await cardsAPI.getAll();
      setCards(data);
      if (data.length > 0) setSelectedCard(data[0]);
    } catch { setError('Failed to load cards'); }
  };

  const loadOrderBook = async (cardId: string) => {
    try { setOrderBook(await cardsAPI.getOrderBook(cardId)); } catch {}
  };

  const loadRecentTrades = async (cardId: string) => {
    try { setRecentTrades(await cardsAPI.getTrades(cardId, 20)); } catch {}
  };

  const handlePlaceOrder = async (e: React.FormEvent) => {
    e.preventDefault();
    if (!selectedCard) return;
    setLoading(true);
    setError('');
    try {
      const resp = await ordersAPI.place({
        card_id: selectedCard.card_id,
        type: orderType,
        mode: orderMode,
        quantity: parseInt(quantity),
        ...(orderMode === 'LIMIT' && { price: parseFloat(price) }),
      });
      setQuantity('1');
      setPrice('');
      loadOrderBook(selectedCard.card_id);
      await refreshUser();
      const msg = resp.filled_quantity > 0
        ? `${resp.filled_quantity} ${selectedCard.name}(s) ${orderType === 'BUY' ? 'bought' : 'sold'}!`
        : 'Order added to book';
      setFillToast(msg);
      setTimeout(() => setFillToast(null), 3000);
    } catch (err: any) {
      setError(err.response?.data?.error || err.message || 'Failed to place order');
      setTimeout(() => setError(''), 5000);
    } finally {
      setLoading(false);
    }
  };

  const filteredCards = cards.filter(c => {
    const matchRarity = rarityFilter === 'All' || c.rarity.toLowerCase() === rarityFilter;
    const matchSearch = c.name.toLowerCase().includes(cardSearch.toLowerCase());
    return matchRarity && matchSearch;
  });

  const getBestBid = () => orderBook?.bids[0]?.[0] ?? 0;
  const getBestAsk = () => orderBook?.asks[0]?.[0] ?? 0;
  const getSpread = () => { const b = getBestBid(), a = getBestAsk(); return b && a ? a - b : 0; };
  const lastTradePrice = recentTrades[0]?.price ?? selectedCard?.price ?? 0;

  return (
    <div className="h-screen flex flex-col overflow-hidden bg-arena-bg hex-grid-bg">

      {/* ── HEADER ── */}
      <header className="h-12 flex-shrink-0 flex items-center px-4 gap-4 border-b border-arena-border bg-arena-surface/80 backdrop-blur-sm">
        <h1 className="text-sm font-display text-transparent bg-clip-text bg-gradient-to-r from-neon-gold via-orange-400 to-neon-gold uppercase tracking-widest flex-shrink-0">
          CR Exchange
        </h1>


        {selectedCard && (
          <div className="flex items-center gap-2 flex-shrink-0">
            <span className="text-white font-bold text-sm">{selectedCard.name}</span>
            <span className={`text-xs font-bold uppercase px-1.5 py-0.5 rounded border ${RARITY_COLORS[selectedCard.rarity.toLowerCase()] ?? 'text-slate-400 border-slate-400'}`}>
              {selectedCard.rarity}
            </span>
          </div>
        )}

        <div className="flex items-center gap-3 ml-auto">
          {user && (
            <RankBadge rank={user.trader_level || 'Goblin Stadium'} xp={user.xp ?? 0} compact />
          )}
          <div className="text-right">
            <p className="text-xs text-slate-500 leading-none">Gold</p>
            <p className="text-sm font-black text-neon-gold font-mono">{user?.gold_balance.toLocaleString()}</p>
          </div>
          <button onClick={() => navigate('/dashboard')} className="px-3 py-1 text-xs text-slate-400 border border-slate-700 rounded hover:border-neon-purple/50 hover:text-neon-purple transition uppercase tracking-wide">
            Dashboard
          </button>
          <button onClick={() => navigate('/portfolio')} className="px-3 py-1 text-xs text-slate-400 border border-slate-700 rounded hover:border-neon-cyan/50 hover:text-neon-cyan transition uppercase tracking-wide">
            Portfolio
          </button>
          <button onClick={() => { logout(); navigate('/login'); }} className="px-3 py-1 text-xs text-bear-red/70 border border-bear-red/30 rounded hover:border-bear-red hover:text-bear-red transition uppercase tracking-wide">
            Logout
          </button>
        </div>
      </header>

      {/* ── MAIN 3-COLUMN BODY ── */}
      <div className="flex-1 flex overflow-hidden min-h-0">

        {/* LEFT: Card Browser */}
        <div className="w-56 flex-shrink-0 flex flex-col border-r border-arena-border overflow-hidden">
          {/* Rarity filter tabs */}
          <div className="flex-shrink-0 px-2 pt-2 pb-0 flex flex-wrap gap-1">
            {RARITY_TABS.map(r => (
              <button
                key={r}
                onClick={() => setRarityFilter(r)}
                className={`px-2 py-0.5 text-[10px] font-bold uppercase rounded border transition ${
                  rarityFilter === r
                    ? r === 'All'
                      ? 'bg-slate-700 text-white border-slate-500'
                      : `bg-transparent text-${r === 'champion' ? 'cyan' : r === 'legendary' ? 'yellow' : r === 'epic' ? 'purple' : r === 'rare' ? 'orange' : 'slate'}-400 border-${r === 'champion' ? 'cyan' : r === 'legendary' ? 'yellow' : r === 'epic' ? 'purple' : r === 'rare' ? 'orange' : 'slate'}-400`
                    : 'text-slate-600 border-slate-800 hover:text-slate-400'
                }`}
              >
                {r === 'All' ? 'All' : r}
              </button>
            ))}
          </div>

          {/* Search */}
          <div className="px-2 pt-2 flex-shrink-0">
            <input
              type="text"
              placeholder="Search…"
              value={cardSearch}
              onChange={e => setCardSearch(e.target.value)}
              className="w-full px-2 py-1.5 bg-slate-950/60 border border-slate-700/50 rounded text-xs text-white placeholder-slate-600 focus:outline-none focus:border-neon-purple/50"
            />
          </div>

          {/* Card list — compact single-column terminal rows */}
          <div className="flex-1 overflow-y-auto arena-scroll py-1">
            {filteredCards.map(card => {
              const rarity = card.rarity.toLowerCase();
              const isSelected = selectedCard?.card_id === card.card_id;
              const dotColor: Record<string, string> = {
                common: 'bg-slate-400', rare: 'bg-orange-400',
                epic: 'bg-purple-400', legendary: 'bg-yellow-400', champion: 'bg-cyan-400',
              };
              return (
                <button
                  key={card.card_id}
                  onClick={() => setSelectedCard(card)}
                  className={`w-full flex items-center gap-2 px-3 py-1.5 text-left transition-colors ${
                    isSelected
                      ? 'bg-slate-800/80 border-l-2 border-neon-purple'
                      : 'border-l-2 border-transparent hover:bg-white/3 hover:border-slate-600'
                  }`}
                >
                  <span className={`w-1.5 h-1.5 rounded-full flex-shrink-0 ${dotColor[rarity] ?? 'bg-slate-400'}`} />
                  <span className={`text-xs flex-1 truncate ${isSelected ? 'text-white font-bold' : 'text-slate-400'}`}>
                    {card.name}
                  </span>
                  <span className="text-[10px] font-mono text-slate-600 flex-shrink-0">{card.price}g</span>
                </button>
              );
            })}
          </div>
        </div>

        {/* CENTER: Chart + Analytics + Order Form */}
        <div className="flex-1 flex flex-col overflow-hidden min-w-0">

          {/* Fair value bar */}
          {selectedCard && (
            <FairValueIndicator cardId={selectedCard.card_id} lastPrice={lastTradePrice} />
          )}

          {/* Candlestick chart — fixed portion of the height */}
          <div className="h-[280px] flex-shrink-0 border-b border-arena-border">
            {selectedCard ? (
              <CandlestickChart cardId={selectedCard.card_id} cardName={selectedCard.name} />
            ) : (
              <div className="flex items-center justify-center h-full text-slate-600 text-sm">Select a card</div>
            )}
          </div>

          {/* Analytics strip — scrolls if it overflows */}
          {selectedCard && (
            <div className="flex-1 overflow-y-auto arena-scroll border-b border-arena-border min-h-0">
              <AnalyticsStrip cardId={selectedCard.card_id} />
            </div>
          )}

          {/* Order form — always visible, pinned to bottom */}
          <div className="flex-shrink-0 p-3 border-t border-arena-border bg-arena-bg/50">
            <form onSubmit={handlePlaceOrder} className="space-y-2">
              {/* BUY / SELL */}
              <div className="grid grid-cols-2 gap-2">
                {(['BUY', 'SELL'] as OrderType[]).map(t => (
                  <button
                    key={t}
                    type="button"
                    onClick={() => setOrderType(t)}
                    className={`py-2 rounded-lg font-black uppercase tracking-wider text-sm border-2 transition-all ${
                      orderType === t
                        ? t === 'BUY'
                          ? 'bg-bull-green text-black border-bull-green shadow-lg shadow-bull-green/30'
                          : 'bg-bear-red text-white border-bear-red shadow-lg shadow-bear-red/30'
                        : 'bg-slate-800/50 text-slate-500 border-slate-700 hover:border-slate-500'
                    }`}
                  >
                    {t}
                  </button>
                ))}
              </div>

              {/* MARKET / LIMIT */}
              <div className="grid grid-cols-2 gap-2">
                {(['MARKET', 'LIMIT'] as OrderMode[]).map(m => (
                  <button
                    key={m}
                    type="button"
                    onClick={() => setOrderMode(m)}
                    className={`py-1 rounded text-xs font-bold uppercase tracking-wide border transition ${
                      orderMode === m
                        ? 'bg-neon-purple/20 text-neon-purple border-neon-purple/50'
                        : 'bg-transparent text-slate-500 border-slate-700 hover:border-slate-500'
                    }`}
                  >
                    {m}
                  </button>
                ))}
              </div>

              <div className="grid grid-cols-2 gap-2">
                {orderMode === 'LIMIT' && (
                  <div>
                    <label className="block text-[10px] text-slate-500 uppercase tracking-wide mb-1">Price</label>
                    <input
                      type="number" value={price} onChange={e => setPrice(e.target.value)}
                      required min="0.01" step="0.01"
                      className="w-full px-2 py-1.5 bg-slate-950/50 border border-slate-700 rounded text-white text-sm font-mono focus:outline-none focus:border-neon-purple/50"
                      placeholder="0.00"
                    />
                  </div>
                )}
                <div className={orderMode === 'LIMIT' ? '' : 'col-span-2'}>
                  <label className="block text-[10px] text-slate-500 uppercase tracking-wide mb-1">Quantity</label>
                  <input
                    type="number" value={quantity} onChange={e => setQuantity(e.target.value)}
                    required min="1"
                    className="w-full px-2 py-1.5 bg-slate-950/50 border border-slate-700 rounded text-white text-sm font-mono focus:outline-none focus:border-neon-purple/50"
                    placeholder="1"
                  />
                </div>
              </div>

              {error && (
                <p className="text-xs text-bear-red text-center font-bold">{error}</p>
              )}

              <div className="relative">
                <button
                  type="submit"
                  disabled={loading || !selectedCard}
                  className={`w-full py-2.5 font-black text-sm uppercase tracking-wider rounded-lg border-2 transition-all disabled:opacity-40 ${
                    orderType === 'BUY'
                      ? 'bg-bull-green/20 text-bull-green border-bull-green/50 hover:bg-bull-green/30'
                      : 'bg-bear-red/20 text-bear-red border-bear-red/50 hover:bg-bear-red/30'
                  }`}
                >
                  {loading ? 'Processing…' : `${orderType} ${quantity} ${selectedCard?.name ?? 'Card'}`}
                </button>

                {/* Fill toast */}
                <AnimatePresence>
                  {fillToast && (
                    <motion.div
                      initial={{ opacity: 0, y: 0, scale: 0.9 }}
                      animate={{ opacity: 1, y: -40, scale: 1 }}
                      exit={{ opacity: 0, y: -60 }}
                      className="absolute left-1/2 -translate-x-1/2 bottom-0 bg-slate-900 border border-bull-green/50 text-bull-green text-xs font-bold px-4 py-1.5 rounded-full whitespace-nowrap shadow-lg shadow-bull-green/20"
                    >
                      ✓ {fillToast}
                    </motion.div>
                  )}
                </AnimatePresence>
              </div>
            </form>
          </div>
        </div>

        {/* RIGHT: Order Book + Recent Trades */}
        <div className="w-72 flex-shrink-0 flex flex-col border-l border-arena-border overflow-hidden">

          {/* Order book header + view toggle */}
          <div className="flex-shrink-0 flex items-center justify-between px-3 py-2 border-b border-arena-border">
            <p className="text-xs text-slate-500 uppercase tracking-widest font-bold">Order Book</p>
            <div className="flex gap-1">
              {(['list', 'depth'] as const).map(v => (
                <button
                  key={v}
                  onClick={() => setOrderBookView(v)}
                  className={`px-2 py-0.5 text-[10px] rounded font-bold uppercase transition ${
                    orderBookView === v ? 'bg-neon-purple/20 text-neon-purple' : 'text-slate-600 hover:text-slate-400'
                  }`}
                >
                  {v}
                </button>
              ))}
            </div>
          </div>

          {/* Spread row */}
          {orderBook && (
            <div className="flex-shrink-0 grid grid-cols-3 gap-1 px-3 py-2 border-b border-arena-border text-center">
              <div>
                <p className="text-[9px] text-slate-600 uppercase">Bid</p>
                <p className="text-xs font-mono font-bold text-bull-green">{getBestBid() || '—'}</p>
              </div>
              <div>
                <p className="text-[9px] text-slate-600 uppercase">Spread</p>
                <p className="text-xs font-mono font-bold text-neon-gold">{getSpread().toFixed(1)}</p>
              </div>
              <div>
                <p className="text-[9px] text-slate-600 uppercase">Ask</p>
                <p className="text-xs font-mono font-bold text-bear-red">{getBestAsk() || '—'}</p>
              </div>
            </div>
          )}

          {/* Order book body — depth chart or list */}
          <div className="flex-1 min-h-0 overflow-hidden border-b border-arena-border">
            {orderBookView === 'depth' ? (
              <div className="h-full p-2">
                <DepthChart bids={orderBook?.bids ?? []} asks={orderBook?.asks ?? []} />
              </div>
            ) : (
              <div className="h-full overflow-y-auto arena-scroll">
                {/* Asks (reversed so highest ask is at top) */}
                <div className="px-2 pt-2">
                  <p className="text-[9px] text-bear-red/70 uppercase tracking-widest mb-1 font-bold">Asks</p>
                  {orderBook?.asks.slice(0, 8).map(([p, q], i) => (
                    <div key={i} className="relative flex justify-between py-0.5 px-1 text-xs">
                      <div
                        className="absolute inset-0 bg-bear-red/8 rounded"
                        style={{ width: `${Math.min((q / (orderBook.asks[0]?.[1] ?? 1)) * 80, 100)}%` }}
                      />
                      <span className="relative text-bear-red font-mono">{p.toFixed(1)}</span>
                      <span className="relative text-slate-400">{q}</span>
                    </div>
                  ))}
                  {(!orderBook || orderBook.asks.length === 0) && (
                    <p className="text-slate-600 text-[10px] text-center py-2">No asks</p>
                  )}
                </div>
                {/* Bids */}
                <div className="px-2 pt-2">
                  <p className="text-[9px] text-bull-green/70 uppercase tracking-widest mb-1 font-bold">Bids</p>
                  {orderBook?.bids.slice(0, 8).map(([p, q], i) => (
                    <div key={i} className="relative flex justify-between py-0.5 px-1 text-xs">
                      <div
                        className="absolute inset-0 bg-bull-green/8 rounded"
                        style={{ width: `${Math.min((q / (orderBook.bids[0]?.[1] ?? 1)) * 80, 100)}%` }}
                      />
                      <span className="relative text-bull-green font-mono">{p.toFixed(1)}</span>
                      <span className="relative text-slate-400">{q}</span>
                    </div>
                  ))}
                  {(!orderBook || orderBook.bids.length === 0) && (
                    <p className="text-slate-600 text-[10px] text-center py-2">No bids</p>
                  )}
                </div>
              </div>
            )}
          </div>

          {/* Recent trades feed */}
          <div className="flex-1 min-h-0 overflow-hidden">
            <RecentTradesFeed trades={recentTrades} currentUserId={user?.user_id} />
          </div>
        </div>
      </div>

      {/* ── TRADE TICKER ── */}
      <TradeTickerBar trades={tickerTrades} cards={cards} />
    </div>
  );
}
