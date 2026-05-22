import { useState, useEffect } from 'react';
import { motion } from 'framer-motion';
import { useAuth } from '../contexts/AuthContext';
import { useNavigate } from 'react-router-dom';
import { usersAPI, cardsAPI } from '../services/api';
import type { Trade, Card } from '../types/api';
import RankBadge from '../components/RankBadge';
import NavBar from '../components/NavBar';

export default function Dashboard() {
  const { user } = useAuth();
  const navigate = useNavigate();

  const [recentTrades, setRecentTrades] = useState<Trade[]>([]);
  const [topCards, setTopCards] = useState<Card[]>([]);

  useEffect(() => {
    if (!user) return;
    usersAPI.getTrades(user.user_id, 5).then(setRecentTrades).catch(() => {});
    cardsAPI.getAll().then(cards => {
      // Sort by usage_rate desc as a proxy for "market movers"
      const sorted = [...cards].sort((a, b) => (b.usage_rate ?? 0) - (a.usage_rate ?? 0));
      setTopCards(sorted.slice(0, 3));
    }).catch(() => {});
  }, [user]);

  const stats = [
    { label: 'Gold Balance', value: user?.gold_balance.toLocaleString() ?? '—', color: 'text-neon-gold', border: 'border-neon-gold/20' },
    { label: 'Total Trades', value: user?.total_trades ?? 0, color: 'text-neon-cyan', border: 'border-neon-cyan/20' },
    { label: 'XP Earned', value: user?.xp ?? 0, color: 'text-neon-purple', border: 'border-neon-purple/20' },
    { label: 'Rank', value: user?.trader_level ?? 'Goblin Stadium', color: 'text-bull-green', border: 'border-bull-green/20' },
  ];

  return (
    <div className="min-h-screen bg-arena-bg hex-grid-bg text-white">

      <NavBar activePage="dashboard" />

      <div className="max-w-6xl mx-auto px-6 py-8 space-y-8">

        {/* Hero banner */}
        <div className="relative overflow-hidden rounded-2xl border border-arena-border bg-arena-surface/50 p-8">
          <div className="absolute inset-0 bg-gradient-to-r from-neon-purple/5 via-transparent to-neon-cyan/5" />
          <div className="relative flex items-center gap-6">
            <div>
              <p className="text-slate-500 text-sm uppercase tracking-widest">Welcome back,</p>
              <h2 className="text-3xl font-black text-white">{user?.username}</h2>
            </div>
            {user && (
              <div className="ml-4">
                <RankBadge rank={user.trader_level || 'Goblin Stadium'} xp={user.xp ?? 0} />
              </div>
            )}
            <div className="ml-auto text-right">
              <p className="text-slate-500 text-xs uppercase tracking-widest">Gold Balance</p>
              <p className="text-4xl font-black text-neon-gold font-mono">{user?.gold_balance.toLocaleString()}</p>
            </div>
          </div>
        </div>

        {/* Stats row */}
        <div className="grid grid-cols-4 gap-4">
          {stats.map(({ label, value, color, border }, i) => (
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

        {/* Two-column: Recent Trades + Market Movers */}
        <div className="grid md:grid-cols-2 gap-6">

          {/* Recent Trade History */}
          <div className="bg-arena-surface/50 border border-arena-border rounded-xl overflow-hidden">
            <div className="px-5 py-3 border-b border-arena-border flex items-center justify-between">
              <p className="text-xs font-bold uppercase tracking-widest text-slate-400">Recent Trades</p>
            </div>
            <div className="divide-y divide-slate-800/50">
              {recentTrades.length === 0 ? (
                <p className="text-slate-600 text-sm text-center py-8">No trades yet — start trading!</p>
              ) : (
                recentTrades.map(trade => {
                  const isBuy = trade.buyer_id === user?.user_id;
                  return (
                    <div key={trade.trade_id} className="flex items-center justify-between px-5 py-3">
                      <div className="flex items-center gap-3">
                        <span className={`text-xs font-bold uppercase px-2 py-0.5 rounded ${isBuy ? 'bg-bull-green/10 text-bull-green' : 'bg-bear-red/10 text-bear-red'}`}>
                          {isBuy ? 'BUY' : 'SELL'}
                        </span>
                        <span className="text-sm text-slate-300 font-semibold">{trade.card_id}</span>
                      </div>
                      <div className="text-right">
                        <p className="text-sm font-mono font-bold text-neon-gold">{trade.total_value.toLocaleString()}g</p>
                        <p className="text-xs text-slate-600">×{trade.quantity} @ {trade.price.toFixed(0)}g</p>
                      </div>
                    </div>
                  );
                })
              )}
            </div>
          </div>

          {/* Market Movers */}
          <div className="bg-arena-surface/50 border border-arena-border rounded-xl overflow-hidden">
            <div className="px-5 py-3 border-b border-arena-border">
              <p className="text-xs font-bold uppercase tracking-widest text-slate-400">Top Cards by Usage</p>
            </div>
            <div className="divide-y divide-slate-800/50">
              {topCards.length === 0 ? (
                <p className="text-slate-600 text-sm text-center py-8">Loading…</p>
              ) : (
                topCards.map((card, idx) => (
                  <div key={card.card_id} className="flex items-center gap-4 px-5 py-3">
                    <span className="text-xl font-black text-slate-600 w-6">#{idx + 1}</span>
                    {card.icon_url && <img src={card.icon_url} alt={card.name} className="w-10 h-10 object-contain" />}
                    <div className="flex-1">
                      <p className="text-sm font-bold text-white">{card.name}</p>
                      <p className="text-xs text-slate-500 capitalize">{card.rarity}</p>
                    </div>
                    <div className="text-right">
                      <p className="text-sm font-mono font-bold text-neon-gold">{card.price}g</p>
                      <p className="text-xs text-slate-500">{((card.usage_rate ?? 0) * 100).toFixed(1)}% usage</p>
                    </div>
                  </div>
                ))
              )}
            </div>
            <div className="px-5 py-4 border-t border-arena-border">
              <button
                onClick={() => navigate('/trading')}
                className="w-full py-2.5 bg-gradient-to-r from-neon-gold/20 to-orange-500/20 text-neon-gold border border-neon-gold/30 rounded-lg font-bold uppercase tracking-wide text-sm hover:from-neon-gold/30 transition"
              >
                Open Trading Interface
              </button>
            </div>
          </div>
        </div>
      </div>
    </div>
  );
}
