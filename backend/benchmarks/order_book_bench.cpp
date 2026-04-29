#include <benchmark/benchmark.h>
#include "core/order_book.hpp"
#include "core/order.hpp"
#include <atomic>
#include <thread>
#include <vector>
#include <string>

using namespace clash_trading::core;

// ─── Helpers ────────────────────────────────────────────────────────────────

static std::atomic<int> order_counter{0};

static Order make_limit_order(OrderType type, double price, int qty = 1) {
    Order o;
    o.order_id = "ord_" + std::to_string(order_counter.fetch_add(1));
    o.user_id  = (type == OrderType::BUY) ? "buyer_1" : "seller_1";
    o.card_id  = "card_1";
    o.type     = type;
    o.mode     = OrderMode::LIMIT;
    o.price    = price;
    o.quantity = qty;
    o.status   = OrderStatus::PENDING;
    return o;
}

static Order make_market_order(OrderType type, int qty = 1) {
    Order o;
    o.order_id = "ord_" + std::to_string(order_counter.fetch_add(1));
    o.user_id  = (type == OrderType::BUY) ? "buyer_1" : "seller_1";
    o.card_id  = "card_1";
    o.type     = type;
    o.mode     = OrderMode::MARKET;
    o.price    = 0.0;
    o.quantity = qty;
    o.status   = OrderStatus::PENDING;
    return o;
}

// Pre-populate a book with `depth` bid levels and `depth` ask levels.
// Bid prices: [mid - depth, mid - 1], Ask prices: [mid + 1, mid + depth]
// Each level has `qty_per_level` quantity.
static void populate_book(OrderBook& book, int depth, int qty_per_level = 10) {
    const double mid = 1000.0;
    for (int i = 1; i <= depth; ++i) {
        auto bid = make_limit_order(OrderType::BUY,  mid - i, qty_per_level);
        book.match_order(bid);
        auto ask = make_limit_order(OrderType::SELL, mid + i, qty_per_level);
        book.match_order(ask);
    }
}

// ─── Benchmark 1: Limit order insertion (no match) ──────────────────────────
// Measures the pure cost of inserting a resting limit order — map insert +
// deque push_back under unique_lock. This is the write-path baseline.
static void BM_LimitOrderInsertion(benchmark::State& state) {
    const int depth = static_cast<int>(state.range(0));

    for (auto _ : state) {
        state.PauseTiming();
        OrderBook book("card_bench");
        // Fill ask side only so bid inserts never match
        for (int i = 1; i <= depth; ++i) {
            auto ask = make_limit_order(OrderType::SELL, 1000.0 + i, 100);
            book.match_order(ask);
        }
        auto order = make_limit_order(OrderType::BUY, 990.0, 1); // below all asks
        state.ResumeTiming();

        book.match_order(order);
        benchmark::DoNotOptimize(order);
    }

    state.SetItemsProcessed(state.iterations());
    state.SetLabel("book_depth=" + std::to_string(depth));
}
BENCHMARK(BM_LimitOrderInsertion)
    ->Arg(10)->Arg(100)->Arg(1000)
    ->Unit(benchmark::kNanosecond);

// ─── Benchmark 2: Market order — single fill ────────────────────────────────
// One market BUY against exactly one resting SELL. Measures the hot path:
// lock → best-price lookup → trade creation (including SHA-256 Merkle hash) → erase.
static void BM_MarketOrderSingleFill(benchmark::State& state) {
    for (auto _ : state) {
        state.PauseTiming();
        OrderBook book("card_bench");
        auto ask = make_limit_order(OrderType::SELL, 1000.0, 10);
        book.match_order(ask);
        auto buy = make_market_order(OrderType::BUY, 1);
        state.ResumeTiming();

        auto trades = book.match_order(buy);
        benchmark::DoNotOptimize(trades);
    }

    state.SetItemsProcessed(state.iterations());
    state.SetLabel("fills=1");
}
BENCHMARK(BM_MarketOrderSingleFill)->Unit(benchmark::kNanosecond);

// ─── Benchmark 3: Market order — sweep N price levels ───────────────────────
// A single large market BUY sweeps across `depth` ask price levels.
// Isolates how matching latency grows with fills: O(k) where k = levels swept.
static void BM_MarketOrderSweep(benchmark::State& state) {
    const int depth = static_cast<int>(state.range(0));

    for (auto _ : state) {
        state.PauseTiming();
        OrderBook book("card_bench");
        for (int i = 1; i <= depth; ++i) {
            auto ask = make_limit_order(OrderType::SELL, 1000.0 + i, 1);
            book.match_order(ask);
        }
        // One market order that sweeps all levels (depth total quantity)
        auto buy = make_market_order(OrderType::BUY, depth);
        state.ResumeTiming();

        auto trades = book.match_order(buy);
        benchmark::DoNotOptimize(trades);
    }

    state.SetItemsProcessed(state.iterations());
    state.SetLabel("levels_swept=" + std::to_string(depth));
}
BENCHMARK(BM_MarketOrderSweep)
    ->Arg(1)->Arg(10)->Arg(50)->Arg(100)
    ->Unit(benchmark::kNanosecond);

