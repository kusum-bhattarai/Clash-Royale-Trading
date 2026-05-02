#include "services/order_service.hpp"
#include "utils/logger.hpp"
#include <uuid/uuid.h>
#include <sstream>
#include <chrono>

namespace clash_trading {
namespace services {

OrderService::OrderService(std::shared_ptr<database::PostgresClient> db,
                           std::shared_ptr<TradeService> trade_service,
                           std::shared_ptr<UserService> user_service)
    : db_(db), trade_service_(trade_service), user_service_(user_service) {}

std::string OrderService::generate_order_id() const {
    uuid_t uuid;
    uuid_generate(uuid);
    
    char uuid_str[37];
    uuid_unparse(uuid, uuid_str);
    
    return std::string(uuid_str);
}

PlaceOrderResponse OrderService::place_order(const PlaceOrderRequest& request) {
    PlaceOrderResponse response;
    
    // Validate the order
    if (request.type == core::OrderType::BUY) {
        validate_buy_order(request);
    } else {
        validate_sell_order(request);
    }
    
    // Create Order object
    core::Order order;
    order.order_id = generate_order_id();
    order.user_id = request.user_id;
    order.card_id = request.card_id;
    order.type = request.type;
    order.mode = request.mode;
    order.price = request.price;
    order.quantity = request.quantity;
    order.filled_quantity = 0;
    order.status = core::OrderStatus::PENDING;
    order.timestamp = std::chrono::system_clock::now();
    
    LOG_INFO("[ORDER] user={} card={} type={} mode={} qty={} price={} order_id={}",
             order.user_id, order.card_id,
             core::order_type_to_string(order.type),
             core::order_mode_to_string(order.mode),
             order.quantity, order.price, order.order_id);

    // Add order to database
    insert_order(order);

    // Route to OrderBook for matching
    auto order_book = get_or_create_order_book(request.card_id);
    auto match_start = std::chrono::steady_clock::now();
    std::vector<core::Trade> trades = order_book->match_order(order);
    auto match_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now() - match_start).count();

    // Execute trades if any occurred
    if (!trades.empty()) {
        for (const auto& t : trades) {
            LOG_INFO("[MATCH] trade_id={} buyer={} seller={} card={} price={} qty={} latency_ns={}",
                     t.trade_id, t.buyer_id, t.seller_id, t.card_id,
                     t.price, t.quantity, match_ns);
        }
        try {
            trade_service_->execute_trades(trades);
        } catch (const std::exception& e) {
            LOG_ERROR("[ORDER] trade execution failed order_id={} error={}", order.order_id, e.what());
            throw TradeExecutionException(
                "Order placed but trade execution failed: " + std::string(e.what())
            );
        }
    }
    
    // Update order status in database
    update_order_status(order.order_id, order.status, order.filled_quantity);

    // Broadcast order book update
    if (on_orderbook_update_) {
        auto snapshot = order_book->get_snapshot();
        on_orderbook_update_(request.card_id, snapshot);
    }
    
    // Notify user if order was filled
    if (on_order_filled_ && order.filled_quantity > 0) {
        on_order_filled_(
            request.user_id,
            order.order_id,
            core::order_status_to_string(order.status),
            order.filled_quantity
        );
    }
    
    // Build response
    response.order_id = order.order_id;
    response.status = order.status;
    response.filled_quantity = order.filled_quantity;
    response.trades = trades;
    
