import type { Trade, Card } from '../types/api';

interface Props {
  trades: Trade[];
  cards: Card[];
}

export default function TradeTickerBar({ trades, cards }: Props) {
  const cardMap = Object.fromEntries(cards.map(c => [c.card_id, c.name]));

  if (trades.length === 0) return (
    <div className="h-7 flex-shrink-0 border-t border-arena-border bg-arena-surface flex items-center px-4">
      <span className="text-xs text-slate-600">Waiting for trades…</span>
    </div>
  );

  // Duplicate items so the CSS scroll loop is seamless
  const items = [...trades, ...trades];

  return (
    <div className="h-7 flex-shrink-0 border-t border-arena-border bg-arena-surface overflow-hidden flex items-center">
      <span className="text-xs text-slate-600 px-3 border-r border-arena-border mr-2 flex-shrink-0 uppercase tracking-widest">
        Live
      </span>
      <div className="flex-1 overflow-hidden">
        <div className="flex gap-6 animate-ticker whitespace-nowrap">
          {items.map((trade, idx) => {
            const name = cardMap[trade.card_id] || trade.card_id;
            return (
              <span key={`${trade.trade_id}-${idx}`} className="text-xs flex-shrink-0 flex items-center gap-1.5">
                <span className="text-slate-300 font-semibold">{name}</span>
                <span className="text-neon-gold font-mono font-bold">{trade.price.toFixed(0)}g</span>
                <span className="text-slate-600">×{trade.quantity}</span>
                <span className="text-slate-700">•</span>
              </span>
            );
          })}
        </div>
      </div>
    </div>
  );
}
