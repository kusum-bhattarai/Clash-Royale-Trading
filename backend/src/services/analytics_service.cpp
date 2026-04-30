#include "services/analytics_service.hpp"
#include <cmath>
#include <numeric>

namespace clash_trading {
namespace services {

AnalyticsService::AnalyticsService(
    std::shared_ptr<database::PostgresClient> db,
    std::shared_ptr<OrderService> order_service,
    std::shared_ptr<PriceAggregationService> price_agg_service
)
    : db_(std::move(db))
    , order_service_(std::move(order_service))
    , price_agg_service_(std::move(price_agg_service))
{}

AnalyticsSnapshot AnalyticsService::get_analytics(const std::string& card_id) const {
    AnalyticsSnapshot snap;
    snap.spread               = compute_spread(card_id);
    snap.order_flow_imbalance = compute_ofi(card_id);
    snap.price_impact_bps     = compute_price_impact_bps(card_id);
    snap.volatility_1m        = compute_volatility(card_id, trading::Timeframe::ONE_MINUTE);
    snap.volatility_1h        = compute_volatility(card_id, trading::Timeframe::ONE_HOUR);
    snap.vwap                 = compute_vwap(card_id);
    return snap;
}

// ── Spread ────────────────────────────────────────────────────────────────────

SpreadMetrics AnalyticsService::compute_spread(const std::string& card_id) const {
    SpreadMetrics result;

    auto snapshot = order_service_->get_order_book_snapshot(card_id);
    if (!snapshot.bids.empty() && !snapshot.asks.empty()) {
        double best_bid = snapshot.bids.front().first;
        double best_ask = snapshot.asks.front().first;
        double mid = (best_bid + best_ask) / 2.0;

        result.instantaneous = best_ask - best_bid;
        if (mid > 0.0)
            result.relative_pct = (result.instantaneous / mid) * 100.0;
    }

    // TWAS proxy: avg candle H-L range over last 60 1m candles.
    // True TWAS requires stored bid/ask tick history; H-L range is a standard proxy.
    auto candles = price_agg_service_->get_candles(card_id, trading::Timeframe::ONE_MINUTE, 60);
    if (!candles.empty()) {
        double sum_hl = 0.0;
        for (const auto& c : candles)
            sum_hl += (c.high_price - c.low_price);
        result.twas_1h = sum_hl / static_cast<double>(candles.size());
    }

    return result;
}

// ── Order Flow Imbalance ──────────────────────────────────────────────────────

OFIMetrics AnalyticsService::compute_ofi(const std::string& card_id) const {
    OFIMetrics result;

    // Hard-coded queries avoid string concatenation while keeping window sizes clear.
    static const char* const kQuery1m =
        "SELECT "
        "  COALESCE(SUM(CASE WHEN order_type = 'BUY'  THEN quantity ELSE 0 END), 0) AS buy_vol, "
        "  COALESCE(SUM(CASE WHEN order_type = 'SELL' THEN quantity ELSE 0 END), 0) AS sell_vol "
        "FROM orders "
        "WHERE card_id = $1 AND created_at >= NOW() - INTERVAL '1 minute'";

    static const char* const kQuery5m =
        "SELECT "
        "  COALESCE(SUM(CASE WHEN order_type = 'BUY'  THEN quantity ELSE 0 END), 0) AS buy_vol, "
        "  COALESCE(SUM(CASE WHEN order_type = 'SELL' THEN quantity ELSE 0 END), 0) AS sell_vol "
        "FROM orders "
        "WHERE card_id = $1 AND created_at >= NOW() - INTERVAL '5 minutes'";

    auto compute = [&](const char* query) -> double {
        auto rows = db_->with_transaction([&](pqxx::work& txn) {
            return txn.exec_params(query, card_id);
        });
        if (rows.empty()) return 0.0;
        double buy_vol  = rows[0]["buy_vol"].as<double>();
        double sell_vol = rows[0]["sell_vol"].as<double>();
        double total    = buy_vol + sell_vol;
        return (total > 0.0) ? (buy_vol - sell_vol) / total : 0.0;
    };

    result.window_1m = compute(kQuery1m);
    result.window_5m = compute(kQuery5m);
    return result;
}

// ── Price Impact ──────────────────────────────────────────────────────────────

double AnalyticsService::compute_price_impact_bps(const std::string& card_id) const {
    // avg |Δclose| / prev_close * 10_000 over consecutive 1m candles.
    // Proxy for typical per-period price displacement from liquidity consumption.
    // get_candles returns newest-first; candles[0] is most recent.
    auto candles = price_agg_service_->get_candles(card_id, trading::Timeframe::ONE_MINUTE, 21);
    if (candles.size() < 2) return 0.0;

    double sum_bps = 0.0;
    int count = 0;
    for (size_t i = 0; i + 1 < candles.size(); ++i) {
        double curr = candles[i].close_price;
        double prev = candles[i + 1].close_price;
        if (prev > 0.0) {
            sum_bps += std::abs(curr - prev) / prev * 10000.0;
            ++count;
        }
    }
    return (count > 0) ? sum_bps / count : 0.0;
}

// ── Rolling Volatility ────────────────────────────────────────────────────────

double AnalyticsService::compute_volatility(const std::string& card_id, trading::Timeframe tf) const {
    // Population stddev of log returns over trailing 20 candles.
    // get_candles returns newest-first; log_return[i] = ln(candles[i] / candles[i+1]).
    auto candles = price_agg_service_->get_candles(card_id, tf, 21);
    if (candles.size() < 2) return 0.0;

    std::vector<double> log_returns;
    log_returns.reserve(candles.size() - 1);
    for (size_t i = 0; i + 1 < candles.size(); ++i) {
        double curr = candles[i].close_price;
        double prev = candles[i + 1].close_price;
        if (prev > 0.0 && curr > 0.0)
            log_returns.push_back(std::log(curr / prev));
    }
    if (log_returns.empty()) return 0.0;

    double mean = std::accumulate(log_returns.begin(), log_returns.end(), 0.0)
                  / static_cast<double>(log_returns.size());
    double variance = 0.0;
    for (double r : log_returns)
        variance += (r - mean) * (r - mean);
    variance /= static_cast<double>(log_returns.size());
    return std::sqrt(variance);
}

// ── VWAP ──────────────────────────────────────────────────────────────────────

double AnalyticsService::compute_vwap(const std::string& card_id) const {
    // Session VWAP: Σ(price * qty) / Σ(qty) for all trades since UTC midnight.
    auto rows = db_->with_transaction([&](pqxx::work& txn) {
        return txn.exec_params(
            "SELECT "
            "  COALESCE(SUM(price * quantity), 0) AS pv, "
            "  COALESCE(SUM(quantity), 0)          AS vol "
            "FROM trades "
            "WHERE card_id = $1 "
            "  AND executed_at >= CURRENT_DATE::timestamp",
            card_id
        );
    });
    if (rows.empty()) return 0.0;
    double pv  = rows[0]["pv"].as<double>();
    double vol = rows[0]["vol"].as<double>();
    return (vol > 0.0) ? pv / vol : 0.0;
}

} // namespace services
} // namespace clash_trading
