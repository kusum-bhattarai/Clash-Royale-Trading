#pragma once

#include <string>
#include <chrono>
#include <optional>

namespace clash_trading {
namespace core {

// Order type: Buy or Sell
enum class OrderType {
    BUY,
    SELL
};

// Order mode: Market (execute immediately) or Limit (wait for price)
enum class OrderMode {
    MARKET,
    LIMIT
};

// Order status lifecycle
enum class OrderStatus {
    PENDING,    // Order placed, waiting to be matched
    PARTIAL,    // Order partially filled
    FILLED,     // Order completely filled
    CANCELLED   // Order cancelled by user
};

// Main Order structure
struct Order {
    std::string order_id;
    std::string user_id;
    std::string card_id;
    OrderType type;
    OrderMode mode;
    double price;              // Price per card (0 for market orders)
    int quantity;              // Total quantity requested
    int filled_quantity;       // Quantity filled so far
    OrderStatus status;
    std::chrono::system_clock::time_point timestamp;
    
    Order() : 
        type(OrderType::BUY),
        mode(OrderMode::MARKET),
        price(0.0),
        quantity(0),
        filled_quantity(0),
        status(OrderStatus::PENDING),
        timestamp(std::chrono::system_clock::now())
    {}
    
    // Check if order is fully filled
    bool is_filled() const {
        return quantity > 0 && filled_quantity >= quantity;
    }
    
    // Get remaining unfilled quantity
    int remaining_quantity() const {
        return quantity - filled_quantity;
    }
    
    // Check if order can be matched (has remaining quantity)
    bool can_match() const {
        return remaining_quantity() > 0 && 
               (status == OrderStatus::PENDING || status == OrderStatus::PARTIAL);
    }
    
    // Price-time priority comparison for sorting
    // For BUY orders: higher price has priority, then earlier timestamp
    // For SELL orders: lower price has priority, then earlier timestamp
    bool has_priority_over(const Order& other) const {
        // Must be same order type and card
        if (type != other.type || card_id != other.card_id) {
            return false;
        }
        
        // Compare prices
        if (price != other.price) {
            return type == OrderType::BUY ? price > other.price : price < other.price;
        }
        
        // If same price, earlier timestamp has priority
        return timestamp < other.timestamp;
    }
};

// Helper functions for string conversion
inline std::string order_type_to_string(OrderType type) {
    switch (type) {
        case OrderType::BUY:  return "BUY";
        case OrderType::SELL: return "SELL";
        default: return "UNKNOWN";
    }
}

inline std::string order_mode_to_string(OrderMode mode) {
    switch (mode) {
        case OrderMode::MARKET: return "MARKET";
        case OrderMode::LIMIT:  return "LIMIT";
        default: return "UNKNOWN";
    }
}

inline std::string order_status_to_string(OrderStatus status) {
    switch (status) {
        case OrderStatus::PENDING:   return "PENDING";
        case OrderStatus::PARTIAL:   return "PARTIAL";
        case OrderStatus::FILLED:    return "FILLED";
        case OrderStatus::CANCELLED: return "CANCELLED";
        default: return "UNKNOWN";
    }
}

inline std::optional<OrderType> string_to_order_type(const std::string& str) {
    if (str == "BUY")  return OrderType::BUY;
    if (str == "SELL") return OrderType::SELL;
    return std::nullopt;
}

inline std::optional<OrderMode> string_to_order_mode(const std::string& str) {
    if (str == "MARKET") return OrderMode::MARKET;
    if (str == "LIMIT")  return OrderMode::LIMIT;
    return std::nullopt;
}

inline std::optional<OrderStatus> string_to_order_status(const std::string& str) {
    if (str == "PENDING")   return OrderStatus::PENDING;
    if (str == "PARTIAL")   return OrderStatus::PARTIAL;
    if (str == "FILLED")    return OrderStatus::FILLED;
    if (str == "CANCELLED") return OrderStatus::CANCELLED;
    return std::nullopt;
}

} // namespace core
} // namespace clash_trading