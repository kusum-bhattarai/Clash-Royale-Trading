#include "services/user_service.hpp"
#include <sstream>

namespace clash_trading {
namespace services {

UserService::UserService(std::shared_ptr<database::PostgresClient> db) 
    : db_(db) {}

std::optional<User> UserService::get_user(const std::string& user_id) const {
    std::string query = 
        "SELECT user_id, username, email, gold_balance, trader_level, total_trades "
        "FROM users WHERE user_id = '" + user_id + "'";
    
    pqxx::result result = db_->execute(query);
    
    if (result.empty()) {
        return std::nullopt;
    }
    
    return parse_user_from_row(result[0]);
}

Portfolio UserService::get_portfolio(const std::string& user_id) const {
    Portfolio portfolio;
    portfolio.user_id = user_id;
    
    // Get user's gold balance
    std::string balance_query = 
        "SELECT gold_balance FROM users WHERE user_id = '" + user_id + "'";
    
    pqxx::result balance_result = db_->execute(balance_query);
    
    if (balance_result.empty()) {
        // User not found, return empty portfolio
        return portfolio;
    }
    
    portfolio.gold_balance = balance_result[0][0].as<int64_t>();
    
    // Get all card holdings with current market prices
    std::string holdings_query = 
        "SELECT ui.card_id, c.name, ui.quantity, ui.avg_purchase_price, "
        "c.current_market_price "
        "FROM user_inventory ui "
        "JOIN cards c ON ui.card_id = c.card_id "
        "WHERE ui.user_id = '" + user_id + "' AND ui.quantity > 0";
    
    pqxx::result holdings_result = db_->execute(holdings_query);
    
    // Build holdings and calculate values
    for (const auto& row : holdings_result) {
        CardHolding holding;
        holding.card_id = row["card_id"].as<std::string>();
        holding.card_name = row["name"].as<std::string>();
        holding.quantity = row["quantity"].as<int>();
        
        // Handle NULL avg_purchase_price (card received as gift/initial inventory)
        if (!row["avg_purchase_price"].is_null()) {
            holding.avg_purchase_price = row["avg_purchase_price"].as<double>();
        } else {
            holding.avg_purchase_price = 0.0;
        }
        
        // Handle NULL current_market_price
        if (!row["current_market_price"].is_null()) {
            holding.current_market_price = row["current_market_price"].as<double>();
        } else {
            holding.current_market_price = 0.0;
        }
        
        // Calculate values and P&L
        calculate_holding_pnl(holding);
        
        portfolio.holdings.push_back(holding);
        portfolio.total_card_value += holding.total_value;
    }
    
    // Calculate total portfolio value
    portfolio.total_portfolio_value = 
        static_cast<double>(portfolio.gold_balance) + portfolio.total_card_value;
    
    return portfolio;
}

TradingStats UserService::get_trading_stats(const std::string& user_id) const {
    TradingStats stats;
    
    // Get basic stats from users table
    std::string user_query = 
        "SELECT trader_level, total_trades FROM users WHERE user_id = '" + user_id + "'";
    
    pqxx::result user_result = db_->execute(user_query);
    
    if (user_result.empty()) {
        return stats; // User not found
    }
    
    stats.trader_level = user_result[0]["trader_level"].as<std::string>();
    stats.total_trades = user_result[0]["total_trades"].as<int>();
    
    // Get detailed trading statistics from trades table
    std::string trades_query = 
        "SELECT "
        "  COUNT(*) as total_count, "
        "  COALESCE(SUM(total_value), 0) as total_vol, "
        "  COALESCE(SUM(CASE WHEN buyer_id = '" + user_id + "' THEN 1 ELSE 0 END), 0) as buys, "
        "  COALESCE(SUM(CASE WHEN seller_id = '" + user_id + "' THEN 1 ELSE 0 END), 0) as sells "
        "FROM trades "
        "WHERE buyer_id = '" + user_id + "' OR seller_id = '" + user_id + "'";
    
    pqxx::result trades_result = db_->execute(trades_query);
    
    if (!trades_result.empty()) {
        const auto& row = trades_result[0];
        
        // Update total_trades from actual trade history (more accurate than users table)
        int actual_trades = row["total_count"].as<int>();
        if (actual_trades > 0) {
            stats.total_trades = actual_trades;
        }
        
        stats.total_volume = row["total_vol"].as<int64_t>();
        stats.buy_count = row["buys"].as<int>();
        stats.sell_count = row["sells"].as<int>();
    }
    
    return stats;
}

std::string UserService::calculate_trader_level(int total_trades) {
    if (total_trades >= 10001) {
        return "ULTIMATE_CHAMPION";  // Highest rank in CR
    } else if (total_trades >= 2001) {
        return "GRAND_CHAMPION";
    } else if (total_trades >= 501) {
        return "CHAMPION";
    } else if (total_trades >= 101) {
        return "MASTER";
    } else {
        return "CHALLENGER";  // Starting rank
    }
}

User UserService::parse_user_from_row(const pqxx::row& row) const {
    User user;
    user.user_id = row["user_id"].as<std::string>();
    user.username = row["username"].as<std::string>();
    user.email = row["email"].as<std::string>();
    user.gold_balance = row["gold_balance"].as<int64_t>();
    user.trader_level = row["trader_level"].as<std::string>();
    user.total_trades = row["total_trades"].as<int>();
    return user;
}

void UserService::calculate_holding_pnl(CardHolding& holding) const {
    // Calculate total value at current market price
    holding.total_value = holding.quantity * holding.current_market_price;
    
    // Calculate unrealized P&L
    if (holding.avg_purchase_price > 0.0) {
        double cost_basis = holding.quantity * holding.avg_purchase_price;
        holding.unrealized_pnl = holding.total_value - cost_basis;
        
        // Calculate percentage P&L
        if (cost_basis > 0.0) {
            holding.unrealized_pnl_percent = 
                (holding.unrealized_pnl / cost_basis) * 100.0;
        }
    } else {
        // No cost basis (free cards), all value is profit
        holding.unrealized_pnl = holding.total_value;
        holding.unrealized_pnl_percent = 0.0;
    }
}

} // namespace services
} // namespace clash_trading