#include "services/dynamic_pricing_service.hpp"
#include "utils/logger.hpp"
#include <cmath>
#include <algorithm>
#include <numeric>

namespace clash_trading {
namespace services {

DynamicPricingService::DynamicPricingService(
    std::shared_ptr<database::PostgresClient> db)
    : db_(db) {}

ReferencePrice DynamicPricingService::compute_reference_price(
    const std::string& card_id) const
{
    PriceFactors f;
    f.base         = rarity_base_price(card_id);
    f.sd           = supply_demand_factor(card_id);
    f.meta         = meta_relevance_factor(card_id);
    f.vol_discount = volatility_discount(card_id);

    double ref = f.base * f.sd * f.meta * f.vol_discount;
    LOG_DEBUG("[PRICE] card={} base={} sd={} meta={} vol_discount={} ref={}",
              card_id, f.base, f.sd, f.meta, f.vol_discount, ref);

    return ReferencePrice{ref, f};
}

double DynamicPricingService::rarity_base_price(const std::string& card_id) const {
    auto result = db_->with_transaction([&](pqxx::work& txn) {
        return txn.exec_params(
            "SELECT rarity FROM cards WHERE card_id = $1",
            card_id
        );
    });

    if (result.empty()) return 800.0;  // fallback: Epic floor

    std::string rarity = result[0][0].as<std::string>();
    if (rarity == "Common")    return 50.0;
    if (rarity == "Rare")      return 200.0;
    if (rarity == "Epic")      return 800.0;
    if (rarity == "Legendary") return 3000.0;
    if (rarity == "Champion")  return 8000.0;
    return 800.0;
}

// sd = clamp(24h_buy_volume / 24h_sell_volume, 0.5, 2.0)
// No sell volume → strong buy pressure → 2.0
// No volume at all → neutral → 1.0
double DynamicPricingService::supply_demand_factor(const std::string& card_id) const {
    auto result = db_->with_transaction([&](pqxx::work& txn) {
        return txn.exec_params(
            "SELECT "
            "  COALESCE(SUM(CASE WHEN order_type = 'BUY'  THEN quantity ELSE 0 END), 0) AS buy_vol, "
            "  COALESCE(SUM(CASE WHEN order_type = 'SELL' THEN quantity ELSE 0 END), 0) AS sell_vol "
            "FROM orders "
            "WHERE card_id = $1 AND created_at > NOW() - INTERVAL '24 hours'",
            card_id
        );
    });

    if (result.empty()) return 1.0;

    double buy_vol  = result[0]["buy_vol"].as<double>();
    double sell_vol = result[0]["sell_vol"].as<double>();

    if (buy_vol == 0.0 && sell_vol == 0.0) return 1.0;
    if (sell_vol == 0.0) return 2.0;

    double ratio = buy_vol / sell_vol;
    return std::clamp(ratio, 0.5, 2.0);
}

// meta = 0.5 + usage_rate * 1.5  (usage_rate stored as 0–1 decimal)
// No usage data → neutral → 1.0
double DynamicPricingService::meta_relevance_factor(const std::string& card_id) const {
    auto result = db_->with_transaction([&](pqxx::work& txn) {
        return txn.exec_params(
            "SELECT usage_rate FROM cards WHERE card_id = $1",
            card_id
        );
    });

    if (result.empty() || result[0][0].is_null()) return 1.0;

    double usage_rate = result[0][0].as<double>();
    if (usage_rate <= 0.0) return 1.0;

    double factor = 0.5 + usage_rate * 1.5;
    return std::clamp(factor, 0.5, 2.0);
}

// vol_discount = 1.0 - clamp(σ * 0.5, 0.0, 0.3)
// σ = stddev(log_returns) over 24h trades
// Fewer than 2 trades → no discount → 1.0
double DynamicPricingService::volatility_discount(const std::string& card_id) const {
    auto result = db_->with_transaction([&](pqxx::work& txn) {
        return txn.exec_params(
            "SELECT price::float8 FROM trades "
            "WHERE card_id = $1 AND executed_at > NOW() - INTERVAL '24 hours' "
            "ORDER BY executed_at",
            card_id
        );
    });

    if (result.size() < 2) return 1.0;

    std::vector<double> prices;
    prices.reserve(result.size());
    for (const auto& row : result) {
        prices.push_back(row[0].as<double>());
    }

    std::vector<double> log_returns;
    log_returns.reserve(prices.size() - 1);
    for (size_t i = 1; i < prices.size(); ++i) {
        if (prices[i - 1] > 0.0) {
            log_returns.push_back(std::log(prices[i] / prices[i - 1]));
        }
    }

    if (log_returns.empty()) return 1.0;

    double mean = std::accumulate(log_returns.begin(), log_returns.end(), 0.0)
                  / static_cast<double>(log_returns.size());
    double sq_sum = 0.0;
    for (double r : log_returns) {
        sq_sum += (r - mean) * (r - mean);
    }
    double sigma = std::sqrt(sq_sum / static_cast<double>(log_returns.size()));

    double discount = 1.0 - std::clamp(sigma * 0.5, 0.0, 0.3);
    return discount;
}

} // namespace services
} // namespace clash_trading
