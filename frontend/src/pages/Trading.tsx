import { useState, useEffect } from 'react';
import { useAuth } from '../contexts/AuthContext';
import { useNavigate } from 'react-router-dom';
import { cardsAPI, ordersAPI } from '../services/api';
import type { Card, OrderBookSnapshot, OrderType, OrderMode } from '../types/api';
import { useWebSocket } from '../contexts/WebSocketContext';

export default function Trading() {
  const { user, logout, refreshUser } = useAuth();
  const navigate = useNavigate();
  const ws = useWebSocket();
  
  const [cards, setCards] = useState<Card[]>([]);
  const [selectedCard, setSelectedCard] = useState<Card | null>(null);
  const [selectedCardDetail, setSelectedCardDetail] = useState<Card | null>(null);
  const [orderBook, setOrderBook] = useState<OrderBookSnapshot | null>(null);
  
  const [orderType, setOrderType] = useState<OrderType>('BUY');
  const [orderMode, setOrderMode] = useState<OrderMode>('MARKET');
  const [price, setPrice] = useState<string>('');
  const [quantity, setQuantity] = useState<string>('1');
  
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string>('');
  const [success, setSuccess] = useState<string>('');
  const [cardSearchQuery, setCardSearchQuery] = useState('');

  useEffect(() => {
    loadCards();
  }, []);

  useEffect(() => {
    if (selectedCard) {
      loadOrderBook(selectedCard.card_id);
      loadCardDetail(selectedCard.card_id);
    }
  }, [selectedCard]);

  // WebSocket: Subscribe to order book updates for selected card
  useEffect(() => {
    if (!selectedCard) return;

    const channel = `orderbook:${selectedCard.card_id}`;
  
    console.log('[Trading] Subscribing to WebSocket channel:', channel);
    ws.subscribe(channel);

    // Listen for order book updates
    const handleOrderBookUpdate = (data: any) => {
      console.log('[Trading] Order book update received:', data);
      if (data.card_id === selectedCard.card_id) {
        setOrderBook({
          card_id: data.card_id,
          bids: data.bids || [],
          asks: data.asks || [],
        });
      }
    };

    ws.on('orderbook_update', handleOrderBookUpdate);

    // Cleanup on unmount or when card changes
    return () => {
      console.log('[Trading] Unsubscribing from:', channel);
      ws.unsubscribe(channel);
      ws.off('orderbook_update', handleOrderBookUpdate);
    };
  }, [selectedCard, ws]);

  // WebSocket: Subscribe to trade notifications
  useEffect(() => {
    if (!user) return;

    // Listen for trades
    const handleTradeExecuted = (data: any) => {
      console.log('[Trading] Trade executed:', data);
      // Show a notification or update UI
      if (selectedCard && data.card_id === selectedCard.card_id) {
        // Reload order book after trade
        loadOrderBook(selectedCard.card_id);
      }
    };

    ws.on('trade_executed', handleTradeExecuted);

    // Listen for portfolio updates
    const handlePortfolioUpdate = (data: any) => {
      console.log('[Trading] Portfolio update:', data);
      // Refresh user balance
    refreshUser();
  };

    ws.on('portfolio_update', handlePortfolioUpdate);

    return () => {
      ws.off('trade_executed', handleTradeExecuted);
      ws.off('portfolio_update', handlePortfolioUpdate);
    };
  }, [user, selectedCard, ws, refreshUser]);

  const loadCardDetail = async (cardId: string) => {
    try {
      const data = await cardsAPI.getById(cardId);
      setSelectedCardDetail(data);
    } catch (err) {
      console.error('Failed to load card detail:', err);
    }
  };

  const loadCards = async () => {
    try {
      const data = await cardsAPI.getAll();
      setCards(data);
      if (data.length > 0) {
        setSelectedCard(data[0]);
      }
    } catch (err) {
      console.error('Failed to load cards:', err);
      setError('Failed to load cards');
    }
  };

  const loadOrderBook = async (cardId: string) => {
    try {
      const data = await cardsAPI.getOrderBook(cardId);
      setOrderBook(data);
    } catch (err) {
      console.error('Failed to load order book:', err);
    }
  };

  const handlePlaceOrder = async (e: React.FormEvent) => {
    e.preventDefault();
    
    if (!selectedCard) {
      setError('Please select a card');
      return;
    }

    setLoading(true);
    setError('');
    setSuccess('');

    try {
      const orderData = {
        card_id: selectedCard.card_id,
        type: orderType,
        mode: orderMode,
        quantity: parseInt(quantity),
        ...(orderMode === 'LIMIT' && { price: parseFloat(price) })
      };

      const response = await ordersAPI.place(orderData);
      
      // Better success message
      if (response.filled_quantity > 0) {
        setSuccess(`✓ ${response.filled_quantity} card(s) ${orderType === 'BUY' ? 'bought' : 'sold'}!`);
      } else {
        setSuccess(`✓ Order placed and added to book`);
      }
      
      setQuantity('1');
      setPrice('');
      
      // Reload data
      loadOrderBook(selectedCard.card_id);
      await refreshUser();  
      
      setTimeout(() => setSuccess(''), 3000);
    } catch (err: any) {
      console.error('Order error:', err);  
      const message = err.response?.data?.error || err.message || 'Failed to place order';
      setError(message);
      setTimeout(() => setError(''), 5000);
    } finally {
      setLoading(false);
    }
  };

  const handleLogout = () => {
    logout();
    navigate('/login');
  };

  const getRarityColor = (rarity: string) => {
    switch (rarity.toLowerCase()) {
      case 'common': return 'from-slate-400 to-slate-500';
      case 'rare': return 'from-orange-400 to-orange-500';
      case 'epic': return 'from-purple-400 to-purple-500';
      case 'legendary': return 'from-yellow-400 to-yellow-500';
      case 'champion': return 'from-blue-400 to-cyan-400';
      default: return 'from-slate-400 to-slate-500';
    }
  };

  const getRarityBorder = (rarity: string) => {
    switch (rarity.toLowerCase()) {
      case 'common': return 'border-slate-400/30';
      case 'rare': return 'border-orange-400/30';
      case 'epic': return 'border-purple-400/30';
      case 'legendary': return 'border-yellow-400/30';
      case 'champion': return 'border-cyan-400/30';
      default: return 'border-slate-400/30';
    }
  };

  const filteredCards = cards.filter(card =>
    card.name.toLowerCase().includes(cardSearchQuery.toLowerCase())
  );

  const getBestBid = () => orderBook?.bids[0]?.[0] || 0;
  const getBestAsk = () => orderBook?.asks[0]?.[0] || 0;
  const getSpread = () => {
    const bid = getBestBid();
    const ask = getBestAsk();
    return bid && ask ? ask - bid : 0;
  };

  return (
    <div className="min-h-screen bg-gradient-to-br from-slate-950 via-orange-950/20 to-purple-950/20">
      
      {/* Retro Gaming Header */}
      <div className="border-b-4 border-orange-500/50 bg-gradient-to-r from-slate-900 via-orange-900/20 to-purple-900/20">
        <div className="max-w-7xl mx-auto px-4 py-4">
          <div className="flex justify-between items-center">
            
            {/* Title with pixel-inspired styling */}
            <div>
              <h1 className="text-3xl font-black text-transparent bg-clip-text bg-gradient-to-r from-orange-400 via-yellow-400 to-orange-500 tracking-wider uppercase">
                Card Trading Arena
              </h1>
              <p className="text-orange-300/60 text-sm font-bold tracking-wide">LIVE MARKETPLACE</p>
            </div>

            {/* Player Stats */}
            <div className="flex items-center gap-6">
              <div className="text-right bg-slate-900/50 px-6 py-3 rounded-lg border-2 border-yellow-500/30">
                <p className="text-xs text-yellow-400/70 font-bold uppercase tracking-wide">Gold Balance</p>
                <p className="text-2xl font-black text-yellow-400 tracking-wider">
                  {user?.gold_balance.toLocaleString()}
                </p>
              </div>
              
              <button
                onClick={() => navigate('/dashboard')}
                className="px-4 py-2 bg-purple-600/20 text-purple-300 border-2 border-purple-500/50 
                         rounded-lg hover:bg-purple-600/30 font-bold uppercase text-sm tracking-wide transition"
              >
                Dashboard
              </button>

              <button
                onClick={handleLogout}
                className="px-4 py-2 bg-red-600/20 text-red-300 border-2 border-red-500/50 
                         rounded-lg hover:bg-red-600/30 font-bold uppercase text-sm tracking-wide transition"
              >
                Logout
              </button>
            </div>
          </div>
        </div>
      </div>

      {/* Main Trading Area */}
      <div className="max-w-7xl mx-auto px-4 py-6">
        <div className="grid lg:grid-cols-12 gap-6">
          
          {/* LEFT: Card Browser */}
          <div className="lg:col-span-4 space-y-4">
            
            {/* Card Search */}
            <div className="bg-slate-900/80 border-2 border-orange-500/30 rounded-xl p-4 backdrop-blur-sm">
              <input
                type="text"
                placeholder="🔍 Search cards..."
                value={cardSearchQuery}
                onChange={(e) => setCardSearchQuery(e.target.value)}
                className="w-full px-4 py-3 bg-slate-950/50 border-2 border-slate-700 rounded-lg 
                         text-white placeholder-slate-500 focus:outline-none focus:border-orange-500 
                         font-semibold"
              />
            </div>

            {/* Card Grid */}
            <div className="bg-slate-900/80 border-2 border-orange-500/30 rounded-xl p-4 backdrop-blur-sm max-h-[600px] overflow-y-auto">
              <h2 className="text-orange-400 font-black uppercase tracking-wider mb-4 text-lg">
                Select Card ({filteredCards.length})
              </h2>
              
              <div className="grid grid-cols-2 gap-3">
                {filteredCards.map(card => (
                  <button
                    key={card.card_id}
                    onClick={() => setSelectedCard(card)}
                    className={`relative p-3 rounded-lg border-2 transition-all transform hover:scale-105 ${
                      selectedCard?.card_id === card.card_id
                        ? `bg-gradient-to-br ${getRarityColor(card.rarity)} border-white shadow-lg shadow-orange-500/50`
                        : `bg-slate-800/50 ${getRarityBorder(card.rarity)} hover:border-orange-400`
                    }`}
                  >
                    {card.icon_url && (
                      <img 
                        src={card.icon_url} 
                        alt={card.name}
                        className="w-full h-20 object-contain mb-2"
                      />
                    )}
                    <p className={`text-xs font-bold text-center ${
                      selectedCard?.card_id === card.card_id ? 'text-white' : 'text-slate-300'
                    }`}>
                      {card.name}
                    </p>
                    <p className={`text-xs text-center capitalize ${
                      selectedCard?.card_id === card.card_id ? 'text-white/80' : 'text-slate-400'
                    }`}>
                      {card.rarity}
                    </p>
                  </button>
                ))}
              </div>
            </div>
          </div>

          {/* CENTER: Selected Card + Order Form */}
          <div className="lg:col-span-5 space-y-4">
            
            {/* Selected Card Display */}
            {selectedCard && (
              <div className={`bg-gradient-to-br ${getRarityColor(selectedCard.rarity)} p-1 rounded-xl`}>
                <div className="bg-slate-900 rounded-lg p-6">
                  <div className="flex items-center gap-6">
                    {selectedCardDetail?.icon_url && (  
                      <img 
                        src={selectedCardDetail.icon_url} 
                        alt={selectedCard.name}
                        className="w-32 h-32 object-contain"
                      />
                    )}
                    <div className="flex-1">
                      <p className="text-xs text-slate-400 uppercase tracking-wider font-bold mb-1">
                        {selectedCard.rarity}
                      </p>
                      <h2 className="text-3xl font-black text-white mb-3">
                        {selectedCard.name}
                      </h2>
                      <div className="flex items-center gap-4">
                        <div>
                          <p className="text-xs text-slate-400">Market Price</p>
                          <p className="text-2xl font-black text-yellow-400">
                            {selectedCard.price}
                          </p>
                        </div>
                        <div>
                          <p className="text-xs text-slate-400">Elixir</p>
                          <p className="text-2xl font-black text-purple-400">
                            {selectedCard.elixir_cost}
                          </p>
                        </div>
                        <div>
                          <p className="text-xs text-slate-400">Max Level</p>
                          <p className="text-2xl font-black text-blue-400">
                            {selectedCard.max_level}
                          </p>
                        </div>
                      </div>
                    </div>
                  </div>
                </div>
              </div>
            )}

            {/* Order Form */}
            <div className="bg-slate-900/80 border-2 border-purple-500/30 rounded-xl p-6 backdrop-blur-sm">
              <h2 className="text-purple-400 font-black uppercase tracking-wider mb-4 text-xl">
                Place Order
              </h2>

              <form onSubmit={handlePlaceOrder} className="space-y-4">
                
                {/* Buy/Sell */}
                <div className="grid grid-cols-2 gap-3">
                  <button
                    type="button"
                    onClick={() => setOrderType('BUY')}
                    className={`py-4 rounded-lg font-black uppercase tracking-wider text-lg border-2 transition-all transform hover:scale-105 ${
                      orderType === 'BUY'
                        ? 'bg-green-500 text-white border-green-400 shadow-lg shadow-green-500/50'
                        : 'bg-slate-800 text-slate-400 border-slate-700 hover:border-green-500/50'
                    }`}
                  >
                    BUY
                  </button>
                  <button
                    type="button"
                    onClick={() => setOrderType('SELL')}
                    className={`py-4 rounded-lg font-black uppercase tracking-wider text-lg border-2 transition-all transform hover:scale-105 ${
                      orderType === 'SELL'
                        ? 'bg-red-500 text-white border-red-400 shadow-lg shadow-red-500/50'
                        : 'bg-slate-800 text-slate-400 border-slate-700 hover:border-red-500/50'
                    }`}
                  >
                    SELL
                  </button>
                </div>

                {/* Market/Limit */}
                <div className="grid grid-cols-2 gap-3">
                  <button
                    type="button"
                    onClick={() => setOrderMode('MARKET')}
                    className={`py-3 rounded-lg font-bold uppercase tracking-wide border-2 transition ${
                      orderMode === 'MARKET'
                        ? 'bg-orange-500 text-white border-orange-400'
                        : 'bg-slate-800 text-slate-400 border-slate-700 hover:border-orange-500/50'
                    }`}
                  >
                    Market
                  </button>
                  <button
                    type="button"
                    onClick={() => setOrderMode('LIMIT')}
                    className={`py-3 rounded-lg font-bold uppercase tracking-wide border-2 transition ${
                      orderMode === 'LIMIT'
                        ? 'bg-orange-500 text-white border-orange-400'
                        : 'bg-slate-800 text-slate-400 border-slate-700 hover:border-orange-500/50'
                    }`}
                  >
                    Limit
                  </button>
                </div>

                {/* Price (Limit only) */}
                {orderMode === 'LIMIT' && (
                  <div>
                    <label className="block text-sm font-bold text-slate-300 mb-2 uppercase tracking-wide">
                      Price per Card
                    </label>
                    <input
                      type="number"
                      value={price}
                      onChange={(e) => setPrice(e.target.value)}
                      required
                      min="0.01"
                      step="0.01"
                      className="w-full px-4 py-3 bg-slate-950/50 border-2 border-slate-700 rounded-lg 
                               text-white text-lg font-bold focus:outline-none focus:border-orange-500"
                      placeholder="0.00"
                    />
                  </div>
                )}

                {/* Quantity */}
                <div>
                  <label className="block text-sm font-bold text-slate-300 mb-2 uppercase tracking-wide">
                    Quantity
                  </label>
                  <input
                    type="number"
                    value={quantity}
                    onChange={(e) => setQuantity(e.target.value)}
                    required
                    min="1"
                    className="w-full px-4 py-3 bg-slate-950/50 border-2 border-slate-700 rounded-lg 
                             text-white text-lg font-bold focus:outline-none focus:border-orange-500"
                    placeholder="1"
                  />
                </div>

                {/* Messages */}
                {error && (
                  <div className="p-4 bg-red-500/20 border-2 border-red-500/50 rounded-lg animate-pulse">
                    <p className="text-red-300 font-bold text-center">{error}</p>
                  </div>
                )}
                {success && (
                  <div className="p-4 bg-green-500/20 border-2 border-green-500/50 rounded-lg animate-pulse">
                    <p className="text-green-300 font-bold text-center">{success}</p>
                  </div>
                )}

                {/* Submit */}
                <button
                  type="submit"
                  disabled={loading || !selectedCard}
                  className="w-full py-4 bg-gradient-to-r from-orange-500 to-orange-600 
                           text-white font-black text-xl uppercase tracking-wider rounded-lg 
                           border-2 border-orange-400 shadow-lg shadow-orange-500/50
                           hover:from-orange-600 hover:to-orange-700 transform hover:scale-105
                           disabled:opacity-50 disabled:cursor-not-allowed disabled:transform-none transition-all"
                >
                  {loading ? 'Processing...' : `${orderType} ${quantity} ${selectedCard?.name || 'Card'}(s)`}
                </button>
              </form>
            </div>
          </div>

          {/* RIGHT: Order Book */}
          <div className="lg:col-span-3">
            <div className="bg-slate-900/80 border-2 border-blue-500/30 rounded-xl p-6 backdrop-blur-sm">
              <h2 className="text-blue-400 font-black uppercase tracking-wider mb-4 text-lg">
                Order Book
              </h2>

              {/* Market Stats */}
              {orderBook && (
                <div className="grid grid-cols-3 gap-2 mb-6">
                  <div className="bg-slate-950/50 p-3 rounded-lg border border-slate-700">
                    <p className="text-xs text-slate-400 uppercase font-bold">Best Bid</p>
                    <p className="text-green-400 font-black text-lg">
                      {getBestBid() || '--'}
                    </p>
                  </div>
                  <div className="bg-slate-950/50 p-3 rounded-lg border border-slate-700">
                    <p className="text-xs text-slate-400 uppercase font-bold">Best Ask</p>
                    <p className="text-red-400 font-black text-lg">
                      {getBestAsk() || '--'}
                    </p>
                  </div>
                  <div className="bg-slate-950/50 p-3 rounded-lg border border-slate-700">
                    <p className="text-xs text-slate-400 uppercase font-bold">Spread</p>
                    <p className="text-yellow-400 font-black text-lg">
                      {getSpread().toFixed(2)}
                    </p>
                  </div>
                </div>
              )}

              {orderBook ? (
                <div className="space-y-4">
                  
                  {/* Asks */}
                  <div>
                    <h3 className="text-red-400 font-bold uppercase text-xs mb-2 tracking-wide">
                      Sell Orders
                    </h3>
                    <div className="space-y-1 max-h-48 overflow-y-auto">
                      {orderBook.asks.length > 0 ? (
                        orderBook.asks.slice(0, 10).map(([price, qty], idx) => (
                          <div 
                            key={idx}
                            className="flex justify-between p-2 bg-red-500/10 border border-red-500/30 rounded"
                          >
                            <span className="text-red-400 font-mono font-bold">{price.toFixed(2)}</span>
                            <span className="text-slate-300 font-bold text-sm">{qty}</span>
                          </div>
                        ))
                      ) : (
                        <p className="text-slate-500 text-center py-4 text-sm">No sellers</p>
                      )}
                    </div>
                  </div>

                  {/* Bids */}
                  <div>
                    <h3 className="text-green-400 font-bold uppercase text-xs mb-2 tracking-wide">
                      Buy Orders
                    </h3>
                    <div className="space-y-1 max-h-48 overflow-y-auto">
                      {orderBook.bids.length > 0 ? (
                        orderBook.bids.slice(0, 10).map(([price, qty], idx) => (
                          <div 
                            key={idx}
                            className="flex justify-between p-2 bg-green-500/10 border border-green-500/30 rounded"
                          >
                            <span className="text-green-400 font-mono font-bold">{price.toFixed(2)}</span>
                            <span className="text-slate-300 font-bold text-sm">{qty}</span>
                          </div>
                        ))
                      ) : (
                        <p className="text-slate-500 text-center py-4 text-sm">No buyers</p>
                      )}
                    </div>
                  </div>
                </div>
              ) : (
                <p className="text-slate-400 text-center py-8">Loading...</p>
              )}
            </div>
          </div>
        </div>
      </div>
    </div>
  );
}