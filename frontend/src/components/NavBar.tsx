import { useNavigate } from 'react-router-dom';
import { useAuth } from '../contexts/AuthContext';
import RankBadge from './RankBadge';

type Page = 'dashboard' | 'trading' | 'portfolio';

const NAV_LINKS: { label: string; path: string; page: Page }[] = [
  { label: 'Dashboard', path: '/dashboard', page: 'dashboard' },
  { label: 'Trading',   path: '/trading',   page: 'trading'   },
  { label: 'Portfolio', path: '/portfolio', page: 'portfolio' },
];

interface Props {
  activePage: Page;
}

export default function NavBar({ activePage }: Props) {
  const { user, logout } = useAuth();
  const navigate = useNavigate();

  return (
    <nav className="border-b border-arena-border bg-arena-surface/80 backdrop-blur-sm">
      <div className="max-w-6xl mx-auto px-6 h-12 flex items-center justify-between">

        <div className="flex items-center gap-6">
          <h1 className="text-sm font-display text-transparent bg-clip-text bg-gradient-to-r from-neon-gold to-orange-400 uppercase tracking-widest">
            CR Exchange
          </h1>
          {NAV_LINKS.map(({ label, path, page }) => (
            <button
              key={page}
              onClick={() => navigate(path)}
              className={`text-xs font-bold uppercase tracking-wide transition ${
                activePage === page
                  ? 'text-white border-b-2 border-neon-gold pb-0.5'
                  : 'text-slate-500 hover:text-slate-300'
              }`}
            >
              {label}
            </button>
          ))}
        </div>

        <div className="flex items-center gap-4">
          {user && <RankBadge rank={user.trader_level || 'Goblin Stadium'} xp={user.xp ?? 0} compact />}
          <button
            onClick={() => { logout(); navigate('/login'); }}
            className="text-xs text-bear-red/70 border border-bear-red/30 rounded px-3 py-1 hover:border-bear-red hover:text-bear-red transition uppercase tracking-wide"
          >
            Logout
          </button>
        </div>

      </div>
    </nav>
  );
}
