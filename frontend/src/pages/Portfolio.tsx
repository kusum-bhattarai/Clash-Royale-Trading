import { useState, useEffect } from 'react';
import { motion } from 'framer-motion';
import { useAuth } from '../contexts/AuthContext';
import { useNavigate } from 'react-router-dom';
import { usersAPI, ordersAPI, cardsAPI } from '../services/api';
import type { Portfolio, Order, Card } from '../types/api';
import NavBar from '../components/NavBar';

export default function PortfolioPage() {
  const { user } = useAuth();
  const navigate = useNavigate();

  const [portfolio, setPortfolio] = useState<Portfolio | null>(null);
  const [orders, setOrders] = useState<Order[]>([]);
  const [cardMap, setCardMap] = useState<Record<string, string>>({});
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string>('');
  const [cancellingOrderId, setCancellingOrderId] = useState<string | null>(null);

  useEffect(() => {
    if (user) loadPortfolioData();
  }, [user]);

  const loadPortfolioData = async () => {
    if (!user) return;
    setLoading(true);
    setError('');
    try {
      const [portfolioData, ordersResponse, cards] = await Promise.all([
        usersAPI.getPortfolio(user.user_id),
        usersAPI.getOrders(user.user_id),
        cardsAPI.getAll(),
      ]);
      setCardMap(Object.fromEntries(cards.map((c: Card) => [c.card_id, c.name])));
      setPortfolio(portfolioData);
      const ordersArray = Array.isArray(ordersResponse)
        ? ordersResponse
        : (ordersResponse as any).orders || [];
      setOrders(ordersArray);
    } catch {
      setError('Failed to load portfolio data');
    } finally {
      setLoading(false);
    }
  };

  const handleCancelOrder = async (orderId: string) => {
    setCancellingOrderId(orderId);
    try {
      await ordersAPI.cancel(orderId);
    } catch (err: any) {
      setError(err.response?.data?.error || 'Failed to cancel order');
      setTimeout(() => setError(''), 3000);
    } finally {
      await loadPortfolioData();
      setCancellingOrderId(null);
    }
  };

  const getStatusColor = (status: string) => {
    switch (status.toUpperCase()) {
      case 'PENDING':   return 'text-neon-gold';
      case 'PARTIAL':   return 'text-neon-cyan';
      case 'FILLED':    return 'text-bull-green';
      case 'CANCELLED': return 'text-bear-red';
      default:          return 'text-slate-400';
    }
  };

  const activeOrders = orders.filter(o => o.status === 'PENDING' || o.status === 'PARTIAL');

  if (loading) {
    return (
      <div className="min-h-screen bg-arena-bg hex-grid-bg flex items-center justify-center">
        <p className="text-neon-gold font-display uppercase tracking-widest text-lg animate-pulse">
          Loading…
        </p>
      </div>
    );
  }

  const summaryStats = portfolio
    ? [
        { label: 'Gold Balance',   value: portfolio.gold_balance.toLocaleString() + 'g', color: 'text-neon-gold',   border: 'border-neon-gold/20' },
        { label: 'Card Value',     value: portfolio.total_card_value.toFixed(0) + 'g',   color: 'text-neon-cyan',   border: 'border-neon-cyan/20' },
        { label: 'Total Value',    value: portfolio.total_portfolio_value.toFixed(0) + 'g', color: 'text-neon-purple', border: 'border-neon-purple/20' },
      ]
    : [];

  return (
    <div className="min-h-screen bg-arena-bg hex-grid-bg text-white">

      <NavBar activePage="portfolio" />

      <div className="max-w-6xl mx-auto px-6 py-8 space-y-8">

        {/* Error */}
        {error && (
          <div className="p-3 bg-bear-red/10 border border-bear-red/30 rounded-lg">
            <p className="text-bear-red text-sm font-bold">{error}</p>
          </div>
        )}

        {/* Summary stats */}
        {portfolio && (
          <div className="grid grid-cols-3 gap-4">
            {summaryStats.map(({ label, value, color, border }, i) => (
              <motion.div
                key={label}
                initial={{ opacity: 0, y: 16 }}
                animate={{ opacity: 1, y: 0 }}
                transition={{ delay: i * 0.08 }}
                className={`bg-arena-surface/50 border ${border} rounded-xl p-5`}
              >
                <p className="text-xs text-slate-500 uppercase tracking-widest mb-2">{label}</p>
                <p className={`text-2xl font-black font-mono ${color}`}>{value}</p>
              </motion.div>
            ))}
          </div>
        )}

        {/* Card Holdings */}
        {portfolio && portfolio.holdings.length > 0 && (
          <motion.div
            initial={{ opacity: 0, y: 16 }}
            animate={{ opacity: 1, y: 0 }}
            transition={{ delay: 0.24 }}
            className="bg-arena-surface/50 border border-arena-border rounded-xl overflow-hidden"
          >
            <div className="px-5 py-3 border-b border-arena-border">
              <p className="text-xs font-bold uppercase tracking-widest text-slate-400">Card Holdings</p>
            </div>
            <div className="overflow-x-auto">
              <table className="w-full">
                <thead>
                  <tr className="border-b border-slate-800">
                    {['Card', 'Qty', 'Avg Cost', 'Market Price', 'Value', 'P&L', 'P&L %'].map(h => (
                      <th
                        key={h}
                        className={`py-2.5 px-4 text-[10px] font-bold uppercase tracking-widest text-slate-500 ${h === 'Card' ? 'text-left' : 'text-right'}`}
                      >
                        {h}
                      </th>
                    ))}
                  </tr>
                </thead>
                <tbody>
                  {portfolio.holdings.map((holding) => (
                    <tr key={holding.card_id} className="border-b border-slate-800/50 hover:bg-white/2 transition">
                      <td className="py-3 px-4 text-sm font-bold text-white">{holding.card_name}</td>
                      <td className="py-3 px-4 text-right text-sm font-mono text-slate-300">{holding.quantity}</td>
                      <td className="py-3 px-4 text-right text-sm font-mono text-slate-400">{holding.avg_purchase_price.toFixed(1)}g</td>
                      <td className="py-3 px-4 text-right text-sm font-mono text-slate-300">{holding.current_market_price.toFixed(1)}g</td>
                      <td className="py-3 px-4 text-right text-sm font-mono font-bold text-neon-cyan">{holding.total_value.toFixed(0)}g</td>
                      <td className={`py-3 px-4 text-right text-sm font-mono font-bold ${holding.unrealized_pnl >= 0 ? 'text-bull-green' : 'text-bear-red'}`}>
                        {holding.unrealized_pnl >= 0 ? '+' : ''}{holding.unrealized_pnl.toFixed(1)}g
                      </td>
                      <td className={`py-3 px-4 text-right text-sm font-mono font-bold ${holding.unrealized_pnl_percent >= 0 ? 'text-bull-green' : 'text-bear-red'}`}>
                        {holding.unrealized_pnl_percent >= 0 ? '+' : ''}{holding.unrealized_pnl_percent.toFixed(2)}%
                      </td>
                    </tr>
                  ))}
                </tbody>
              </table>
            </div>
          </motion.div>
        )}

        {/* No Holdings */}
        {portfolio && portfolio.holdings.length === 0 && (
          <div className="bg-arena-surface/50 border border-arena-border rounded-xl p-12 text-center">
            <p className="text-slate-500 text-sm mb-4">No card holdings yet</p>
            <button
              onClick={() => navigate('/trading')}
              className="px-6 py-2.5 bg-gradient-to-r from-neon-gold/20 to-orange-500/20 text-neon-gold border border-neon-gold/30 rounded-lg font-bold uppercase tracking-wide text-sm hover:from-neon-gold/30 transition"
            >
              Start Trading
            </button>
          </div>
        )}

        {/* Active Orders */}
        {activeOrders.length > 0 && (
          <motion.div
            initial={{ opacity: 0, y: 16 }}
            animate={{ opacity: 1, y: 0 }}
            transition={{ delay: 0.32 }}
            className="bg-arena-surface/50 border border-arena-border rounded-xl overflow-hidden"
          >
            <div className="px-5 py-3 border-b border-arena-border flex items-center gap-2">
              <p className="text-xs font-bold uppercase tracking-widest text-slate-400">Active Orders</p>
              <span className="text-[10px] font-mono bg-neon-purple/20 text-neon-purple px-1.5 py-0.5 rounded">
                {activeOrders.length}
              </span>
            </div>
            <div className="overflow-x-auto">
              <table className="w-full">
                <thead>
                  <tr className="border-b border-slate-800">
                    {['Card', 'Type', 'Mode', 'Price', 'Qty', 'Filled', 'Status', ''].map((h, i) => (
                      <th
                        key={i}
                        className={`py-2.5 px-4 text-[10px] font-bold uppercase tracking-widest text-slate-500 ${i <= 2 ? 'text-left' : i === 7 ? 'text-center' : 'text-right'}`}
                      >
                        {h}
                      </th>
                    ))}
                  </tr>
                </thead>
                <tbody>
                  {activeOrders.map((order) => (
                    <tr key={order.order_id} className="border-b border-slate-800/50 hover:bg-white/2 transition">
                      <td className="py-3 px-4 text-sm font-bold text-white">
                        {cardMap[order.card_id] ?? order.card_id}
                      </td>
                      <td className="py-3 px-4">
                        <span className={`text-xs font-bold uppercase px-2 py-0.5 rounded ${order.order_type === 'BUY' ? 'bg-bull-green/10 text-bull-green' : 'bg-bear-red/10 text-bear-red'}`}>
                          {order.order_type}
                        </span>
                      </td>
                      <td className="py-3 px-4 text-sm text-slate-400 uppercase">{order.order_mode}</td>
                      <td className="py-3 px-4 text-right text-sm font-mono text-slate-300">
                        {order.price ? order.price.toFixed(1) + 'g' : '—'}
                      </td>
                      <td className="py-3 px-4 text-right text-sm font-mono text-slate-300">{order.quantity}</td>
                      <td className="py-3 px-4 text-right text-sm font-mono text-neon-cyan">{order.filled_quantity}</td>
                      <td className={`py-3 px-4 text-sm font-bold ${getStatusColor(order.status)}`}>
                        {order.status}
                      </td>
                      <td className="py-3 px-4 text-center">
                        <button
                          onClick={() => handleCancelOrder(order.order_id)}
                          disabled={cancellingOrderId === order.order_id}
                          className="px-3 py-1 text-xs font-bold uppercase tracking-wide text-bear-red/70 border border-bear-red/30 rounded hover:border-bear-red hover:text-bear-red disabled:opacity-40 disabled:cursor-not-allowed transition"
                        >
                          {cancellingOrderId === order.order_id ? '…' : 'Cancel'}
                        </button>
                      </td>
                    </tr>
                  ))}
                </tbody>
              </table>
            </div>
          </motion.div>
        )}
      </div>
    </div>
  );
}
