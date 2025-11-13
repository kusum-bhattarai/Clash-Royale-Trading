#pragma once

#include "core/order_book.hpp"
#include "database/postgres_client.hpp"
#include <vector>
#include <memory>
#include <stdexcept>
#include <functional>

namespace clash_trading {
namespace services {

class TradeExecutionException : public std::runtime_error {
public:
    explicit TradeExecutionException(const std::string& message) 
        : std::runtime_error(message) {}
};

// Service for handling trade executions and queries
class TradeService {
private:
    std::shared_ptr<database::PostgresClient> db_;
    std::function<void(const core::Trade&)> on_trade_executed_;

public:
    explicit TradeService(std::shared_ptr<database::PostgresClient> db);

    void set_trade_callback(std::function<void(const core::Trade&)> callback) {
        on_trade_executed_ = callback;
    }
    
    // Execute a single trade
    void execute_trade(const core::Trade& trade);
    
    // Execute multiple trades atomically
    // All trades succeed together or all fail together
    void execute_trades(const std::vector<core::Trade>& trades);
    
    // Get recent trades for a specific card
    // Returns a vector of trades sorted by timestamp descending
    std::vector<core::Trade> get_card_trades(const std::string& card_id, 
                                             int limit = 50) const;
    
    // Get recent trades for a specific user (as buyer or seller)
    std::vector<core::Trade> get_user_trades(const std::string& user_id, 
                                             int limit = 50) const;
    
    // Get trade by its unique ID
    std::optional<core::Trade> get_trade(const std::string& trade_id) const;
    
private:
    // Update user gold balance within a transaction
    void update_balance(pqxx::work& txn, const std::string& user_id, int64_t delta);
    
    // Transfer cards between users within a transaction
    void transfer_cards(pqxx::work& txn, 
                       const std::string& from_user_id,
                       const std::string& to_user_id,
                       const std::string& card_id, 
                       int quantity);
    
    // Record trade in the database within a transaction
    void record_trade(pqxx::work& txn, const core::Trade& trade);
    
    // Parse a Trade object from a database row
    core::Trade parse_trade_from_row(const pqxx::row& row) const;
};

} // namespace services
} // namespace clash_trading