    return response;
}

bool OrderService::cancel_order(const std::string& order_id, const std::string& user_id) {
    // Get order from database to verify ownership and get card_id
    auto order_opt = get_order(order_id);
    
    if (!order_opt.has_value()) {
        return false; // Order not found
    }
    
    const auto& order = order_opt.value();
    
    // Verify user owns this order
    if (order.user_id != user_id) {
        throw std::runtime_error(
            "Permission denied: User " + user_id + " does not own order " + order_id
        );
    }
    
    // Check if order can be cancelled (must be PENDING or PARTIAL)
    if (order.status == core::OrderStatus::FILLED) {
        throw std::runtime_error("Cannot cancel order: already filled");
    }
    
    if (order.status == core::OrderStatus::CANCELLED) {
        return false; // Already cancelled
    }
    
    // Remove from OrderBook
    auto order_book = get_or_create_order_book(order.card_id);
    bool removed = order_book->cancel_order(order_id);
    
    // Update status in database
    if (removed) {
        update_order_status(order_id, core::OrderStatus::CANCELLED, order.filled_quantity);

        if (on_orderbook_update_) {
            auto snapshot = order_book->get_snapshot();
            on_orderbook_update_(order.card_id, snapshot);
        }
    }
    
    return removed;
}

std::vector<core::Order> OrderService::get_user_orders(const std::string& user_id) const {
    // Using parameterized query
    auto result = db_->with_transaction([&](pqxx::work& txn) {
        return txn.exec_params(
            "SELECT * FROM orders WHERE user_id = $1 ORDER BY created_at DESC",
            user_id
        );
    });
    
    std::vector<core::Order> orders;
    orders.reserve(result.size());
    
    for (const auto& row : result) {
        orders.push_back(parse_order_from_row(row));
    }
    
    return orders;
}

std::optional<core::Order> OrderService::get_order(const std::string& order_id) const {
    // Using parameterized query
    auto result = db_->with_transaction([&](pqxx::work& txn) {
        return txn.exec_params(
            "SELECT * FROM orders WHERE order_id = $1",
            order_id
        );
    });
    
    if (result.empty()) {
        return std::nullopt;
    }
    
    return parse_order_from_row(result[0]);
}

core::OrderBookSnapshot OrderService::get_order_book_snapshot(const std::string& card_id) const {
    // Check if order book exists
    {
        std::shared_lock lock(order_books_mutex_);
        auto it = order_books_.find(card_id);
        if (it != order_books_.end()) {
            return it->second->get_snapshot();
        }
    }
    
    // Order book doesn't exist yet, return empty snapshot
    core::OrderBookSnapshot snapshot;
    snapshot.card_id = card_id;
    snapshot.timestamp = std::chrono::system_clock::now();
    return snapshot;
}

std::shared_ptr<core::OrderBook> OrderService::get_or_create_order_book(
    const std::string& card_id) {
    
    // First try with read lock (fast path)
    {
        std::shared_lock lock(order_books_mutex_);
        auto it = order_books_.find(card_id);
        if (it != order_books_.end()) {
            return it->second;
        }
    }
    
    // Create new order book with write lock (slow path)
    std::unique_lock lock(order_books_mutex_);
    
    // Double-check in case another thread created it
    auto it = order_books_.find(card_id);
    if (it != order_books_.end()) {
        return it->second;
    }
    
    // Create and store new order book
    auto order_book = std::make_shared<core::OrderBook>(card_id);
    order_books_[card_id] = order_book;
    
    return order_book;
}

void OrderService::validate_buy_order(const PlaceOrderRequest& request) {
    // Get user's current balance
    auto user = user_service_->get_user(request.user_id);
    
    if (!user.has_value()) {
        throw OrderValidationException("User not found: " + request.user_id);
    }
    
    // Calculate required gold
    int64_t required_gold = calculate_required_gold(request);
    
    if (user->gold_balance < required_gold) {
        throw OrderValidationException(
            "Insufficient balance. Required: " + std::to_string(required_gold) + 
            ", Available: " + std::to_string(user->gold_balance)
        );
    }
}

void OrderService::validate_sell_order(const PlaceOrderRequest& request) {
    // Get user's portfolio to check inventory
    auto portfolio = user_service_->get_portfolio(request.user_id);
    
    // Find the card in holdings
    int available_quantity = 0;
    for (const auto& holding : portfolio.holdings) {
        if (holding.card_id == request.card_id) {
            available_quantity = holding.quantity;
            break;
        }
    }
    
    if (available_quantity < request.quantity) {
        throw OrderValidationException(
            "Insufficient inventory. Card: " + request.card_id + 
            ", Required: " + std::to_string(request.quantity) + 
            ", Available: " + std::to_string(available_quantity)
        );
    }
}

int64_t OrderService::calculate_required_gold(const PlaceOrderRequest& request) const {
    if (request.mode == core::OrderMode::LIMIT) {
        // For limit orders, we know the exact price
        return static_cast<int64_t>(request.price * request.quantity);
    } else {
        // Using parameterized query for market orders
        auto result = db_->with_transaction([&](pqxx::work& txn) {
            return txn.exec_params(
                "SELECT current_market_price FROM cards WHERE card_id = $1",
                request.card_id
            );
        });
        
        if (result.empty() || result[0][0].is_null()) {
            // No market price available, require a conservative amount
            return static_cast<int64_t>(request.quantity * 10000); // Conservative estimate
        }
        
        double market_price = result[0][0].as<double>();
        // Add 10% buffer for market orders (price might move)
        return static_cast<int64_t>(market_price * request.quantity * 1.1);
    }
}

void OrderService::insert_order(const core::Order& order) {
    // Using parameterized query
    db_->with_transaction([&](pqxx::work& txn) {
        txn.exec_params(
            "INSERT INTO orders "
            "(order_id, user_id, card_id, order_type, order_mode, price, "
            "quantity, filled_quantity, status, created_at, updated_at) "
            "VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, NOW(), NOW())",
            order.order_id,
            order.user_id,
            order.card_id,
            core::order_type_to_string(order.type),
            core::order_mode_to_string(order.mode),
            order.price,
            order.quantity,
            order.filled_quantity,
            core::order_status_to_string(order.status)
        );
    });
}

void OrderService::update_order_status(const std::string& order_id, 
                                       core::OrderStatus status,
                                       int filled_quantity) {
    // Using parameterized query
    db_->with_transaction([&](pqxx::work& txn) {
        txn.exec_params(
            "UPDATE orders SET "
            "status = $1, "
            "filled_quantity = $2, "
            "updated_at = NOW() "
            "WHERE order_id = $3",
            core::order_status_to_string(status),
            filled_quantity,
            order_id
        );
    });
}

core::Order OrderService::parse_order_from_row(const pqxx::row& row) const {
    core::Order order;
    order.order_id = row["order_id"].as<std::string>();
    order.user_id = row["user_id"].as<std::string>();
    order.card_id = row["card_id"].as<std::string>();
    
    std::string type_str = row["order_type"].as<std::string>();
    order.type = core::string_to_order_type(type_str).value_or(core::OrderType::BUY);
    
    std::string mode_str = row["order_mode"].as<std::string>();
    order.mode = core::string_to_order_mode(mode_str).value_or(core::OrderMode::MARKET);
    
    if (!row["price"].is_null()) {
        order.price = row["price"].as<double>();
    } else {
        order.price = 0.0;
    }
    
    order.quantity = row["quantity"].as<int>();
    order.filled_quantity = row["filled_quantity"].as<int>();
    
    std::string status_str = row["status"].as<std::string>();
    order.status = core::string_to_order_status(status_str)
        .value_or(core::OrderStatus::PENDING);
    
    order.timestamp = std::chrono::system_clock::now();
    
    return order;
}

} // namespace services
} // namespace clash_trading