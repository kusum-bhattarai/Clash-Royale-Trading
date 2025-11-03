#include "core/order_book.hpp"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <random>
#include <openssl/sha.h>

namespace clash_trading {
namespace core {

OrderBook::OrderBook(std::string card_id) 
    : card_id_(std::move(card_id)) {}

std::vector<Trade> OrderBook::match_order(Order& incoming_order) {
    std::unique_lock lock(mutex_);
    std::vector<Trade> trades;
    
    if (incoming_order.type == OrderType::BUY) {
        // BUY order matches against asks (sell orders)
        while (incoming_order.remaining_quantity() > 0 && !asks_.empty()) {
            auto& [price, orders] = *asks_.begin();
            
            // For limit orders, check if price matches
            if (incoming_order.mode == OrderMode::LIMIT) {
                if (incoming_order.price < price) break;  // Price too high
            }
            
            auto& resting_order = orders.front();
            int match_quantity = std::min(
                incoming_order.remaining_quantity(),
                resting_order.remaining_quantity()
            );
            
            Trade trade = create_trade(incoming_order, resting_order, match_quantity, price);
            trades.push_back(trade);
            
            incoming_order.filled_quantity += match_quantity;
            resting_order.filled_quantity += match_quantity;
            
            if (incoming_order.is_filled()) {
                incoming_order.status = OrderStatus::FILLED;
            } else if (incoming_order.filled_quantity > 0) {
                incoming_order.status = OrderStatus::PARTIAL;
            }
            
            if (resting_order.is_filled()) {
                resting_order.status = OrderStatus::FILLED;
                orders.pop_front();
                if (orders.empty()) {
                    asks_.erase(asks_.begin());
                }
            } else if (resting_order.filled_quantity > 0) {
                resting_order.status = OrderStatus::PARTIAL;
            }
        }
    } else {
        // SELL order matches against bids (buy orders)
        while (incoming_order.remaining_quantity() > 0 && !bids_.empty()) {
            auto& [price, orders] = *bids_.begin();
            
            // For limit orders, check if price matches
            if (incoming_order.mode == OrderMode::LIMIT) {
                if (incoming_order.price > price) break;  // Price too low
            }
            
            auto& resting_order = orders.front();
            int match_quantity = std::min(
                incoming_order.remaining_quantity(),
                resting_order.remaining_quantity()
            );
            
            Trade trade = create_trade(incoming_order, resting_order, match_quantity, price);
            trades.push_back(trade);
            
            incoming_order.filled_quantity += match_quantity;
            resting_order.filled_quantity += match_quantity;
            
            if (incoming_order.is_filled()) {
                incoming_order.status = OrderStatus::FILLED;
            } else if (incoming_order.filled_quantity > 0) {
                incoming_order.status = OrderStatus::PARTIAL;
            }
            
            if (resting_order.is_filled()) {
                resting_order.status = OrderStatus::FILLED;
                orders.pop_front();
                if (orders.empty()) {
                    bids_.erase(bids_.begin());
                }
            } else if (resting_order.filled_quantity > 0) {
                resting_order.status = OrderStatus::PARTIAL;
            }
        }
    }
    // Add remaining quantity to book if it's a limit order
    if (incoming_order.mode == OrderMode::LIMIT && 
        incoming_order.remaining_quantity() > 0) {
        add_order(incoming_order);
    }
    
    return trades;
}

void OrderBook::add_order(const Order& order) {
    // Assumes lock is already held by caller (match_order)
    if (order.type == OrderType::BUY) {
        bids_[order.price].push_back(order);
    } else {
        asks_[order.price].push_back(order);
    }
}

bool OrderBook::cancel_order(const std::string& order_id) {
    std::unique_lock lock(mutex_);
    
    // Search in bids
    for (auto& [price, orders] : bids_) {
        auto it = std::find_if(orders.begin(), orders.end(),
            [&order_id](const Order& o) { return o.order_id == order_id; });
        
        if (it != orders.end()) {
            orders.erase(it);
            if (orders.empty()) {
                bids_.erase(price);
            }
            return true;
        }
    }
    
    // Search in asks
    for (auto& [price, orders] : asks_) {
        auto it = std::find_if(orders.begin(), orders.end(),
            [&order_id](const Order& o) { return o.order_id == order_id; });
        
        if (it != orders.end()) {
            orders.erase(it);
            if (orders.empty()) {
                asks_.erase(price);
            }
            return true;
        }
    }
    
    return false;
}

OrderBookSnapshot OrderBook::get_snapshot() const {
    std::shared_lock lock(mutex_);
    
    OrderBookSnapshot snapshot;
    snapshot.card_id = card_id_;
    snapshot.timestamp = std::chrono::system_clock::now();
    
    // Aggregate bids by price
    for (const auto& [price, orders] : bids_) {
        int total_quantity = 0;
        for (const auto& order : orders) {
            total_quantity += order.remaining_quantity();
        }
        snapshot.bids.push_back({price, total_quantity});
    }
    
    // Aggregate asks by price
    for (const auto& [price, orders] : asks_) {
        int total_quantity = 0;
        for (const auto& order : orders) {
            total_quantity += order.remaining_quantity();
        }
        snapshot.asks.push_back({price, total_quantity});
    }
    
    return snapshot;
}

std::optional<Order> OrderBook::get_order(const std::string& order_id) const {
    std::shared_lock lock(mutex_);
    
    // Search in bids
    for (const auto& [price, orders] : bids_) {
        auto it = std::find_if(orders.begin(), orders.end(),
            [&order_id](const Order& o) { return o.order_id == order_id; });
        if (it != orders.end()) {
            return *it;
        }
    }
    
    // Search in asks
    for (const auto& [price, orders] : asks_) {
        auto it = std::find_if(orders.begin(), orders.end(),
            [&order_id](const Order& o) { return o.order_id == order_id; });
        if (it != orders.end()) {
            return *it;
        }
    }
    
    return std::nullopt;
}

double OrderBook::get_best_bid() const {
    std::shared_lock lock(mutex_);
    return bids_.empty() ? 0.0 : bids_.begin()->first;
}

double OrderBook::get_best_ask() const {
    std::shared_lock lock(mutex_);
    return asks_.empty() ? 0.0 : asks_.begin()->first;
}

double OrderBook::get_spread() const {
    std::shared_lock lock(mutex_);
    if (bids_.empty() || asks_.empty()) return 0.0;
    return asks_.begin()->first - bids_.begin()->first;
}

size_t OrderBook::get_order_count() const {
    std::shared_lock lock(mutex_);
    
    size_t count = 0;
    for (const auto& [price, orders] : bids_) {
        count += orders.size();
    }
    for (const auto& [price, orders] : asks_) {
        count += orders.size();
    }
    return count;
}

Trade OrderBook::create_trade(const Order& incoming, const Order& resting,
                              int quantity, double price) {
    Trade trade;
    trade.trade_id = generate_trade_id();
    trade.card_id = card_id_;
    trade.quantity = quantity;
    trade.price = price;
    trade.total_value = price * quantity;
    trade.timestamp = std::chrono::system_clock::now();
    
    // Determine buyer and seller
    if (incoming.type == OrderType::BUY) {
        trade.buyer_id = incoming.user_id;
        trade.seller_id = resting.user_id;
        trade.buyer_order_id = incoming.order_id;
        trade.seller_order_id = resting.order_id;
    } else {
        trade.buyer_id = resting.user_id;
        trade.seller_id = incoming.user_id;
        trade.buyer_order_id = resting.order_id;
        trade.seller_order_id = incoming.order_id;
    }
    trade.merkle_hash = trade.calculate_merkle_hash();
    return trade;
}

std::string OrderBook::generate_trade_id() const {
    // Simple UUID-like generation using timestamp + random
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    
    std::stringstream ss;
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    
    ss << "trade_" << std::hex << ms << "_";
    for (int i = 0; i < 8; i++) {
        ss << std::hex << dis(gen);
    }
    
    return ss.str();
}

std::string Trade::calculate_merkle_hash() const {
    // Concatenate all trade data for hashing
    std::ostringstream oss;
    oss << trade_id 
        << buyer_id 
        << seller_id 
        << card_id 
        << std::fixed << std::setprecision(2) << price 
        << quantity 
        << buyer_order_id 
        << seller_order_id;
    
    std::string data = oss.str();
    
    // Calculate SHA-256 hash
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(data.c_str()), 
           data.length(), 
           hash);
    
    // Convert to hex string
    std::ostringstream hex_stream;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        hex_stream << std::hex << std::setw(2) << std::setfill('0') 
                   << static_cast<int>(hash[i]);
    }
    
    return hex_stream.str();
}

bool Trade::verify_integrity() const {
    return calculate_merkle_hash() == merkle_hash;
}

} // namespace core
} // namespace clash_trading