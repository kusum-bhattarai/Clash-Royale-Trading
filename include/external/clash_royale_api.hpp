#pragma once

#include <string>
#include <vector>
#include <optional>
#include <nlohmann/json.hpp>

namespace clash_trading {
namespace external {

struct CardData {
    std::string id;
    std::string name;
    std::string rarity;
    int max_level;
    std::string icon_url;
    std::optional<int> elixir_cost;
};

class ClashRoyaleAPI {
private:
    std::string base_url_;
    std::string api_key_;
    int rate_limit_per_second_;
    int timeout_seconds_;
    
public:
    ClashRoyaleAPI(const std::string& base_url, 
                   const std::string& api_key,
                   int rate_limit = 8,
                   int timeout_seconds = 10);
    
    std::vector<CardData> fetch_all_cards();
    std::optional<CardData> fetch_card(const std::string& card_id);
    
private:
    std::string make_request(const std::string& endpoint);
    CardData parse_card(const nlohmann::json& json);
};

} // namespace external
} // namespace clash_trading