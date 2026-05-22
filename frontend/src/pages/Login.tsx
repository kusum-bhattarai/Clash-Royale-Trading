import { useState } from 'react';
import type { FormEvent } from 'react';
import { useNavigate, Link } from 'react-router-dom';
import { useAuth } from '../contexts/AuthContext';

export default function Login() {
  const navigate = useNavigate();
  const { login } = useAuth();
  const [username, setUsername] = useState('');
  const [password, setPassword] = useState('');
  const [error, setError] = useState('');
  const [isLoading, setIsLoading] = useState(false);

  const handleSubmit = async (e: FormEvent) => {
    e.preventDefault();
    setError('');
    setIsLoading(true);
    try {
      await login({ username, password });
      navigate('/dashboard');
    } catch (err: any) {
      setError(err.response?.data?.error || 'Login failed. Please try again.');
    } finally {
      setIsLoading(false);
    }
  };

  return (
    <div className="min-h-screen flex items-center justify-center bg-arena-bg hex-grid-bg">
      <div className="max-w-md w-full mx-4">

        {/* Logo */}
        <div className="text-center mb-8">
          <h1 className="text-4xl font-display text-transparent bg-clip-text bg-gradient-to-r from-neon-gold via-orange-400 to-neon-gold uppercase tracking-widest mb-2">
            CR Exchange
          </h1>
          <p className="text-slate-500 text-sm uppercase tracking-widest">Sign in to trade</p>
        </div>

        {/* Card */}
        <div className="bg-arena-surface/80 backdrop-blur-sm border border-arena-border rounded-2xl p-8 shadow-2xl shadow-neon-purple/5">
          {error && (
            <div className="mb-6 p-3 bg-bear-red/10 border border-bear-red/30 rounded-lg">
              <p className="text-bear-red text-sm font-bold">{error}</p>
            </div>
          )}

          <form onSubmit={handleSubmit} className="space-y-5">
            <div>
              <label className="block text-xs text-slate-400 uppercase tracking-widest font-bold mb-2">Username</label>
              <input
                type="text"
                value={username}
                onChange={e => setUsername(e.target.value)}
                required
                className="w-full px-4 py-3 bg-slate-950/60 border border-slate-700 rounded-lg text-white placeholder-slate-600 focus:outline-none focus:border-neon-purple/70 focus:ring-1 focus:ring-neon-purple/30 transition font-mono"
                placeholder="your_username"
              />
            </div>

            <div>
              <label className="block text-xs text-slate-400 uppercase tracking-widest font-bold mb-2">Password</label>
              <input
                type="password"
                value={password}
                onChange={e => setPassword(e.target.value)}
                required
                className="w-full px-4 py-3 bg-slate-950/60 border border-slate-700 rounded-lg text-white placeholder-slate-600 focus:outline-none focus:border-neon-purple/70 focus:ring-1 focus:ring-neon-purple/30 transition font-mono"
                placeholder="••••••••"
              />
            </div>

            <button
              type="submit"
              disabled={isLoading}
              className="w-full py-3 bg-gradient-to-r from-neon-purple/30 to-neon-cyan/20 text-white font-black uppercase tracking-widest rounded-lg border border-neon-purple/40 hover:border-neon-purple/70 hover:from-neon-purple/40 transition-all disabled:opacity-50 disabled:cursor-not-allowed shadow-lg shadow-neon-purple/10"
            >
              {isLoading ? 'Signing in…' : 'Sign In'}
            </button>
          </form>

          <p className="text-center text-slate-600 text-sm mt-6">
            No account?{' '}
            <Link to="/register" className="text-neon-purple hover:text-neon-cyan font-bold transition">
              Create one
            </Link>
          </p>
        </div>

        <p className="text-center text-slate-700 text-xs mt-6 uppercase tracking-widest">
          Start with 100,000 gold · 121 cards to trade
        </p>
      </div>
    </div>
  );
}
