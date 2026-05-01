# Clash Royale Trading Exchange

![CI](https://github.com/kusum-bhattarai/Clash-Royale-Trading/actions/workflows/ci.yml/badge.svg)

A real-time card trading platform modeled on financial exchanges. Players trade Clash Royale cards on a live order book with price-time priority matching, atomic settlement, and WebSocket-driven market data.

Built with a C++ matching engine, PostgreSQL settlement layer, and a React/TypeScript frontend.

---

## Performance

Benchmarked with [Google Benchmark](https://github.com/google/benchmark). CI numbers (Linux x86, 2 vCPU GitHub runner) are the reproducible baseline; local numbers are on Apple M-series.

| Metric | CI Linux x86 | Local macOS M-series |
|--------|-------------|----------------------|
| Single-fill match latency | **3.7 µs** | 4.7 µs (high load) |
| — of which SHA-256 Merkle hash | **2.2 µs (59%)** | 2.9 µs (62%) |
| Order book snapshot — 100 levels | 1.9 µs | **1.1 µs** |
| Limit insertion — 10 orders | **1.2 µs** | — |
| Throughput — 1 thread | **411k orders/sec** | 540k orders/sec |
| Throughput — 8 threads | 78.7k orders/sec† | **170k orders/sec** |
| Cancel order (O(n) in book depth) | 1.1 µs @ 10 · 34.7 µs @ 500 | 0.96 µs @ 10 · 25.7 µs @ 500 |

> †8-thread CI number reflects 8 threads on 2 vCPUs (heavy context-switch overhead), not a throughput ceiling.
> SHA-256 accounts for ~60% of match latency — the primary optimization target if throughput becomes a bottleneck.

**End-to-end (HTTP round-trip)** — measured with [k6](https://k6.io), 50 VU steady-state / 100 VU spike, 74k requests:

| p50 | p90 | p95 | p99 | Peak throughput | 5xx rate |
|-----|-----|-----|-----|-----------------|----------|
| **4.2 ms** | 31.1 ms | 42.7 ms | **57.8 ms** | **353 req/s** | **0.00%** |

> p50/p90 gap reflects a bimodal distribution: fast-path order rejections (validation only, ~1ms) vs. full DB-write orders (~30ms).
> In-process matching (µs above) and HTTP round-trip (ms here) measure different things — both matter.

```bash
# In-process matching latency (Google Benchmark)
bash backend/benchmarks/run_benchmarks.sh

# HTTP round-trip latency (k6, requires running server)
bash backend/load_tests/run_load_test.sh
```

Raw results: [`backend/benchmarks/results/`](backend/benchmarks/results/) · [`backend/load_tests/results/`](backend/load_tests/results/)

---

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                      React / TypeScript                      │
│         Trading · Portfolio · Dashboard · Auth              │
└────────────────────────┬──────────────┬─────────────────────┘
                         │ REST         │ WebSocket
                         ▼              ▼
┌─────────────────────────────────────────────────────────────┐
│                   Boost.Beast (C++17)                        │
│              HTTP Server · WebSocket Server                  │
└────────────────────────┬────────────────────────────────────┘
                         │
                         ▼
┌─────────────────────────────────────────────────────────────┐
│                      API Router                              │
│   Auth · Orders · Cards · Trades · Market · Candles         │
└──────┬──────────────────────────────────┬───────────────────┘
       │                                  │
       ▼                                  ▼
┌─────────────┐                  ┌────────────────┐
│ OrderService│                  │  TradeService  │
│             │                  │                │
│  per-card   │                  │  PostgreSQL    │
│  OrderBook  │──── trades ─────▶│  transaction   │
│  (shared_   │                  │  (atomic gold  │
│   mutex)    │                  │  + inventory)  │
└─────────────┘                  └───────┬────────┘
       │                                 │
       │ snapshot                        │ trade event
       ▼                                 ▼
┌──────────────┐              ┌──────────────────────┐
│PriceAgg      │              │  EventBroadcaster    │
│Service       │              │                      │
│(OHLCV candle │              │  trades:{card_id}    │
│ generation)  │              │  orderbook:{card_id} │
└──────┬───────┘              │  user:{user_id}      │
       │                      └──────────────────────┘
       ▼
┌─────────────┐
│  PostgreSQL │
│  candle_data│
└─────────────┘
```

**Data flow for an order placement:**
1. JWT-authenticated POST to `/api/v1/orders`
2. `OrderService` validates balance/inventory, writes order to PostgreSQL
3. `OrderBook::match_order` acquires `unique_lock`, runs price-time priority matching
4. Each fill generates a `Trade` with a SHA-256 Merkle hash
5. `TradeService` settles atomically: gold debit + credit + inventory transfer in one transaction
6. `PriceAggregationService` updates OHLCV candle buffer for the traded card
7. `EventBroadcaster` pushes order book snapshot and trade event to WebSocket subscribers

---

## Key Technical Decisions

**Price-time priority matching**
Bids stored as `std::map<double, std::deque<Order>, std::greater<>>` (descending), asks as ascending. `map::begin()` gives O(1) best-price access; `deque::front()`/`pop_front()` gives O(1) time-priority within a price level. Match loop is O(k) in number of fills.

**Reader-writer locking (`shared_mutex`)**
`get_snapshot()` — called on every WebSocket broadcast — acquires a `shared_lock`, allowing concurrent reads. Only `match_order` and `cancel_order` take `unique_lock`. In a read-heavy market-data workload this outperforms a plain mutex. Benchmarks show the tradeoff: single-threaded throughput is ~411k orders/sec on the CI runner; at 8 concurrent writers on 2 vCPUs it drops to ~79k due to `unique_lock` contention and context-switch overhead.

**SHA-256 Merkle hash per trade**
Every trade record carries a SHA-256 hash of its fields (trade ID, buyer, seller, card, price, quantity, both order IDs). Any post-settlement mutation of a trade record invalidates the hash — verifiable with `Trade::verify_integrity()`. The hash is computed inside the write lock (2.9 µs, 62% of match latency); moving it outside is the identified optimization path.

**Atomic trade settlement**
Three writes — record trade, transfer gold, transfer inventory — execute inside a single `pqxx::work` transaction. RAII commit/rollback ensures no partial state: a buyer's gold is never debited without the card being credited.

**OHLCV candle generation with period-boundary alignment**
Trade timestamps are floored to period boundaries (`timestamp_ms - timestamp_ms % period_ms`) rather than elapsed-time tracking. All cards' 1-minute candles start at :00, :01, :02... regardless of when the first trade in a period arrives. Candles are joinable across cards by timestamp.

**In-process WebSocket broadcast**
`EventBroadcaster` iterates subscribers directly on the settling thread — no serialization, no Redis round-trip. Lower broadcast latency than a pub/sub intermediary at the cost of single-process deployment. Redis pub/sub is the horizontal scaling path.

---

## Quick Start

**Prerequisites**: Docker, CMake 3.20+, vcpkg

```bash
# 1. Start PostgreSQL + Redis
docker-compose up -d

# 2. Build backend (Release)
cd backend
cmake -B build -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_TOOLCHAIN_FILE=vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build -j$(nproc)

# 3. Run server (from backend/)
./build/bin/clash_trading

# 4. Start frontend
cd ../frontend
npm install && npm run dev
```

Frontend: http://localhost:5173  
API: http://localhost:8080

---

## Running Benchmarks

```bash
bash backend/benchmarks/run_benchmarks.sh
```

Builds in Release mode, runs 3 repetitions per benchmark, saves timestamped JSON to `backend/benchmarks/results/`, and prints headline numbers.

To run a specific benchmark:
```bash
./backend/build/bin/order_book_bench --benchmark_filter="BM_MarketOrder"
```

---

## Running Tests

```bash
cd backend/build && ctest --output-on-failure
```

Test coverage: OrderBook matching (exact fill, partial fill, multi-fill, no-match, concurrent placement), TradeService settlement, UserService portfolio, AuthService JWT, PostgreSQL client.

---

## Stack

| Layer | Technology |
|-------|-----------|
| Matching engine | C++17, `std::map`, `std::shared_mutex` |
| HTTP / WebSocket | Boost.Beast 1.85, Boost.ASIO |
| Database | PostgreSQL 15, libpqxx |
| Cache / Infra | Redis 7, Docker Compose |
| Serialization | nlohmann/json |
| Auth | JWT (HMAC-SHA256), bcrypt |
| Benchmarks | Google Benchmark |
| Tests | Google Test |
| Frontend | React 19, TypeScript, Vite, Tailwind CSS |
| Charts | lightweight-charts (candlesticks), Recharts |
| State | Zustand, TanStack Query |

---

## Project Structure

```
clash-royale-trading/
├── backend/
│   ├── src/
│   │   ├── core/           # OrderBook matching engine
│   │   ├── services/       # Order, Trade, User, PriceAgg, CardSync
│   │   ├── api/            # HTTP server, WebSocket, routes, auth
│   │   ├── database/       # PostgreSQL client
│   │   ├── external/       # Clash Royale API client
│   │   └── utils/          # Config, logger
│   ├── include/            # Headers
│   ├── tests/unit/         # Google Test suites
│   ├── benchmarks/         # Google Benchmark harness + results
│   └── CMakeLists.txt
├── frontend/
│   └── src/
│       ├── pages/          # Trading, Dashboard, Portfolio, Auth
│       ├── components/     # CandlestickChart, ProtectedRoute
│       ├── contexts/       # AuthContext, WebSocketContext
│       └── services/       # Typed API client
├── docker-compose.yml
└── README.md
```
