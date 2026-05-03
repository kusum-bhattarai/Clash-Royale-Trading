import { useMemo } from 'react';
import { AreaChart, Area, XAxis, YAxis, Tooltip, ResponsiveContainer } from 'recharts';

interface Props {
  bids: [number, number][];
  asks: [number, number][];
}

interface DepthPoint {
  price: number;
  bidVol: number | null;
  askVol: number | null;
}

function computeDepthData(bids: [number, number][], asks: [number, number][]): DepthPoint[] {
  // Bids arrive sorted high→low; accumulate and reverse so chart is low→high
  let cumBid = 0;
  const bidPoints: DepthPoint[] = [...bids].reverse().map(([price, qty]) => {
    cumBid += qty;
    return { price, bidVol: cumBid, askVol: null };
  });
  bidPoints.reverse(); // back to high→low for correct display

  // Asks arrive sorted low→high; accumulate
  let cumAsk = 0;
  const askPoints: DepthPoint[] = asks.map(([price, qty]) => {
    cumAsk += qty;
    return { price, askVol: cumAsk, bidVol: null };
  });

  return [...bidPoints, ...askPoints].sort((a, b) => a.price - b.price);
}

const CustomTooltip = ({ active, payload, label }: any) => {
  if (!active || !payload?.length) return null;
  const p = payload[0];
  const isBid = p.dataKey === 'bidVol';
  return (
    <div className="bg-arena-surface border border-arena-border rounded px-3 py-2 text-xs">
      <p className="text-slate-400">{label}g</p>
      <p className={isBid ? 'text-bull-green' : 'text-bear-red'}>
        {isBid ? 'Bid vol' : 'Ask vol'}: {p.value}
      </p>
    </div>
  );
};

export default function DepthChart({ bids, asks }: Props) {
  const data = useMemo(() => computeDepthData(bids, asks), [bids, asks]);

  if (data.length === 0) {
    return (
      <div className="flex items-center justify-center h-full text-slate-600 text-sm">
        No depth data
      </div>
    );
  }

  return (
    <ResponsiveContainer width="100%" height="100%">
      <AreaChart data={data} margin={{ top: 4, right: 8, bottom: 4, left: 0 }}>
        <defs>
          <linearGradient id="bidGrad" x1="0" y1="0" x2="0" y2="1">
            <stop offset="5%" stopColor="#00ff88" stopOpacity={0.3} />
            <stop offset="95%" stopColor="#00ff88" stopOpacity={0.02} />
          </linearGradient>
          <linearGradient id="askGrad" x1="0" y1="0" x2="0" y2="1">
            <stop offset="5%" stopColor="#ff3b5c" stopOpacity={0.3} />
            <stop offset="95%" stopColor="#ff3b5c" stopOpacity={0.02} />
          </linearGradient>
        </defs>
        <XAxis
          dataKey="price"
          tick={{ fill: '#64748b', fontSize: 10 }}
          tickLine={false}
          axisLine={false}
          tickFormatter={v => v.toFixed(0)}
        />
        <YAxis hide />
        <Tooltip content={<CustomTooltip />} />
        <Area
          type="stepAfter"
          dataKey="bidVol"
          stroke="#00ff88"
          strokeWidth={1.5}
          fill="url(#bidGrad)"
          connectNulls={false}
          dot={false}
          isAnimationActive={false}
        />
        <Area
          type="stepAfter"
          dataKey="askVol"
          stroke="#ff3b5c"
          strokeWidth={1.5}
          fill="url(#askGrad)"
          connectNulls={false}
          dot={false}
          isAnimationActive={false}
        />
      </AreaChart>
    </ResponsiveContainer>
  );
}
