/**
 * k6 end-to-end load test — order placement
 *
 * Measures full HTTP round-trip latency: TCP → HTTP parse → JWT verify →
 * order validate → OrderBook match → DB write → HTTP response.
 *
 * Usage:
 *   k6 run --env TOKEN=<jwt> backend/load_tests/order_placement.js
 *
 * Get TOKEN via run_load_test.sh, or manually:
 *   TOKEN=$(curl -s -X POST http://localhost:8080/api/v1/auth/login \
 *     -H 'Content-Type: application/json' \
 *     -d '{"username":"load_test_user","password":"load_test_pass"}' \
 *     | python3 -c "import sys,json; print(json.load(sys.stdin)['token'])")
 *
 * Error counting: only 5xx responses are counted as server errors.
 * 4xx (insufficient balance, no inventory) are valid business-logic rejections.
 *
 * Thresholds:
 *   p95 < 150ms, p99 < 300ms, 5xx error rate < 1%
 */

import http from 'k6/http';
import { check, sleep } from 'k6';
import { Rate, Trend } from 'k6/metrics';

const serverErrorRate = new Rate('server_errors');
const orderLatency    = new Trend('order_latency_ms', true);

export const options = {
  stages: [
    { duration: '30s', target: 20 },
    { duration: '2m',  target: 50 },
    { duration: '30s', target: 100 },
    { duration: '30s', target: 0 },
  ],
  thresholds: {
    http_req_duration: ['p(95)<150', 'p(99)<300'],
    server_errors:     ['rate<0.01'],
  },
  summaryTrendStats: ['avg', 'min', 'med', 'max', 'p(90)', 'p(95)', 'p(99)'],
};

const BASE_URL = __ENV.BASE_URL || 'http://localhost:8080';
const TOKEN    = __ENV.TOKEN;

// setup() runs once before VUs start — fetches real card_ids from the API
// so the test doesn't hardcode IDs that may differ from what's in the DB.
export function setup() {
  const res = http.get(`${BASE_URL}/api/v1/cards`, {
    headers: { 'Authorization': `Bearer ${TOKEN}` },
  });

  if (res.status !== 200) {
    throw new Error(`Failed to fetch cards: ${res.status} ${res.body}`);
  }

  let cards;
  try {
    cards = JSON.parse(res.body);
  } catch {
    throw new Error(`Could not parse /api/v1/cards response: ${res.body}`);
  }

  // Expect either an array or { cards: [...] }
  const list = Array.isArray(cards) ? cards : (cards.cards || []);
  if (list.length === 0) {
    throw new Error('No cards returned from /api/v1/cards — seed the DB first');
  }

  const cardIds = list.map((c) => c.card_id || c.id).filter(Boolean);
  console.log(`Setup: found ${cardIds.length} cards: ${cardIds.slice(0, 5).join(', ')}...`);
  return { cardIds };
}

const BUY_PRICES = [1200, 1220, 1240, 1260, 1280];
const SEL_PRICES = [1310, 1330, 1350, 1370, 1390];

export default function (data) {
  if (!TOKEN) {
    console.error('TOKEN env var required — see run_load_test.sh');
    return;
  }

  const { cardIds } = data;
  const card  = cardIds[__VU % cardIds.length];
  const isBuy = __VU % 2 === 0;
  const prices = isBuy ? BUY_PRICES : SEL_PRICES;
  const price  = prices[Math.floor(Math.random() * prices.length)];

  const payload = JSON.stringify({
    card_id:  card,
    type:     isBuy ? 'BUY' : 'SELL',
    mode:     'LIMIT',
    price:    price,
    quantity: 1,
  });

  const headers = {
    'Content-Type':  'application/json',
    'Authorization': `Bearer ${TOKEN}`,
  };

  const res = http.post(`${BASE_URL}/api/v1/orders`, payload, { headers, timeout: '5s' });

  const isServerError = res.status >= 500;
  serverErrorRate.add(isServerError);
  orderLatency.add(res.timings.duration);

  check(res, {
    'not 5xx':   (r) => r.status < 500,
    'responded': (r) => r.status !== 0,
  });

  sleep(0.1);
}

export function handleSummary(data) {
  return {
    'backend/load_tests/results/latest.json': JSON.stringify(data, null, 2),
    stdout: formatSummary(data),
  };
}

function formatSummary(data) {
  const d = data.metrics.http_req_duration?.values;
  if (!d) return 'No HTTP metrics recorded.\n';

  const fmt  = (v) => v !== undefined ? v.toFixed(1) : 'n/a';
  const reqs = data.metrics.http_reqs?.values?.count ?? 0;
  const dur  = data.state?.testRunDurationMs ?? 0;
  const rps  = dur > 0 ? (reqs / (dur / 1000)).toFixed(0) : 'n/a';
  const err5xx = ((data.metrics.server_errors?.values?.rate ?? 0) * 100).toFixed(2);

  return [
    '',
    '=== k6 Load Test Results ===',
    `  Total requests:  ${reqs} over ${(dur / 1000).toFixed(0)}s`,
    `  Peak throughput: ~${rps} req/s`,
    `  Latency p50:     ${fmt(d.med)} ms`,
    `  Latency p90:     ${fmt(d['p(90)'])} ms`,
    `  Latency p95:     ${fmt(d['p(95)'])} ms`,
    `  Latency p99:     ${fmt(d['p(99)'])} ms`,
    `  Latency max:     ${fmt(d.max)} ms`,
    `  5xx error rate:  ${err5xx}%`,
    '',
  ].join('\n');
}
