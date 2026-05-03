interface Props {
  rank: string;
  xp: number;
  compact?: boolean;
}

interface RankConfig {
  color: string;
  bg: string;
  border: string;
  next: number;
  prev: number;
}

const RANK_CONFIG: Record<string, RankConfig> = {
  'Goblin Stadium':    { color: 'text-slate-400',   bg: 'bg-slate-700/30',   border: 'border-slate-600/50',  prev: 0,     next: 100 },
  'Challenger':        { color: 'text-blue-400',    bg: 'bg-blue-900/20',    border: 'border-blue-500/40',   prev: 100,   next: 500 },
  'Master':            { color: 'text-purple-400',  bg: 'bg-purple-900/20',  border: 'border-purple-500/40', prev: 500,   next: 2000 },
  'Grand Champion':    { color: 'text-neon-gold',   bg: 'bg-yellow-900/20',  border: 'border-yellow-500/40', prev: 2000,  next: 10000 },
  'Ultimate Champion': { color: 'text-neon-cyan',   bg: 'bg-cyan-900/20',    border: 'border-cyan-500/40',   prev: 10000, next: Infinity },
};

export default function RankBadge({ rank, xp, compact = false }: Props) {
  const cfg = RANK_CONFIG[rank] ?? RANK_CONFIG['Goblin Stadium'];
  const isMax = cfg.next === Infinity;
  const progress = isMax ? 100 : Math.min(((xp - cfg.prev) / (cfg.next - cfg.prev)) * 100, 100);
  const xpToNext = isMax ? 0 : cfg.next - xp;

  if (compact) {
    return (
      <span className={`inline-flex items-center gap-1.5 px-2 py-0.5 rounded text-xs font-bold border ${cfg.color} ${cfg.bg} ${cfg.border}`}>
        {rank}
      </span>
    );
  }

  return (
    <div className={`flex flex-col gap-1 px-3 py-2 rounded-lg border ${cfg.bg} ${cfg.border}`}>
      <div className="flex items-center justify-between gap-3">
        <span className={`text-xs font-bold uppercase tracking-wide ${cfg.color}`}>{rank}</span>
        <span className="text-xs text-slate-500 font-mono">{xp} XP</span>
      </div>
      <div className="w-full h-1 bg-slate-800 rounded-full overflow-hidden">
        <div
          className={`h-full rounded-full transition-all duration-500 ${
            isMax ? 'bg-neon-cyan' :
            rank === 'Grand Champion' ? 'bg-neon-gold' :
            rank === 'Master' ? 'bg-purple-400' :
            rank === 'Challenger' ? 'bg-blue-400' : 'bg-slate-400'
          }`}
          style={{ width: `${progress}%` }}
        />
      </div>
      {!isMax && (
        <p className="text-xs text-slate-600">{xpToNext} XP to {Object.keys(RANK_CONFIG)[Object.keys(RANK_CONFIG).indexOf(rank) + 1]}</p>
      )}
    </div>
  );
}
