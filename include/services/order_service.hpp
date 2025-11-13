#pragma once

#include "core/order_book.hpp"
#include "services/trade_service.hpp"
#include "services/user_service.hpp"
#include "database/postgres_client.hpp"
#include <unordered_map>
#include <memory>
#include <shared_mutex>
#include <stdexcept>
#include <functional>

namespace clash_trading {
namespace services {

class OrderValidationException : public std::runtime_error {
public:
    explicit OrderValidationException(const std::string& message) 
        : std::runtime_error(message) {}
};

// request to place an order
struct PlaceOrderRequest {
    std::string user_id;
    std::string card_id;
    core::OrderType type;           // BUY or SELL
    core::OrderMode mode;           // MARKET or LIMIT
    double price;                   // Required for LIMIT, ignored for MARKET
    int quantity;
    
    PlaceOrderRequest() : type(core::OrderType::BUY), 
                          mode(core::OrderMode::MARKET),
                          price(0.0), quantity(0) {}
};

// response after placing an order
struct PlaceOrderResponse {
    std::string order_id;
    core::OrderStatus status;       // PENDING, PARTIAL, or FILLED
    int filled_quantity;
    std::vector<core::Trade> trades; // Trades that were executed
    
    PlaceOrderResponse() : status(core::OrderStatus::PENDING), 
                           filled_quantity(0) {}
};

// Service to handle order placement, cancellation, and querying
class OrderService {
private:
    std::shared_ptr<database::PostgresClient> db_;
    std::shared_ptr<TradeService> trade_service_;
    std::shared_ptr<UserService> user_service_;
    
    // One OrderBook per card (lazy initialization)
    std::unordered_map<std::string, std::shared_ptr<core::OrderBook>> order_books_;
    mutable std::shared_mutex order_books_mutex_;

    // Callbacks for events
    std::function<void(const std::string&, const core::OrderBookSnapshot&)> on_orderbook_update_;
    std::function<void(const std::string&, const std::string&, const std::string&, int)> on_order_filled_;

public:
    OrderService(std::shared_ptr<database::PostgresClient> db,
                 std::shared_ptr<TradeService> trade_service,
                 std::shared_ptr<UserService> user_service);
    
    // Place a new order
    PlaceOrderResponse place_order(const PlaceOrderRequest& request);
    
    // Cancel an existing order
    bool cancel_order(const std::string& order_id, const std::string& user_id);
    
    // Get all orders for a user
    std::vector<core::Order> get_user_orders(const std::string& user_id) const;
    
    // Get order by ID
    std::optional<core::Order> get_order(const std::string& order_id) const;
    
    // Get current order book snapshot for a card
    core::OrderBookSnapshot get_order_book_snapshot(const std::string& card_id) const;

    // Set event handlers
    void set_orderbook_callback(
        std::function<void(const std::string&, const core::OrderBookSnapshot&)> callback) {
        on_orderbook_update_ = callback;
    }

    void set_order_filled_callback(
        std::function<void(const std::string&, const std::string&, const std::string&, int)> callback) {
        on_order_filled_ = callback;
    }
    
private:
    // Generate unique order ID
    std::string generate_order_id() const;
    
    // Get or create OrderBook for a card
    std::shared_ptr<core::OrderBook> get_or_create_order_book(const std::string& card_id);
    
    // Validate a buy order (check user balance)
    void validate_buy_order(const PlaceOrderRequest& request);
    
    // Validate a sell order (check user inventory)
    void validate_sell_order(const PlaceOrderRequest& request);
    
    // Insert new order into database
    void insert_order(const core::Order& order);
    
    // Update order status in database
    void update_order_status(const std::string& order_id, 
                            core::OrderStatus status,
                            int filled_quantity);
    
    // Parse order from database row
    core::Order parse_order_from_row(const pqxx::row& row) const;
    
    // Calculate required gold for a buy order
    int64_t calculate_required_gold(const PlaceOrderRequest& request) const;
};

} // namespace services
} // namespace clash_trading