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
            <div className="flex items-center gap-6">
              <h1 className="text-2xl font-bold text-white">
                Clash Trading
              </h1>
              
              {/* Navigation Links */}
              <div className="hidden md:flex gap-4">
                <button
                  onClick={() => navigate('/dashboard')}
                  className="px-4 py-2 text-white font-semibold border-b-2 border-purple-500"
                >
                  Dashboard
                </button>
                <button
                  onClick={() => navigate('/trading')}
                  className="px-4 py-2 text-slate-300 hover:text-white font-semibold 
                           hover:border-b-2 hover:border-orange-500 transition"
                >
                  Trading
                </button>
                <button
                  onClick={() => navigate('/portfolio')}
                  className="px-4 py-2 text-slate-300 hover:text-white font-semibold 
                           hover:border-b-2 hover:border-blue-500 transition"
                >
                  Portfolio
                </button>
              </div>
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
            Welcome to Clash Royale Trading!
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

        {/* Quick Actions Grid */}
        <div className="grid md:grid-cols-2 gap-6">
          {/* Trading Card */}
          <div className="bg-gradient-to-r from-orange-600/20 to-orange-500/20 border border-orange-500/30 rounded-xl p-8">
            <h3 className="text-2xl font-bold text-white mb-4">Start Trading</h3>
            <p className="text-slate-300 mb-6">
              Trade all 121 Clash Royale cards on the live marketplace
            </p>
            <button
              onClick={() => navigate('/trading')}
              className="px-8 py-4 bg-gradient-to-r from-orange-500 to-orange-600 
                       text-white font-bold rounded-lg shadow-lg hover:from-orange-600 
                       hover:to-orange-700 transform hover:scale-105 transition-all"
            >
              Open Trading Interface
            </button>
          </div>

          {/* Portfolio Card */}
          <div className="bg-gradient-to-r from-blue-600/20 to-purple-600/20 border border-blue-500/30 rounded-xl p-8">
            <h3 className="text-2xl font-bold text-white mb-4">View Portfolio</h3>
            <p className="text-slate-300 mb-6">
              Track your holdings, P&L, and active orders
            </p>
            <button
              onClick={() => navigate('/portfolio')}
              className="px-8 py-4 bg-gradient-to-r from-blue-500 to-purple-600 
                       text-white font-bold rounded-lg shadow-lg hover:from-blue-600 
                       hover:to-purple-700 transform hover:scale-105 transition-all"
            >
              View My Portfolio
            </button>
          </div>
        </div>
      </div>
    </div>
  );
}