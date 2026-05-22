import { useEffect, useRef, useState } from 'react';
import { createChart } from 'lightweight-charts';

interface CandlestickChartProps {
  cardId: string;
  cardName: string;
}

interface Candle {
  timestamp: number;
  open: number;
  high: number;
  low: number;
  close: number;
  volume: number;
  trade_count: number;
}

type Timeframe = '1m' | '5m' | '15m' | '1h' | '4h' | '1d';

const TIMEFRAMES: { value: Timeframe; label: string }[] = [
  { value: '1m',  label: '1M'  },
  { value: '5m',  label: '5M'  },
  { value: '15m', label: '15M' },
  { value: '1h',  label: '1H'  },
  { value: '4h',  label: '4H'  },
  { value: '1d',  label: '1D'  },
];

export default function CandlestickChart({ cardId, cardName }: CandlestickChartProps) {
  const wrapperRef      = useRef<HTMLDivElement>(null);
  const chartAreaRef    = useRef<HTMLDivElement>(null);
  const chartRef        = useRef<any>(null);
  const candleSeriesRef = useRef<any>(null);
  const volumeSeriesRef = useRef<any>(null);

  const [timeframe, setTimeframe] = useState<Timeframe>('1m');
  const [loading, setLoading]     = useState(true);
  const [error, setError]         = useState('');

  useEffect(() => {
    if (!chartAreaRef.current) return;

    const chart = createChart(chartAreaRef.current, {
      width:  chartAreaRef.current.clientWidth  || 600,
      height: chartAreaRef.current.clientHeight || 300,
      layout: {
        background:  { color: 'transparent' },
        textColor:   '#64748b',
      },
      grid: {
        vertLines: { color: 'rgba(99, 102, 241, 0.06)' },
        horzLines: { color: 'rgba(99, 102, 241, 0.06)' },
      },
      crosshair: { mode: 1 },
      rightPriceScale: { borderColor: 'rgba(99, 102, 241, 0.2)' },
      timeScale: {
        borderColor:    'rgba(99, 102, 241, 0.2)',
        timeVisible:    true,
        secondsVisible: false,
      },
    });

    chartRef.current = chart;

    candleSeriesRef.current = chart.addCandlestickSeries({
      upColor:      '#00ff88',
      downColor:    '#ff3b5c',
      borderVisible: false,
      wickUpColor:   '#00ff88',
      wickDownColor: '#ff3b5c',
    });

    volumeSeriesRef.current = chart.addHistogramSeries({
      color:       '#64748b',
      priceFormat: { type: 'volume' },
      priceScaleId: '',
    });

    // ResizeObserver picks up flex layout changes that window resize misses
    const ro = new ResizeObserver(entries => {
      const { width, height } = entries[0].contentRect;
      if (width > 0 && height > 0) {
        chart.applyOptions({ width, height });
      }
    });
    ro.observe(chartAreaRef.current);

    return () => {
      ro.disconnect();
      chart.remove();
    };
  }, []);

  useEffect(() => { fetchCandles(); }, [cardId, timeframe]);

  const fetchCandles = async () => {
    setLoading(true);
    setError('');
    try {
      const res = await fetch(
        `http://localhost:8080/api/v1/cards/${cardId}/candles?timeframe=${timeframe}&limit=500`
      );
      if (!res.ok) throw new Error('Failed to fetch candles');

      const data = await res.json();
      const candles: Candle[] = (data.candles as Candle[]).sort((a, b) => a.timestamp - b.timestamp);

      if (candles.length === 0) {
        setError('No price data yet — make some trades to generate charts!');
        setLoading(false);
        return;
      }

      candleSeriesRef.current?.setData(
        candles.map(c => ({ time: c.timestamp, open: c.open, high: c.high, low: c.low, close: c.close }))
      );
      volumeSeriesRef.current?.setData(
        candles.map(c => ({
          time:  c.timestamp,
          value: c.volume,
          color: c.close >= c.open ? '#00ff8830' : '#ff3b5c30',
        }))
      );
      chartRef.current?.timeScale().fitContent();
      setLoading(false);
    } catch (err: any) {
      setError(err.message || 'Failed to load chart data');
      setLoading(false);
    }
  };

  return (
    <div ref={wrapperRef} className="h-full flex flex-col overflow-hidden">

      {/* Header */}
      <div className="flex-shrink-0 flex items-center justify-between px-3 py-1.5 border-b border-arena-border">
        <p className="text-[10px] font-bold uppercase tracking-widest text-slate-500">
          Price Chart — <span className="text-slate-300">{cardName}</span>
        </p>
        <div className="flex gap-1">
          {TIMEFRAMES.map(tf => (
            <button
              key={tf.value}
              onClick={() => setTimeframe(tf.value)}
              className={`px-2 py-0.5 text-[10px] font-bold uppercase rounded transition ${
                timeframe === tf.value
                  ? 'bg-neon-gold/20 text-neon-gold border border-neon-gold/40'
                  : 'text-slate-600 border border-transparent hover:text-slate-400'
              }`}
            >
              {tf.label}
            </button>
          ))}
        </div>
      </div>

      {/* Chart area — fills remaining height */}
      <div ref={chartAreaRef} className="flex-1 relative overflow-hidden">
        {loading && (
          <div className="absolute inset-0 flex items-center justify-center bg-arena-bg/70 z-10">
            <p className="text-slate-500 text-xs uppercase tracking-widest animate-pulse">Loading…</p>
          </div>
        )}
        {error && !loading && (
          <div className="absolute inset-0 flex items-center justify-center z-10">
            <div className="text-center">
              <p className="text-slate-500 text-xs mb-3">{error}</p>
              <button
                onClick={fetchCandles}
                className="px-3 py-1 text-xs font-bold uppercase tracking-wide text-neon-gold border border-neon-gold/30 rounded hover:border-neon-gold/60 transition"
              >
                Retry
              </button>
            </div>
          </div>
        )}
      </div>
    </div>
  );
}
