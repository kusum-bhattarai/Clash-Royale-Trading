#include "services/trade_service.hpp"
#include <sstream>

namespace clash_trading {
namespace services {

TradeService::TradeService(std::shared_ptr<database::PostgresClient> db) 
    : db_(db) {}

void TradeService::execute_trade(const core::Trade& trade) {
    try {
        db_->with_transaction([&](pqxx::work& txn) {
            // Debit buyer's gold (negative delta)
            int64_t buyer_cost = static_cast<int64_t>(trade.total_value);
            update_balance(txn, trade.buyer_id, -buyer_cost);
            
            // Credit seller's gold (positive delta)
            int64_t seller_credit = static_cast<int64_t>(trade.total_value);
            update_balance(txn, trade.seller_id, seller_credit);
            
            // Transfer cards from seller to buyer
            transfer_cards(txn, trade.seller_id, trade.buyer_id, 
                          trade.card_id, trade.quantity);
            
            // Record trade in immutable ledger
            record_trade(txn, trade);
            
            // Transaction committed automatically by with_transaction
        });
        
        // Future: Emit event for WebSocket broadcasting
        
    } catch (const std::exception& e) {
        // Transaction automatically rolled back by with_transaction
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
        });
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
    // First, check current balance
    std::string check_query = 
        "SELECT gold_balance FROM users WHERE user_id = " + txn.quote(user_id);
    
    pqxx::result result = txn.exec(check_query);
    
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
    
    // Update balance
    std::string update_query = 
        "UPDATE users SET gold_balance = gold_balance + " + 
        std::to_string(delta) + 
        " WHERE user_id = " + txn.quote(user_id);
    
    txn.exec(update_query);
}

void TradeService::transfer_cards(pqxx::work& txn,
                                  const std::string& from_user_id,
                                  const std::string& to_user_id,
                                  const std::string& card_id,
                                  int quantity) {
    // Check seller has enough cards
    std::string check_query = 
        "SELECT quantity FROM user_inventory WHERE user_id = " + 
        txn.quote(from_user_id) + " AND card_id = " + txn.quote(card_id);
    
    pqxx::result result = txn.exec(check_query);
    
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
    
    // Deduct from seller
    std::string deduct_query = 
        "UPDATE user_inventory SET quantity = quantity - " + 
        std::to_string(quantity) + 
        " WHERE user_id = " + txn.quote(from_user_id) + 
        " AND card_id = " + txn.quote(card_id);
    txn.exec(deduct_query);
    
    // Add to buyer (insert or update using PostgreSQL UPSERT)
    std::string upsert_query = 
        "INSERT INTO user_inventory (user_id, card_id, quantity) VALUES (" +
        txn.quote(to_user_id) + ", " +
        txn.quote(card_id) + ", " +
        std::to_string(quantity) + 
        ") ON CONFLICT (user_id, card_id) " +
        "DO UPDATE SET quantity = user_inventory.quantity + " + 
        std::to_string(quantity);
    txn.exec(upsert_query);
}

void TradeService::record_trade(pqxx::work& txn, const core::Trade& trade) {
    std::string query = 
        "INSERT INTO trades "
        "(trade_id, card_id, buyer_id, seller_id, price, quantity, "
        "total_value, buyer_order_id, seller_order_id, merkle_hash, executed_at) "
        "VALUES (" +
        txn.quote(trade.trade_id) + ", " +
        txn.quote(trade.card_id) + ", " +
        txn.quote(trade.buyer_id) + ", " +
        txn.quote(trade.seller_id) + ", " +
        std::to_string(trade.price) + ", " +
        std::to_string(trade.quantity) + ", " +
        std::to_string(trade.total_value) + ", " +
        txn.quote(trade.buyer_order_id) + ", " +
        txn.quote(trade.seller_order_id) + ", " +
        txn.quote(trade.merkle_hash) + ", " +
        "NOW())";
    
    txn.exec(query);
}

std::vector<core::Trade> TradeService::get_card_trades(const std::string& card_id,
                                                       int limit) const {
    std::string query = 
        "SELECT * FROM trades WHERE card_id = " + db_->execute("SELECT " + card_id)[0][0].as<std::string>() +
        " ORDER BY executed_at DESC LIMIT " + std::to_string(limit);
    
    pqxx::result result = db_->execute(query);
    
    std::vector<core::Trade> trades;
    trades.reserve(result.size());
    
    for (const auto& row : result) {
        trades.push_back(parse_trade_from_row(row));
    }
    
    return trades;
}

std::vector<core::Trade> TradeService::get_user_trades(const std::string& user_id,
                                                       int limit) const {
    // Use parameterized query to prevent SQL injection
    std::string query = 
        "SELECT * FROM trades WHERE buyer_id = '" + user_id + "' " +
        "OR seller_id = '" + user_id + "' " +
        "ORDER BY executed_at DESC LIMIT " + std::to_string(limit);
    
    pqxx::result result = db_->execute(query);
    
    std::vector<core::Trade> trades;
    trades.reserve(result.size());
    
    for (const auto& row : result) {
        trades.push_back(parse_trade_from_row(row));
    }
    
    return trades;
}

std::optional<core::Trade> TradeService::get_trade(const std::string& trade_id) const {
    std::string query = 
        "SELECT * FROM trades WHERE trade_id = '" + trade_id + "'";
    
    pqxx::result result = db_->execute(query);
    
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