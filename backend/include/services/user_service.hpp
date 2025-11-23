#pragma once

#include "database/postgres_client.hpp"
#include <string>
#include <vector>
#include <memory>
#include <optional>

namespace clash_trading {
namespace services {

// a user's holding of a specific card with P&L tracking
struct CardHolding {
    std::string card_id;
    std::string card_name;
    int quantity;
    double avg_purchase_price;      // Average price paid per card
    double current_market_price;    // Current market value per card
    double total_value;             // quantity * current_market_price
    double unrealized_pnl;          // Total profit/loss if sold at current price
    double unrealized_pnl_percent;  // Percentage gain/loss
    
    CardHolding() : quantity(0), avg_purchase_price(0.0), 
                    current_market_price(0.0), total_value(0.0),
                    unrealized_pnl(0.0), unrealized_pnl_percent(0.0) {}
};

// a user's complete portfolio
struct Portfolio {
    std::string user_id;
    int64_t gold_balance;
    double total_card_value;        // Sum of all card holdings
    double total_portfolio_value;   // gold_balance + total_card_value
    std::vector<CardHolding> holdings;
    
    Portfolio() : gold_balance(0), total_card_value(0.0), 
                  total_portfolio_value(0.0) {}
};

// trading stats for a user
struct TradingStats {
    int total_trades;               // Total number of trades
    int64_t total_volume;           // Total gold traded (sum of all trade values)
    int buy_count;                  // Number of buy trades
    int sell_count;                 // Number of sell trades
    std::string trader_level;       // NOVICE, MERCHANT, BARON, TYCOON, LEGEND
    
    TradingStats() : total_trades(0), total_volume(0), 
                     buy_count(0), sell_count(0), trader_level("NOVICE") {}
};

// basic user info
struct User {
    std::string user_id;
    std::string username;
    std::string email;
    int64_t gold_balance;
    std::string trader_level;
    int total_trades;
    
    User() : gold_balance(0), total_trades(0) {}
};

// Service for user-related operations
class UserService {
private:
    std::shared_ptr<database::PostgresClient> db_;

public:
    explicit UserService(std::shared_ptr<database::PostgresClient> db);
    
    // retreive user by ID
    std::optional<User> get_user(const std::string& user_id) const;
    
    // get user's portfolio
    Portfolio get_portfolio(const std::string& user_id) const;
    
    // get user's trading stats
    TradingStats get_trading_stats(const std::string& user_id) const;
    
    // calculate trader level based on total trades
    static std::string calculate_trader_level(int total_trades);
    
private:
    User parse_user_from_row(const pqxx::row& row) const;
    
    // calculate P&L for a specific card holding
    void calculate_holding_pnl(CardHolding& holding) const;
};

} // namespace services
} // namespace clash_trading