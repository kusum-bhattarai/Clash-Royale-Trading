import { useAuth } from '../contexts/AuthContext';
import { useNavigate } from 'react-router-dom';

export default function Dashboard() {
  const { user, logout } = useAuth();
  const navigate = useNavigate();

  const handleLogout = () => {
    logout();
    navigate('/login');
  };

  return (
    <div className="min-h-screen bg-gradient-to-br from-clash-dark via-purple-900 to-clash-dark">
      {/* Navigation Bar */}
      <nav className="bg-slate-800/50 backdrop-blur-lg border-b border-purple-500/20">
        <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8">
          <div className="flex justify-between items-center h-16">
            {/* Logo */}
            <div className="flex items-center">
              <h1 className="text-2xl font-bold text-white">
                Clash Trading
              </h1>
            </div>

            {/* User Info & Logout */}
            <div className="flex items-center gap-6">
              <div className="text-right">
                <p className="text-sm text-slate-400">Welcome back,</p>
                <p className="text-white font-semibold">{user?.username}</p>
              </div>
              
              <div className="text-right">
                <p className="text-sm text-slate-400">Gold Balance</p>
                <p className="text-clash-gold font-bold text-lg">
                  {user?.gold_balance.toLocaleString()}
                </p>
              </div>

              <button
                onClick={handleLogout}
                className="px-4 py-2 bg-red-500/20 text-red-400 border border-red-500/50 
                         rounded-lg hover:bg-red-500/30 transition"
              >
                Logout
              </button>
            </div>
          </div>
        </div>
      </nav>

      {/* Main Content */}
      <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8 py-12">
        {/* Welcome Card */}
        <div className="bg-slate-800/50 backdrop-blur-lg border border-purple-500/20 rounded-2xl p-8 mb-8">
          <h2 className="text-3xl font-bold text-white mb-4">
            Welcome to Clash Royale Trading! 🎮
          </h2>
          <p className="text-slate-300 text-lg">
            Your account is active. You have{' '}
            <span className="text-clash-gold font-bold">
              {user?.gold_balance.toLocaleString()} gold
            </span>{' '}
            to start trading.
          </p>
        </div>

        {/* Stats Grid */}
        <div className="grid md:grid-cols-3 gap-6 mb-8">
          {/* Portfolio Value */}
          <div className="bg-slate-800/50 backdrop-blur-lg border border-purple-500/20 rounded-xl p-6">
            <p className="text-slate-400 text-sm mb-2">Portfolio Value</p>
            <p className="text-3xl font-bold text-white">
              {user?.gold_balance.toLocaleString()}
            </p>
            <p className="text-green-400 text-sm mt-2">+0.00%</p>
          </div>

          {/* Total Trades */}
          <div className="bg-slate-800/50 backdrop-blur-lg border border-purple-500/20 rounded-xl p-6">
            <p className="text-slate-400 text-sm mb-2">Total Trades</p>
            <p className="text-3xl font-bold text-white">
              {user?.total_trades || 0}
            </p>
            <p className="text-slate-400 text-sm mt-2">All time</p>
          </div>

          {/* Trader Level */}
          <div className="bg-slate-800/50 backdrop-blur-lg border border-purple-500/20 rounded-xl p-6">
            <p className="text-slate-400 text-sm mb-2">Trader Level</p>
            <p className="text-3xl font-bold text-purple-400">
              {user?.trader_level || 'CHALLENGER'}
            </p>
            <p className="text-slate-400 text-sm mt-2">Current rank</p>
          </div>
        </div>

        {/* Coming Soon Section */}
        <div className="bg-gradient-to-r from-purple-600/20 to-blue-600/20 border border-purple-500/30 rounded-xl p-8">
          <h3 className="text-2xl font-bold text-white mb-4">Coming Soon 🚀</h3>
          <div className="grid md:grid-cols-2 gap-4 text-slate-300">
            <div>
              <p className="font-semibold text-white mb-2">📊 Trading Interface</p>
              <p className="text-sm">View order books and place trades</p>
            </div>
            <div>
              <p className="font-semibold text-white mb-2">💼 Portfolio Manager</p>
              <p className="text-sm">Track your card holdings and P&L</p>
            </div>
            <div>
              <p className="font-semibold text-white mb-2">🏆 Leaderboards</p>
              <p className="text-sm">Compete with other traders</p>
            </div>
            <div>
              <p className="font-semibold text-white mb-2">📈 Real-Time Charts</p>
              <p className="text-sm">Live price updates via WebSocket</p>
            </div>
          </div>
        </div>
      </div>
    </div>
  );
}