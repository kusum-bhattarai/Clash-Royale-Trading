import { useState, useEffect } from 'react';
import { useAuth } from '../contexts/AuthContext';
import { useNavigate } from 'react-router-dom';
import { usersAPI, ordersAPI } from '../services/api';
import type { Portfolio, Order } from '../types/api';

export default function PortfolioPage() {
  const { user, logout } = useAuth();
  const navigate = useNavigate();

  const [portfolio, setPortfolio] = useState<Portfolio | null>(null);
  const [orders, setOrders] = useState<Order[]>([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string>('');
  const [cancellingOrderId, setCancellingOrderId] = useState<string | null>(null);

  useEffect(() => {
    if (user) {
      loadPortfolioData();
    }
  }, [user]);

  const loadPortfolioData = async () => {
    if (!user) return;

    setLoading(true);
    setError('');

    try {
      const [portfolioData, ordersData] = await Promise.all([
        usersAPI.getPortfolio(user.user_id),
        usersAPI.getOrders(user.user_id),
      ]);

      setPortfolio(portfolioData);
      setOrders(ordersData);
    } catch (err: any) {
      console.error('Failed to load portfolio:', err);
      setError('Failed to load portfolio data');
    } finally {
      setLoading(false);
    }
  };

  const handleCancelOrder = async (orderId: string) => {
    setCancellingOrderId(orderId);
    try {
      await ordersAPI.cancel(orderId);
      await loadPortfolioData();
    } catch (err: any) {
      console.error('Failed to cancel order:', err);
      setError(err.response?.data?.error || 'Failed to cancel order');
      setTimeout(() => setError(''), 3000);
    } finally {
      setCancellingOrderId(null);
    }
  };

  const handleLogout = () => {
    logout();
    navigate('/login');
  };

  const getStatusColor = (status: string) => {
    switch (status.toUpperCase()) {
      case 'PENDING': return 'text-yellow-400';
      case 'PARTIAL': return 'text-blue-400';
      case 'FILLED': return 'text-green-400';
      case 'CANCELLED': return 'text-red-400';
      default: return 'text-slate-400';
    }
  };

  const activeOrders = orders.filter(
    o => o.status === 'PENDING' || o.status === 'PARTIAL'
  );

  if (loading) {
    return (
      <div className="min-h-screen bg-gradient-to-br from-slate-950 via-orange-950/20 to-purple-950/20 flex items-center justify-center">
        <div className="text-orange-400 text-xl font-bold">Loading portfolio...</div>
      </div>
    );
  }

  return (
    <div className="min-h-screen bg-gradient-to-br from-slate-950 via-orange-950/20 to-purple-950/20">
      
      {/* Header */}
      <div className="border-b-4 border-orange-500/50 bg-gradient-to-r from-slate-900 via-orange-900/20 to-purple-900/20">
        <div className="max-w-7xl mx-auto px-4 py-4">
          <div className="flex justify-between items-center">
            <div>
              <h1 className="text-3xl font-black text-transparent bg-clip-text bg-gradient-to-r from-orange-400 via-yellow-400 to-orange-500 tracking-wider uppercase">
                Portfolio
              </h1>
              <p className="text-orange-300/60 text-sm font-bold tracking-wide">
                {user?.username}
              </p>
            </div>

            <div className="flex items-center gap-4">
              <button
                onClick={() => navigate('/trading')}
                className="px-4 py-2 bg-orange-600/20 text-orange-300 border-2 border-orange-500/50 
                         rounded-lg hover:bg-orange-600/30 font-bold uppercase text-sm tracking-wide transition"
              >
                Trading
              </button>

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

      {/* Main Content */}
      <div className="max-w-7xl mx-auto px-4 py-6 space-y-6">
        
        {/* Error Message */}
        {error && (
          <div className="p-4 bg-red-500/20 border-2 border-red-500/50 rounded-lg">
            <p className="text-red-300 font-bold text-center">{error}</p>
          </div>
        )}

        {/* Portfolio Summary */}
        {portfolio && (
          <div className="grid md:grid-cols-3 gap-6">
            <div className="bg-slate-900/80 border-2 border-yellow-500/30 rounded-xl p-6 backdrop-blur-sm">
              <p className="text-xs text-yellow-400/70 font-bold uppercase tracking-wide mb-2">
                Gold Balance
              </p>
              <p className="text-3xl font-black text-yellow-400">
                {portfolio.gold_balance.toLocaleString()}
              </p>
            </div>

            <div className="bg-slate-900/80 border-2 border-blue-500/30 rounded-xl p-6 backdrop-blur-sm">
              <p className="text-xs text-blue-400/70 font-bold uppercase tracking-wide mb-2">
                Card Value
              </p>
              <p className="text-3xl font-black text-blue-400">
                {portfolio.total_card_value.toFixed(2)}
              </p>
            </div>

            <div className="bg-slate-900/80 border-2 border-purple-500/30 rounded-xl p-6 backdrop-blur-sm">
              <p className="text-xs text-purple-400/70 font-bold uppercase tracking-wide mb-2">
                Total Value
              </p>
              <p className="text-3xl font-black text-purple-400">
                {portfolio.total_portfolio_value.toFixed(2)}
              </p>
            </div>
          </div>
        )}

        {/* Card Holdings */}
        {portfolio && portfolio.holdings.length > 0 && (
          <div className="bg-slate-900/80 border-2 border-orange-500/30 rounded-xl p-6 backdrop-blur-sm">
            <h2 className="text-orange-400 font-black uppercase tracking-wider mb-4 text-xl">
              Card Holdings
            </h2>
            
            <div className="overflow-x-auto">
              <table className="w-full">
                <thead>
                  <tr className="border-b-2 border-slate-700">
                    <th className="text-left py-3 px-4 text-slate-400 font-bold uppercase text-sm">
                      Card
                    </th>
                    <th className="text-right py-3 px-4 text-slate-400 font-bold uppercase text-sm">
                      Quantity
                    </th>
                    <th className="text-right py-3 px-4 text-slate-400 font-bold uppercase text-sm">
                      Avg Cost
                    </th>
                    <th className="text-right py-3 px-4 text-slate-400 font-bold uppercase text-sm">
                      Current Price
                    </th>
                    <th className="text-right py-3 px-4 text-slate-400 font-bold uppercase text-sm">
                      Total Value
                    </th>
                    <th className="text-right py-3 px-4 text-slate-400 font-bold uppercase text-sm">
                      P&L
                    </th>
                    <th className="text-right py-3 px-4 text-slate-400 font-bold uppercase text-sm">
                      P&L %
                    </th>
                  </tr>
                </thead>
                <tbody>
                  {portfolio.holdings.map((holding) => (
                    <tr
                      key={holding.card_id}
                      className="border-b border-slate-800 hover:bg-slate-800/50 transition"
                    >
                      <td className="py-3 px-4">
                        <span className="text-white font-bold">{holding.card_name}</span>
                      </td>
                      <td className="text-right py-3 px-4 text-slate-300 font-bold">
                        {holding.quantity}
                      </td>
                      <td className="text-right py-3 px-4 text-slate-300 font-mono">
                        {holding.avg_purchase_price.toFixed(2)}
                      </td>
                      <td className="text-right py-3 px-4 text-slate-300 font-mono">
                        {holding.current_market_price.toFixed(2)}
                      </td>
                      <td className="text-right py-3 px-4 text-blue-400 font-bold">
                        {holding.total_value.toFixed(2)}
                      </td>
                      <td
                        className={`text-right py-3 px-4 font-bold ${
                          holding.unrealized_pnl >= 0 ? 'text-green-400' : 'text-red-400'
                        }`}
                      >
                        {holding.unrealized_pnl >= 0 ? '+' : ''}
                        {holding.unrealized_pnl.toFixed(2)}
                      </td>
                      <td
                        className={`text-right py-3 px-4 font-bold ${
                          holding.unrealized_pnl_percent >= 0 ? 'text-green-400' : 'text-red-400'
                        }`}
                      >
                        {holding.unrealized_pnl_percent >= 0 ? '+' : ''}
                        {holding.unrealized_pnl_percent.toFixed(2)}%
                      </td>
                    </tr>
                  ))}
                </tbody>
              </table>
            </div>
          </div>
        )}

        {/* No Holdings Message */}
        {portfolio && portfolio.holdings.length === 0 && (
          <div className="bg-slate-900/80 border-2 border-slate-700/30 rounded-xl p-12 backdrop-blur-sm text-center">
            <p className="text-slate-400 text-lg font-bold mb-4">No card holdings yet</p>
            <button
              onClick={() => navigate('/trading')}
              className="px-6 py-3 bg-orange-600/20 text-orange-300 border-2 border-orange-500/50 
                       rounded-lg hover:bg-orange-600/30 font-bold uppercase text-sm tracking-wide transition"
            >
              Start Trading
            </button>
          </div>
        )}

        {/* Active Orders */}
        {activeOrders.length > 0 && (
          <div className="bg-slate-900/80 border-2 border-purple-500/30 rounded-xl p-6 backdrop-blur-sm">
            <h2 className="text-purple-400 font-black uppercase tracking-wider mb-4 text-xl">
              Active Orders ({activeOrders.length})
            </h2>
            
            <div className="overflow-x-auto">
              <table className="w-full">
                <thead>
                  <tr className="border-b-2 border-slate-700">
                    <th className="text-left py-3 px-4 text-slate-400 font-bold uppercase text-sm">
                      Card
                    </th>
                    <th className="text-left py-3 px-4 text-slate-400 font-bold uppercase text-sm">
                      Type
                    </th>
                    <th className="text-left py-3 px-4 text-slate-400 font-bold uppercase text-sm">
                      Mode
                    </th>
                    <th className="text-right py-3 px-4 text-slate-400 font-bold uppercase text-sm">
                      Price
                    </th>
                    <th className="text-right py-3 px-4 text-slate-400 font-bold uppercase text-sm">
                      Quantity
                    </th>
                    <th className="text-right py-3 px-4 text-slate-400 font-bold uppercase text-sm">
                      Filled
                    </th>
                    <th className="text-left py-3 px-4 text-slate-400 font-bold uppercase text-sm">
                      Status
                    </th>
                    <th className="text-center py-3 px-4 text-slate-400 font-bold uppercase text-sm">
                      Action
                    </th>
                  </tr>
                </thead>
                <tbody>
                  {activeOrders.map((order) => (
                    <tr
                      key={order.order_id}
                      className="border-b border-slate-800 hover:bg-slate-800/50 transition"
                    >
                      <td className="py-3 px-4 text-white font-bold">
                        {order.card_id}
                      </td>
                      <td className="py-3 px-4">
                        <span
                          className={`font-bold ${
                            order.order_type === 'BUY' ? 'text-green-400' : 'text-red-400'
                          }`}
                        >
                          {order.order_type}
                        </span>
                      </td>
                      <td className="py-3 px-4 text-slate-300 font-bold">
                        {order.order_mode}
                      </td>
                      <td className="text-right py-3 px-4 text-slate-300 font-mono">
                        {order.price ? order.price.toFixed(2) : 'MARKET'}
                      </td>
                      <td className="text-right py-3 px-4 text-slate-300 font-bold">
                        {order.quantity}
                      </td>
                      <td className="text-right py-3 px-4 text-blue-400 font-bold">
                        {order.filled_quantity}
                      </td>
                      <td className="py-3 px-4">
                        <span className={`font-bold ${getStatusColor(order.status)}`}>
                          {order.status}
                        </span>
                      </td>
                      <td className="text-center py-3 px-4">
                        <button
                          onClick={() => handleCancelOrder(order.order_id)}
                          disabled={cancellingOrderId === order.order_id}
                          className="px-3 py-1 bg-red-600/20 text-red-300 border border-red-500/50 
                                   rounded hover:bg-red-600/30 font-bold text-xs uppercase 
                                   disabled:opacity-50 disabled:cursor-not-allowed transition"
                        >
                          {cancellingOrderId === order.order_id ? 'Cancelling...' : 'Cancel'}
                        </button>
                      </td>
                    </tr>
                  ))}
                </tbody>
              </table>
            </div>
          </div>
        )}
      </div>
    </div>
  );
}