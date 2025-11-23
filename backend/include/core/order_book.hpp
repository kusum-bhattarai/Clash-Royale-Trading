#pragma once

#include "core/order.hpp"
#include <map>
#include <deque>
#include <shared_mutex>
#include <vector>
#include <optional>
#include <string>

namespace clash_trading {
namespace core {

// Represents an executed trade
struct Trade {
    std::string trade_id;
    std::string card_id;
    std::string buyer_id;
    std::string seller_id;
    double price;
    int quantity;
    double total_value;          // price * quantity
    std::string buyer_order_id;
    std::string seller_order_id;
    std::string merkle_hash;
    std::chrono::system_clock::time_point timestamp;
    
    Trade() : price(0.0), quantity(0), total_value(0.0), timestamp(std::chrono::system_clock::now()) {}

    std::string calculate_merkle_hash() const;
    bool verify_integrity() const;
};

// Snapshot of order book state for real-time updates
struct OrderBookSnapshot {
    std::string card_id;
    std::vector<std::pair<double, int>> bids;  // [price, total_quantity]
    std::vector<std::pair<double, int>> asks;
    std::chrono::system_clock::time_point timestamp;
    
    OrderBookSnapshot() : timestamp(std::chrono::system_clock::now()) {}
};

// Order matching engine - one per card
class OrderBook {
private:
    // Price -> Queue of orders at that price
    // Bids: descending (highest price first)
    // Asks: ascending (lowest price first)
    std::map<double, std::deque<Order>, std::greater<>> bids_;
    std::map<double, std::deque<Order>> asks_;
    
    mutable std::shared_mutex mutex_;  // Thread-safe read/write
    std::string card_id_;
    
public:
    explicit OrderBook(std::string card_id);
    
    // Match incoming order against resting orders, returns executed trades
    std::vector<Trade> match_order(Order& incoming_order);
    
    // Add order to book (for unfilled limit orders)
    void add_order(const Order& order);
    
    // Cancel order from book
    bool cancel_order(const std::string& order_id);
    
    // Get current order book state (thread-safe read)
    OrderBookSnapshot get_snapshot() const;
    
    // Find order by ID
    std::optional<Order> get_order(const std::string& order_id) const;
    
    // Query methods
    double get_best_bid() const;
    double get_best_ask() const;
    double get_spread() const;
    size_t get_order_count() const;
    
private:
    // Helper to create trade from matched orders
    Trade create_trade(const Order& incoming, const Order& resting, 
                      int quantity, double price);
    
    // Generate unique trade ID
    std::string generate_trade_id() const;
};

} // namespace core
} // namespace clash_trading