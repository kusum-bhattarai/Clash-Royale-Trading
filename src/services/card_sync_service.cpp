#include "services/card_sync_service.hpp"
#include <fmt/core.h>
#include <type_traits>

namespace clash_trading {
namespace services {

CardSyncService::CardSyncService(
    std::shared_ptr<database::PostgresClient> db,
    std::shared_ptr<external::ClashRoyaleAPI> api)
    : db_(db), api_(api) {}

void CardSyncService::sync_all_cards() {
    fmt::print("[SYNC] Starting card synchronization from Clash Royale API\n");
    
    try {
        auto cards = api_->fetch_all_cards();
        
        fmt::print("[SYNC] Inserting {} cards into database\n", cards.size());
        
        int inserted = 0;
        
        for (const auto& card : cards) {
            try {
                insert_or_update_card(card);
                inserted++;
            } catch (const std::exception& e) {
                fmt::print(stderr, "[WARN] Failed to sync card {}: {}\n", 
                          card.name, e.what());
            }
        }
        
        fmt::print("[SYNC] Complete: {} cards in database\n", inserted);
        
    } catch (const std::exception& e) {
        fmt::print(stderr, "[ERROR] Card sync failed: {}\n", e.what());
        throw;
    }
}

int CardSyncService::get_card_count() {
    return db_->with_transaction([](pqxx::work& txn) {
        auto result = txn.exec("SELECT COUNT(*) FROM cards");
        return result[0][0].as<int>();
    });
}

void CardSyncService::insert_or_update_card(const external::CardData& card) {
    db_->with_transaction([&](pqxx::work& txn) -> void {
        double initial_price = get_initial_price(card.rarity);
        
        // Handle elixir cost - use NULL if not present
        if (card.elixir_cost.has_value()) {
            txn.exec(
                R"(
                    INSERT INTO cards (card_id, name, rarity, max_level, icon_url, 
                                     elixir_cost, current_market_price, total_supply, 
                                     usage_rate, last_synced)
                    VALUES ($1, $2, $3, $4, $5, $6, $7, 0, 0.0, NOW())
                    ON CONFLICT (card_id) 
                    DO UPDATE SET
                        name = EXCLUDED.name,
                        rarity = EXCLUDED.rarity,
                        max_level = EXCLUDED.max_level,
                        icon_url = EXCLUDED.icon_url,
                        elixir_cost = EXCLUDED.elixir_cost,
                        last_synced = NOW()
                )",
                pqxx::params(
                    card.id, card.name, card.rarity, card.max_level,
                    card.icon_url, std::to_string(card.elixir_cost.value()),
                    initial_price
                )
            );
        } else {
            txn.exec(
                R"(
                    INSERT INTO cards (card_id, name, rarity, max_level, icon_url, 
                                     elixir_cost, current_market_price, total_supply, 
                                     usage_rate, last_synced)
                    VALUES ($1, $2, $3, $4, $5, NULL, $6, 0, 0.0, NOW())
                    ON CONFLICT (card_id) 
                    DO UPDATE SET
                        name = EXCLUDED.name,
                        rarity = EXCLUDED.rarity,
                        max_level = EXCLUDED.max_level,
                        icon_url = EXCLUDED.icon_url,
                        elixir_cost = NULL,
                        last_synced = NOW()
                )",
                pqxx::params(
                    card.id, card.name, card.rarity, card.max_level,
                    card.icon_url, initial_price
                )
            );
        }
    });
}

double CardSyncService::get_initial_price(const std::string& rarity) {
    if (rarity == "Common") return 100.0;
    if (rarity == "Rare") return 500.0;
    if (rarity == "Epic") return 2000.0;
    if (rarity == "Legendary") return 10000.0;
    if (rarity == "Champion") return 25000.0;
    return 100.0;
}

} // namespace services
} // namespace clash_trading