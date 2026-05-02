#include "services/trade_service.hpp"
#include "utils/logger.hpp"
#include <sstream>
#include <chrono>

namespace clash_trading {
namespace services {

TradeService::TradeService(
    std::shared_ptr<database::PostgresClient> db,
    std::shared_ptr<PriceAggregationService> price_agg_service
) 
    : db_(db)
    , price_agg_service_(price_agg_service) {}

void TradeService::execute_trade(const core::Trade& trade) {
    try {
        auto t0 = std::chrono::steady_clock::now();
        db_->with_transaction([&](pqxx::work& txn) {
            int64_t buyer_cost = static_cast<int64_t>(trade.total_value);
            update_balance(txn, trade.buyer_id, -buyer_cost);
            update_balance(txn, trade.seller_id, static_cast<int64_t>(trade.total_value));
            transfer_cards(txn, trade.seller_id, trade.buyer_id,
                          trade.card_id, trade.quantity);
            record_trade(txn, trade);
            return 0;
        });
        auto txn_us = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - t0).count();
        LOG_INFO("[SETTLE] trade_id={} card={} price={} qty={} txn_duration_us={} status=ok",
                 trade.trade_id, trade.card_id, trade.price, trade.quantity, txn_us);

        if (on_trade_executed_) {
            on_trade_executed_(trade);
        }
        if (price_agg_service_) {
            price_agg_service_->on_trade(
                trade.card_id, trade.price, trade.quantity,
                std::chrono::system_clock::now()
            );
        }
    } catch (const std::exception& e) {
        LOG_ERROR("[SETTLE] trade_id={} status=failed error={}", trade.trade_id, e.what());
        throw TradeExecutionException(
            "Failed to execute trade " + trade.trade_id + ": " + std::string(e.what())
        );
    }
}

void TradeService::execute_trades(const std::vector<core::Trade>& trades) {
    try {
        db_->with_transaction([&](pqxx::work& txn) {
            for (const auto& trade : trades) {
                // Execute each trade's steps within the same transaction
                int64_t cost = static_cast<int64_t>(trade.total_value);
                update_balance(txn, trade.buyer_id, -cost);
                update_balance(txn, trade.seller_id, cost);
                transfer_cards(txn, trade.seller_id, trade.buyer_id, 
                              trade.card_id, trade.quantity);
                record_trade(txn, trade);
            }
            return 0;
        });
        
        if (on_trade_executed_) {
            for (const auto& trade : trades) {
                on_trade_executed_(trade);
            }
        }

        if (price_agg_service_) {
            for (const auto& trade : trades) {
                price_agg_service_->on_trade(
                    trade.card_id,
                    trade.price,
                    trade.quantity,
                    std::chrono::system_clock::now()
                );
            }
        }

    } catch (const std::exception& e) {
        throw TradeExecutionException(
            "Failed to execute batch of " + std::to_string(trades.size()) + 
            " trades: " + std::string(e.what())
        );
    }
}

void TradeService::update_balance(pqxx::work& txn, 
                                  const std::string& user_id, 
                                  int64_t delta) {
    // Using parameterized query to check current balance
    pqxx::result result = txn.exec_params(
        "SELECT gold_balance FROM users WHERE user_id = $1",
        user_id
    );
    
    if (result.empty()) {
        throw TradeExecutionException("User not found: " + user_id);
    }
    
    int64_t current_balance = result[0][0].as<int64_t>();
    int64_t new_balance = current_balance + delta;
    
    if (new_balance < 0) {
        throw TradeExecutionException(
            "Insufficient balance for user " + user_id + 
            " (has: " + std::to_string(current_balance) + 
            ", needs: " + std::to_string(-delta) + ")"
        );
    }
    
    // Using parameterized query to update balance
    txn.exec_params(
        "UPDATE users SET gold_balance = gold_balance + $1 WHERE user_id = $2",
        delta,
        user_id
    );
}

void TradeService::transfer_cards(pqxx::work& txn,
                                  const std::string& from_user_id,
                                  const std::string& to_user_id,
                                  const std::string& card_id,
                                  int quantity) {
    // Using parameterized query to check seller's inventory
    pqxx::result result = txn.exec_params(
        "SELECT quantity FROM user_inventory WHERE user_id = $1 AND card_id = $2",
        from_user_id,
        card_id
    );
    
    if (result.empty()) {
        throw TradeExecutionException(
            "Seller " + from_user_id + " does not own card: " + card_id
        );
    }
    
    int current_quantity = result[0][0].as<int>();
    
    if (current_quantity < quantity) {
        throw TradeExecutionException(
            "Seller " + from_user_id + " has insufficient cards " + card_id +
            " (has: " + std::to_string(current_quantity) + 
            ", needs: " + std::to_string(quantity) + ")"
        );
    }

    // Using parameterized query to deduct from seller
    txn.exec_params(
        "UPDATE user_inventory SET quantity = quantity - $1 "
        "WHERE user_id = $2 AND card_id = $3",
        quantity,
        from_user_id,
        card_id
    );

    // Using parameterized query for upsert to buyer
    txn.exec_params(
        "INSERT INTO user_inventory (user_id, card_id, quantity) "
        "VALUES ($1, $2, $3) "
        "ON CONFLICT (user_id, card_id) "
        "DO UPDATE SET quantity = user_inventory.quantity + $3",
        to_user_id,
        card_id,
        quantity
    );
}

void TradeService::record_trade(pqxx::work& txn, const core::Trade& trade) {
    // Using parameterized query to record trade
    txn.exec_params(
        "INSERT INTO trades "
        "(trade_id, card_id, buyer_id, seller_id, price, quantity, "
        "total_value, buyer_order_id, seller_order_id, merkle_hash, executed_at) "
        "VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, NOW())",
        trade.trade_id,
        trade.card_id,
        trade.buyer_id,
        trade.seller_id,
        trade.price,
        trade.quantity,
        trade.total_value,
        trade.buyer_order_id,
        trade.seller_order_id,
        trade.merkle_hash
    );
}

std::vector<core::Trade> TradeService::get_card_trades(const std::string& card_id,
                                                       int limit, int offset) const {
    auto result = db_->with_transaction([&](pqxx::work& txn) {
        return txn.exec_params(
            "SELECT * FROM trades WHERE card_id = $1 "
            "ORDER BY executed_at DESC LIMIT $2 OFFSET $3",
            card_id, limit, offset
        );
    });
    
    std::vector<core::Trade> trades;
    trades.reserve(result.size());
    
    for (const auto& row : result) {
        trades.push_back(parse_trade_from_row(row));
    }
    
    return trades;
}

std::vector<core::Trade> TradeService::get_user_trades(const std::string& user_id,
                                                       int limit, int offset) const {
    auto result = db_->with_transaction([&](pqxx::work& txn) {
        return txn.exec_params(
            "SELECT * FROM trades WHERE buyer_id = $1 OR seller_id = $1 "
            "ORDER BY executed_at DESC LIMIT $2 OFFSET $3",
            user_id, limit, offset
        );
    });
    
    std::vector<core::Trade> trades;
    trades.reserve(result.size());
    
    for (const auto& row : result) {
        trades.push_back(parse_trade_from_row(row));
    }
    
    return trades;
}

std::optional<core::Trade> TradeService::get_trade(const std::string& trade_id) const {
    // Using parameterized query
    auto result = db_->with_transaction([&](pqxx::work& txn) {
        return txn.exec_params(
            "SELECT * FROM trades WHERE trade_id = $1",
            trade_id
        );
    });
    
    if (result.empty()) {
        return std::nullopt;
    }
    
    return parse_trade_from_row(result[0]);
}

core::Trade TradeService::parse_trade_from_row(const pqxx::row& row) const {
    core::Trade trade;
    trade.trade_id = row["trade_id"].as<std::string>();
    trade.card_id = row["card_id"].as<std::string>();
    trade.buyer_id = row["buyer_id"].as<std::string>();
    trade.seller_id = row["seller_id"].as<std::string>();
    trade.price = row["price"].as<double>();
    trade.quantity = row["quantity"].as<int>();
    trade.total_value = row["total_value"].as<double>();
    trade.buyer_order_id = row["buyer_order_id"].as<std::string>();
    trade.seller_order_id = row["seller_order_id"].as<std::string>();
    trade.merkle_hash = row["merkle_hash"].as<std::string>();
    
    return trade;
}

} // namespace services
} // namespace clash_trading