// ─── Benchmark 4: Limit order partial fill ──────────────────────────────────
// Incoming BUY limit matches partway through the best ask level, then rests.
// Tests the partial-fill branch and the subsequent add_order call.
static void BM_LimitOrderPartialFill(benchmark::State& state) {
    for (auto _ : state) {
        state.PauseTiming();
        OrderBook book("card_bench");
        auto ask = make_limit_order(OrderType::SELL, 1000.0, 10); // qty=10
        book.match_order(ask);
        auto buy = make_limit_order(OrderType::BUY, 1000.0, 5);   // fills 5, rests 5
        state.ResumeTiming();

        auto trades = book.match_order(buy);
        benchmark::DoNotOptimize(trades);
    }

    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_LimitOrderPartialFill)->Unit(benchmark::kNanosecond);

// ─── Benchmark 5: Order cancellation ────────────────────────────────────────
// Linear scan across price levels to find and remove an order by ID.
// Cancel is O(n) in total order count; this confirms the degradation curve.
static void BM_CancelOrder(benchmark::State& state) {
    const int depth = static_cast<int>(state.range(0));

    for (auto _ : state) {
        state.PauseTiming();
        OrderBook book("card_bench");
        std::string target_id;
        for (int i = 0; i < depth; ++i) {
            auto bid = make_limit_order(OrderType::BUY, 900.0 + i, 1);
            if (i == depth / 2) target_id = bid.order_id; // cancel from middle
            book.match_order(bid);
        }
        state.ResumeTiming();

        bool cancelled = book.cancel_order(target_id);
        benchmark::DoNotOptimize(cancelled);
    }

    state.SetItemsProcessed(state.iterations());
    state.SetLabel("orders=" + std::to_string(depth));
}
BENCHMARK(BM_CancelOrder)
    ->Arg(10)->Arg(100)->Arg(500)
    ->Unit(benchmark::kNanosecond);

// ─── Benchmark 6: Order book snapshot ───────────────────────────────────────
// get_snapshot() acquires a shared_lock and iterates all levels.
// Used by the WebSocket broadcast path on every trade event.
static void BM_GetSnapshot(benchmark::State& state) {
    const int depth = static_cast<int>(state.range(0));

    OrderBook book("card_bench");
    populate_book(book, depth);

    for (auto _ : state) {
        auto snap = book.get_snapshot();
        benchmark::DoNotOptimize(snap);
    }

    state.SetItemsProcessed(state.iterations());
    state.SetLabel("levels=" + std::to_string(depth));
}
BENCHMARK(BM_GetSnapshot)
    ->Arg(10)->Arg(100)->Arg(1000)
    ->Unit(benchmark::kNanosecond);

// ─── Benchmark 7: Concurrent order placement ────────────────────────────────
// N threads each place limit orders on the same book simultaneously.
// Measures throughput (orders/sec) under lock contention.
// Expected: throughput plateaus or drops as threads increase due to unique_lock.
static void BM_ConcurrentOrders(benchmark::State& state) {
    const int num_threads = static_cast<int>(state.range(0));

    for (auto _ : state) {
        state.PauseTiming();
        OrderBook book("card_bench");
        std::vector<std::thread> threads;
        threads.reserve(num_threads);
        std::atomic<int> ready{0};
        std::atomic<bool> go{false};
        state.ResumeTiming();

        for (int t = 0; t < num_threads; ++t) {
            threads.emplace_back([&, t]() {
                ready.fetch_add(1);
                while (!go.load()) {}  // spin-wait for all threads ready
                for (int i = 0; i < 100; ++i) {
                    // Interleave buys and sells so some trades happen
                    auto order = (i % 2 == 0)
                        ? make_limit_order(OrderType::BUY,  1000.0 + (i % 5), 1)
                        : make_limit_order(OrderType::SELL, 1000.0 + (i % 5), 1);
                    benchmark::DoNotOptimize(book.match_order(order));
                }
            });
        }

        while (ready.load() < num_threads) {}
        go.store(true);
        for (auto& th : threads) th.join();
    }

    // Each iteration = num_threads * 100 orders
    state.SetItemsProcessed(state.iterations() * num_threads * 100);
    state.SetLabel("threads=" + std::to_string(num_threads));
}
BENCHMARK(BM_ConcurrentOrders)
    ->Arg(1)->Arg(2)->Arg(4)->Arg(8)
    ->Unit(benchmark::kMillisecond)
    ->MeasureProcessCPUTime()
    ->UseRealTime();

// ─── Benchmark 8: Merkle hash computation ───────────────────────────────────
// SHA-256 runs on every trade. Isolate its cost so we know what fraction of
// match latency comes from hashing vs book manipulation.
static void BM_MerkleHash(benchmark::State& state) {
    Trade t;
    t.trade_id       = "trade_abc123def456";
    t.buyer_id       = "user_00000001";
    t.seller_id      = "user_00000002";
    t.card_id        = "card_mega_knight";
    t.price          = 1580.25;
    t.quantity       = 3;
    t.buyer_order_id  = "ord_buy_9999";
    t.seller_order_id = "ord_sell_8888";

    for (auto _ : state) {
        auto hash = t.calculate_merkle_hash();
        benchmark::DoNotOptimize(hash);
    }

    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_MerkleHash)->Unit(benchmark::kNanosecond);

BENCHMARK_MAIN();
