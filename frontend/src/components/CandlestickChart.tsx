import { useEffect, useRef, useState } from 'react';
import { createChart} from 'lightweight-charts';

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

export default function CandlestickChart({ cardId, cardName }: CandlestickChartProps) {
  const chartContainerRef = useRef<HTMLDivElement>(null);
  const chartRef = useRef<any>(null);
  const candlestickSeriesRef = useRef<any>(null);
  const volumeSeriesRef = useRef<any>(null);

  const [timeframe, setTimeframe] = useState<Timeframe>('1m');
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string>('');

  useEffect(() => {
    if (!chartContainerRef.current) return;

    const chart = createChart(chartContainerRef.current, {
      width: chartContainerRef.current.clientWidth,
      height: 400,
      layout: {
        background: { color: 'transparent' },
        textColor: '#9ca3af',
      },
      grid: {
        vertLines: { color: 'rgba(148, 163, 184, 0.1)' },
        horzLines: { color: 'rgba(148, 163, 184, 0.1)' },
      },
      crosshair: {
        mode: 1,
      },
      rightPriceScale: {
        borderColor: 'rgba(148, 163, 184, 0.3)',
      },
      timeScale: {
        borderColor: 'rgba(148, 163, 184, 0.3)',
        timeVisible: true,
        secondsVisible: false,
      },
    });

    chartRef.current = chart;

    const candlestickSeries = chart.addCandlestickSeries({
      upColor: '#22c55e',
      downColor: '#ef4444',
      borderVisible: false,
      wickUpColor: '#22c55e',
      wickDownColor: '#ef4444',
    });

    candlestickSeriesRef.current = candlestickSeries;

    const volumeSeries = chart.addHistogramSeries({
      color: '#64748b',
      priceFormat: {
        type: 'volume',
      },
      priceScaleId: '',
    });

    volumeSeriesRef.current = volumeSeries;

    const handleResize = () => {
      if (chartContainerRef.current && chartRef.current) {
        chartRef.current.applyOptions({
          width: chartContainerRef.current.clientWidth,
        });
      }
    };

    window.addEventListener('resize', handleResize);

    return () => {
      window.removeEventListener('resize', handleResize);
      chart.remove();
    };
  }, []);

  useEffect(() => {
    fetchCandles();
  }, [cardId, timeframe]);

  const fetchCandles = async () => {
    setLoading(true);
    setError('');

    try {
      const response = await fetch(
        `http://localhost:8080/api/v1/cards/${cardId}/candles?timeframe=${timeframe}&limit=500`
      );

      if (!response.ok) {
        throw new Error('Failed to fetch candles');
      }

      const data = await response.json();
      const candles: Candle[] = (data.candles as Candle[])
        .sort((a, b) => a.timestamp - b.timestamp);

      if (candles.length === 0) {
        setError('No price data yet. Make some trades to generate charts!');
        setLoading(false);
        return;
      }

      const candlestickData = candles.map(candle => ({
        time: candle.timestamp,
        open: candle.open,
        high: candle.high,
        low: candle.low,
        close: candle.close,
      }));

      const volumeData = candles.map(candle => ({
        time: candle.timestamp,
        value: candle.volume,
        color: candle.close >= candle.open ? '#22c55e40' : '#ef444440',
      }));

      if (candlestickSeriesRef.current && volumeSeriesRef.current) {
        candlestickSeriesRef.current.setData(candlestickData);
        volumeSeriesRef.current.setData(volumeData);
        
        if (chartRef.current) {
          chartRef.current.timeScale().fitContent();
        }
      }

      setLoading(false);
    } catch (err: any) {
      console.error('Failed to fetch candles:', err);
      setError(err.message || 'Failed to load chart data');
      setLoading(false);
    }
  };

  const timeframes: { value: Timeframe; label: string }[] = [
    { value: '1m', label: '1M' },
    { value: '5m', label: '5M' },
    { value: '15m', label: '15M' },
    { value: '1h', label: '1H' },
    { value: '4h', label: '4H' },
    { value: '1d', label: '1D' },
  ];

  return (
    <div className="bg-slate-900/80 border-2 border-orange-500/30 rounded-xl p-6 backdrop-blur-sm">
      <div className="flex justify-between items-center mb-4">
        <h2 className="text-orange-400 font-black uppercase tracking-wider text-lg">
          Price Chart - {cardName}
        </h2>

        <div className="flex gap-2">
          {timeframes.map(tf => (
            <button
              key={tf.value}
              onClick={() => setTimeframe(tf.value)}
              className={`px-3 py-1.5 rounded-lg font-bold text-xs uppercase tracking-wide border-2 transition ${
                timeframe === tf.value
                  ? 'bg-orange-500 text-white border-orange-400'
                  : 'bg-slate-800 text-slate-400 border-slate-700 hover:border-orange-500/50'
              }`}
            >
              {tf.label}
            </button>
          ))}
        </div>
      </div>

      <div className="relative">
        {loading && (
          <div className="absolute inset-0 flex items-center justify-center bg-slate-950/50 rounded-lg z-10">
            <p className="text-slate-400 font-bold">Loading chart...</p>
          </div>
        )}

        {error && (
          <div className="absolute inset-0 flex items-center justify-center bg-slate-950/50 rounded-lg z-10">
            <div className="text-center">
              <p className="text-orange-400 font-bold mb-2">{error}</p>
              <button
                onClick={fetchCandles}
                className="px-4 py-2 bg-orange-500 text-white rounded-lg font-bold hover:bg-orange-600"
              >
                Retry
              </button>
            </div>
          </div>
        )}

        <div ref={chartContainerRef} className="rounded-lg overflow-hidden" />
      </div>
    </div>
  );
}