#pragma once

#include "core/candle.hpp"
#include "database/postgres_client.hpp"
#include "services/order_service.hpp"
#include "services/price_aggregation_service.hpp"
#include <memory>
#include <string>

namespace clash_trading {
namespace services {

struct SpreadMetrics {
    double instantaneous = 0.0;  // best_ask - best_bid from live order book
    double twas_1h = 0.0;        // proxy: avg candle H-L range over last 60 1m candles
    double relative_pct = 0.0;   // instantaneous / mid_price * 100
};

struct OFIMetrics {
    // (buy_qty - sell_qty) / (buy_qty + sell_qty) — range [-1, 1]
    // positive = net buy pressure, negative = net sell pressure
    double window_1m = 0.0;
    double window_5m = 0.0;
};

struct AnalyticsSnapshot {
    SpreadMetrics spread;
    OFIMetrics order_flow_imbalance;
    double price_impact_bps = 0.0;  // avg |Δclose| / close * 10_000 over 20 1m candles
    double volatility_1m = 0.0;     // stddev(log_returns) over trailing 20 1m candles
    double volatility_1h = 0.0;     // stddev(log_returns) over trailing 20 1h candles
    double vwap = 0.0;              // session VWAP, resets at UTC midnight
};

class AnalyticsService {
public:
    AnalyticsService(
        std::shared_ptr<database::PostgresClient> db,
        std::shared_ptr<OrderService> order_service,
        std::shared_ptr<PriceAggregationService> price_agg_service
    );

    AnalyticsSnapshot get_analytics(const std::string& card_id) const;

private:
    SpreadMetrics compute_spread(const std::string& card_id) const;
    OFIMetrics compute_ofi(const std::string& card_id) const;
    double compute_price_impact_bps(const std::string& card_id) const;
    double compute_volatility(const std::string& card_id, trading::Timeframe tf) const;
    double compute_vwap(const std::string& card_id) const;

    std::shared_ptr<database::PostgresClient> db_;
    std::shared_ptr<OrderService> order_service_;
    std::shared_ptr<PriceAggregationService> price_agg_service_;
};

} // namespace services
} // namespace clash_trading
