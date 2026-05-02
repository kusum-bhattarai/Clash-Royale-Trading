#pragma once

#include "database/postgres_client.hpp"
#include <memory>
#include <string>

namespace clash_trading {
namespace services {

struct PriceFactors {
    double base;         // rarity-tier floor
    double sd;           // supply/demand multiplier [0.5, 2.0]
    double meta;         // meta-relevance multiplier [0.5, 2.0]
    double vol_discount; // volatility discount [0.7, 1.0]
};

struct ReferencePrice {
    double reference_price;
    PriceFactors factors;
};

class DynamicPricingService {
public:
    explicit DynamicPricingService(std::shared_ptr<database::PostgresClient> db);

    // Compute multi-factor reference price for a card.
    // Formula: base × sd_factor × meta_factor × vol_discount
    ReferencePrice compute_reference_price(const std::string& card_id) const;

private:
    double rarity_base_price(const std::string& rarity) const;
    double supply_demand_factor(const std::string& card_id) const;
    double meta_relevance_factor(const std::string& card_id) const;
    double volatility_discount(const std::string& card_id) const;

    std::shared_ptr<database::PostgresClient> db_;
};

} // namespace services
} // namespace clash_trading
