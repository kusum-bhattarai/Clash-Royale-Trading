import { useState, useEffect } from 'react';
import { cardsAPI } from '../services/api';
import type { PriceData } from '../types/api';

interface Props {
  cardId: string;
  lastPrice: number;
}

export default function FairValueIndicator({ cardId, lastPrice }: Props) {
  const [data, setData] = useState<PriceData | null>(null);

  useEffect(() => {
    let cancelled = false;
    cardsAPI.getPrice(cardId)
      .then(d => { if (!cancelled) setData(d); })
      .catch(() => {});
    return () => { cancelled = true; };
  }, [cardId]);

  if (!data) return null;

  const ref = data.reference_price;
  const diff = lastPrice > 0 ? ((lastPrice - ref) / ref) * 100 : 0;
  const overvalued = lastPrice > ref * 1.02;
  const undervalued = lastPrice < ref * 0.98;

  const labelColor = overvalued
    ? 'text-bear-red'
    : undervalued
    ? 'text-bull-green'
    : 'text-neon-gold';

  const label = overvalued ? 'OVERVALUED' : undervalued ? 'UNDERVALUED' : 'FAIR VALUE';
  const bgColor = overvalued
    ? 'bg-bear-red/10 border-bear-red/30'
    : undervalued
    ? 'bg-bull-green/10 border-bull-green/30'
    : 'bg-neon-gold/10 border-neon-gold/30';

  return (
    <div className={`flex items-center gap-4 px-4 py-2 border-b border-arena-border text-xs ${bgColor} border`}>
      <span className="text-slate-400 uppercase tracking-wider font-bold">Fair Value</span>
      <span className="text-white font-mono font-bold">{ref.toFixed(0)}g</span>
      <span className="text-slate-500">vs last {lastPrice > 0 ? lastPrice.toFixed(0) + 'g' : '—'}</span>
      {lastPrice > 0 && (
        <>
          <span className={`font-bold uppercase tracking-widest ${labelColor}`}>{label}</span>
          <span className={`font-mono ${labelColor}`}>
            {diff > 0 ? '+' : ''}{diff.toFixed(1)}%
          </span>
        </>
      )}
      <span className="ml-auto text-slate-600 font-mono">
        base {data.factors.base} × sd {data.factors.sd.toFixed(2)} × meta {data.factors.meta.toFixed(2)} × vol {data.factors.vol_discount.toFixed(2)}
      </span>
    </div>
  );
}
