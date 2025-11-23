#pragma once

#include <memory>
#include <vector>
#include "database/postgres_client.hpp"
#include "external/clash_royale_api.hpp"

namespace clash_trading {
namespace services {

class CardSyncService {
private:
    std::shared_ptr<database::PostgresClient> db_;
    std::shared_ptr<external::ClashRoyaleAPI> api_;
    
public:
    CardSyncService(std::shared_ptr<database::PostgresClient> db,
                    std::shared_ptr<external::ClashRoyaleAPI> api);
    
    // Sync all cards from API to database
    void sync_all_cards();
    
    // Get card count from database
    int get_card_count();
    
private:
    void insert_or_update_card(const external::CardData& card);
    double get_initial_price(const std::string& rarity);
};

} // namespace services
} // namespace clash_trading