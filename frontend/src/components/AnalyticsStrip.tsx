import { useState, useEffect } from 'react';
import { cardsAPI } from '../services/api';
import type { AnalyticsSnapshot } from '../types/api';

interface Props {
  cardId: string;
}

const PLACEHOLDER: AnalyticsSnapshot = {
  spread: { instantaneous: 0, twas_1h: 0, relative_pct: 0 },
  order_flow_imbalance: { '1m': 0, '5m': 0 },
  price_impact_bps: 0,
  volatility: { '1m': 0, '1h': 0 },
  vwap: 0,
};

function fmt(n: number, digits = 2): string {
  return n === 0 ? '—' : n.toFixed(digits);
}

export default function AnalyticsStrip({ cardId }: Props) {
  const [data, setData] = useState<AnalyticsSnapshot>(PLACEHOLDER);
  const [loaded, setLoaded] = useState(false);

  useEffect(() => {
    let cancelled = false;
    setLoaded(false);
    setData(PLACEHOLDER);

    const fetchData = async () => {
      try {
        const snap = await cardsAPI.getAnalytics(cardId);
        if (!cancelled) {
          setData(snap);
          setLoaded(true);
        }
      } catch {
        if (!cancelled) setLoaded(true); // show zeros gracefully
      }
    };

    fetchData();
    const interval = setInterval(fetchData, 15_000);
    return () => {
      cancelled = true;
      clearInterval(interval);
    };
  }, [cardId]);

  const ofi = data.order_flow_imbalance['1m'];
  const ofiPositive = ofi > 0;
  const ofiBarWidth = `${Math.abs(ofi) * 50}%`;
  const ofiBarLeft = ofiPositive ? '50%' : `${50 + ofi * 50}%`;
  const ofiColor = ofi > 0.1 ? 'text-green-400' : ofi < -0.1 ? 'text-red-400' : 'text-slate-300';
  const ofiLabel = ofi === 0 ? '—' : `${ofi > 0 ? '+' : ''}${ofi.toFixed(2)}`;

  const vol1m = data.volatility['1m'];
  const vol1h = data.volatility['1h'];

  return (
    <div className="bg-slate-900/80 border-2 border-cyan-500/20 rounded-xl p-4 backdrop-blur-sm">
      <p className="text-xs text-cyan-400/50 uppercase font-bold tracking-widest mb-3">
        Market Analytics
        {!loaded && <span className="ml-2 text-slate-600 normal-case font-normal">loading…</span>}
      </p>

      <div className="grid grid-cols-4 gap-3">

        {/* Spread */}
        <div className="bg-slate-950/50 p-3 rounded-lg border border-slate-700">
          <p className="text-xs text-slate-400 uppercase font-bold tracking-wide mb-1">Spread</p>
          <p className="text-cyan-400 font-mono font-bold text-lg">
            {fmt(data.spread.instantaneous)}
          </p>
          <p className="text-xs text-slate-500">
            {fmt(data.spread.relative_pct, 3)}% of mid
          </p>
        </div>

        {/* OFI */}
        <div className="bg-slate-950/50 p-3 rounded-lg border border-slate-700">
          <p className="text-xs text-slate-400 uppercase font-bold tracking-wide mb-1">OFI 1m</p>
          <p className={`font-mono font-bold text-lg ${ofiColor}`}>{ofiLabel}</p>
          {/* [-1 ── 0 ── +1] bar */}
          <div className="relative mt-2 h-1.5 bg-slate-700 rounded-full overflow-hidden">
            {ofi !== 0 && (
              <div
                className={`absolute h-full rounded-full transition-all duration-500 ${
                  ofiPositive ? 'bg-green-500' : 'bg-red-500'
                }`}
                style={{ width: ofiBarWidth, left: ofiBarLeft }}
              />
            )}
            <div className="absolute h-full w-px bg-slate-500" style={{ left: '50%' }} />
          </div>
          <p className="text-xs text-slate-600 mt-1">
            5m: {fmt(data.order_flow_imbalance['5m'])}
          </p>
        </div>

        {/* Volatility */}
        <div className="bg-slate-950/50 p-3 rounded-lg border border-slate-700">
          <p className="text-xs text-slate-400 uppercase font-bold tracking-wide mb-1">Volatility</p>
          <p className="text-purple-400 font-mono font-bold text-lg">
            {vol1m > 0 ? `${(vol1m * 100).toFixed(3)}%` : '—'}
          </p>
          <p className="text-xs text-slate-500">
            1h: {vol1h > 0 ? `${(vol1h * 100).toFixed(3)}%` : '—'}
          </p>
        </div>

        {/* VWAP */}
        <div className="bg-slate-950/50 p-3 rounded-lg border border-slate-700">
          <p className="text-xs text-slate-400 uppercase font-bold tracking-wide mb-1">VWAP</p>
          <p className="text-yellow-400 font-mono font-bold text-lg">
            {data.vwap > 0 ? data.vwap.toFixed(0) : '—'}
          </p>
          <p className="text-xs text-slate-500">Session avg</p>
        </div>

      </div>
    </div>
  );
}
