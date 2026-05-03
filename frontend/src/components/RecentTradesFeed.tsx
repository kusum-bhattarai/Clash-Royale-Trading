import { AnimatePresence, motion } from 'framer-motion';
import type { Trade } from '../types/api';

interface Props {
  trades: Trade[];
  currentUserId?: string;
}

function timeAgo(iso: string): string {
  const diff = Date.now() - new Date(iso).getTime();
  if (diff < 60000) return `${Math.floor(diff / 1000)}s ago`;
  if (diff < 3600000) return `${Math.floor(diff / 60000)}m ago`;
  return `${Math.floor(diff / 3600000)}h ago`;
}

export default function RecentTradesFeed({ trades, currentUserId }: Props) {
  return (
    <div className="flex flex-col h-full">
      <div className="px-3 py-2 border-b border-arena-border">
        <p className="text-xs text-slate-500 uppercase tracking-widest font-bold">Recent Trades</p>
      </div>
      <div className="flex-1 overflow-y-auto arena-scroll">
        {trades.length === 0 ? (
          <p className="text-slate-600 text-xs text-center py-6">No trades yet</p>
        ) : (
          <AnimatePresence initial={false}>
            {trades.map((trade) => {
              const isBuy = trade.buyer_id === currentUserId;
              const isSell = trade.seller_id === currentUserId;
              const side = isBuy ? 'bull-green' : isSell ? 'bear-red' : 'slate-400';
              return (
                <motion.div
                  key={trade.trade_id}
                  initial={{ opacity: 0, y: -8 }}
                  animate={{ opacity: 1, y: 0 }}
                  exit={{ opacity: 0 }}
                  transition={{ duration: 0.2 }}
                  className="flex items-center justify-between px-3 py-1.5 border-b border-slate-800/50 hover:bg-white/2"
                >
                  <div className="flex items-center gap-2">
                    <span className={`w-1 h-1 rounded-full bg-${side} flex-shrink-0`} />
                    <span className={`text-xs font-mono font-bold text-${side}`}>
                      {trade.price.toFixed(0)}g
                    </span>
                  </div>
                  <span className="text-xs text-slate-500">×{trade.quantity}</span>
                  <span className="text-xs text-slate-600">
                    {trade.executed_at ? timeAgo(trade.executed_at) : '—'}
                  </span>
                </motion.div>
              );
            })}
          </AnimatePresence>
        )}
      </div>
    </div>
  );
}
