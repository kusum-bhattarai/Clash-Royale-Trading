#include "api/routes.hpp"
#include "api/error_codes.hpp"
#include "core/order.hpp"
#include <uuid/uuid.h>
#include <fmt/core.h>

namespace clash_trading {
namespace api {

APIRouter::APIRouter(std::shared_ptr<HTTPServer> server,
                     std::shared_ptr<AuthService> auth_service,
                     std::shared_ptr<database::PostgresClient> db,
                     std::shared_ptr<services::OrderService> order_service,
                     std::shared_ptr<services::TradeService> trade_service,
                     std::shared_ptr<services::UserService> user_service,
                     std::shared_ptr<services::PriceAggregationService> price_agg_service,
                     std::shared_ptr<services::AnalyticsService> analytics_service,
                     std::shared_ptr<services::DynamicPricingService> pricing_service)
    : server_(server)
    , auth_service_(auth_service)
    , db_(db)
    , order_service_(order_service)
    , trade_service_(trade_service)
    , user_service_(user_service)
    , price_agg_service_(price_agg_service)
    , analytics_service_(analytics_service)
    , pricing_service_(pricing_service) {}

void APIRouter::register_routes() {
    using namespace std::placeholders;
    
    fmt::print("\n📋 Registering API routes...\n");
    
    // Authentication routes
    server_->register_route(http::verb::post, "/api/v1/auth/register",
        std::bind(&APIRouter::handle_register, this, _1, _2));
    server_->register_route(http::verb::post, "/api/v1/auth/login",
        std::bind(&APIRouter::handle_login, this, _1, _2));

    // Order routes
    server_->register_route(http::verb::post, "/api/v1/orders",
        std::bind(&APIRouter::handle_place_order, this, _1, _2));
    server_->register_route(http::verb::delete_, "/api/v1/orders/:orderId",
        std::bind(&APIRouter::handle_cancel_order, this, _1, _2));
    server_->register_route(http::verb::get, "/api/v1/orders/:orderId",
        std::bind(&APIRouter::handle_get_order, this, _1, _2));

    // User routes
    server_->register_route(http::verb::get, "/api/v1/users/:userId/orders",
        std::bind(&APIRouter::handle_get_user_orders, this, _1, _2));
    server_->register_route(http::verb::get, "/api/v1/users/:userId/portfolio",
        std::bind(&APIRouter::handle_get_portfolio, this, _1, _2));
    server_->register_route(http::verb::get, "/api/v1/users/:userId/stats",
        std::bind(&APIRouter::handle_get_stats, this, _1, _2));
    server_->register_route(http::verb::get, "/api/v1/users/:userId/trades",
        std::bind(&APIRouter::handle_get_user_trades, this, _1, _2));

    // Market data routes
    server_->register_route(http::verb::get, "/api/v1/cards/:cardId/orderbook",
        std::bind(&APIRouter::handle_get_order_book, this, _1, _2));
    server_->register_route(http::verb::get, "/api/v1/cards/:cardId/trades",
        std::bind(&APIRouter::handle_get_card_trades, this, _1, _2));

    // Trade routes
    server_->register_route(http::verb::get, "/api/v1/trades/:tradeId",
        std::bind(&APIRouter::handle_get_trade, this, _1, _2));

    // Card routes
    server_->register_route(http::verb::get, "/api/v1/cards",
        std::bind(&APIRouter::handle_get_all_cards, this, _1, _2));
    server_->register_route(http::verb::get, "/api/v1/cards/:cardId",
        std::bind(&APIRouter::handle_get_card_details, this, _1, _2));

    // Price/Candle routes
    server_->register_route(http::verb::get, "/api/v1/cards/:cardId/candles",
        std::bind(&APIRouter::handle_get_candles, this, _1, _2));

    // Analytics routes
    server_->register_route(http::verb::get, "/api/v1/cards/:cardId/analytics",
        std::bind(&APIRouter::handle_get_analytics, this, _1, _2));

    server_->register_route(http::verb::get, "/api/v1/cards/:cardId/price",
        std::bind(&APIRouter::handle_get_card_price, this, _1, _2));

    fmt::print(" All routes registered!\n\n");
}

// Auth Handlers
void APIRouter::handle_register(const http_request& req, http_response& res) {
    try {
        auto body = nlohmann::json::parse(req.body());
        
        std::string username = body["username"];
        std::string email = body["email"];
        std::string password = body["password"];

        if (username.empty() || email.empty() || password.empty()) {
            send_error(res, http::status::bad_request,
                       "MISSING_FIELDS", "username, email and password are required",
                       error_codes::INVALID_REQUEST);
            return;
        }

        if (username.length() < 3 || username.length() > 20) {
            send_error(res, http::status::bad_request,
                       "INVALID_USERNAME", "Username must be 3-20 characters",
                       error_codes::INVALID_REQUEST);
            return;
        }

        if (email.find('@') == std::string::npos) {
            send_error(res, http::status::bad_request,
                       "INVALID_EMAIL", "Invalid email format",
                       error_codes::INVALID_REQUEST);
            return;
        }

        if (password.length() < 8) {
            send_error(res, http::status::bad_request,
                       "INVALID_PASSWORD", "Password must be at least 8 characters",
                       error_codes::INVALID_REQUEST);
            return;
        }

        auto existing_user = db_->with_transaction([&](pqxx::work& txn) {
            return txn.exec_params(
                "SELECT user_id FROM users WHERE username = $1 LIMIT 1",
                username
            );
        });
        if (!existing_user.empty()) {
            send_error(res, http::status::conflict,
                       "DUPLICATE_RESOURCE", "Username already taken",
                       error_codes::DUPLICATE_RESOURCE);
            return;
        }

        auto existing_email = db_->with_transaction([&](pqxx::work& txn) {
            return txn.exec_params(
                "SELECT user_id FROM users WHERE email = $1 LIMIT 1",
                email
            );
        });
        if (!existing_email.empty()) {
            send_error(res, http::status::conflict,
                       "DUPLICATE_RESOURCE", "Email already registered",
                       error_codes::DUPLICATE_RESOURCE);
            return;
        }
        
        // Hash password
        std::string password_hash = auth_service_->hash_password(password);
        
        // Generate user ID
        uuid_t uuid;
        uuid_generate(uuid);
        char uuid_str[37];
        uuid_unparse(uuid, uuid_str);
        std::string user_id(uuid_str);
        
        // Insert user into database
        db_->with_transaction([&](pqxx::work& txn) {
            txn.exec_params(
                "INSERT INTO users (user_id, username, email, password_hash, gold_balance) "
                "VALUES ($1, $2, $3, $4, 100000)",
                user_id, username, email, password_hash
            );
        });
        
        // Generate JWT
        std::string token = auth_service_->generate_jwt(user_id, username);
        
        nlohmann::json response_data = {
            {"token", token},
            {"user", {
                {"user_id", user_id},
                {"username", username},
                {"email", email},
                {"gold_balance", 100000}
            }}
        };
        
        send_json(res, http::status::created, response_data);
        
    } catch (const std::exception& e) {
        send_error(res, http::status::internal_server_error,
                   "INTERNAL_ERROR", e.what(), error_codes::INTERNAL_ERROR);
    }
}

void APIRouter::handle_login(const http_request& req, http_response& res) {
    try {
        auto body = nlohmann::json::parse(req.body());
        
        std::string username = body["username"];
        std::string password = body["password"];
        
        if (username.empty() || password.empty()) {
            send_error(res, http::status::bad_request,
                       "MISSING_FIELDS", "username and password are required",
                       error_codes::INVALID_REQUEST);
            return;
        }
        
        // Using parameterized query to prevent SQL injection
        auto result = db_->with_transaction([&](pqxx::work& txn) {
            return txn.exec_params(
                "SELECT user_id, username, password_hash, email, gold_balance, "
                "trader_level, total_trades, xp FROM users WHERE username = $1",
                username
            );
        });
        
        if (result.empty()) {
            send_error(res, http::status::unauthorized,
                       "INVALID_CREDENTIALS", "Invalid credentials",
                       error_codes::INVALID_CREDENTIALS);
            return;
        }

        std::string user_id = result[0]["user_id"].as<std::string>();
        std::string stored_hash = result[0]["password_hash"].as<std::string>();

        if (!auth_service_->verify_password(password, stored_hash)) {
            send_error(res, http::status::unauthorized,
                       "INVALID_CREDENTIALS", "Invalid credentials",
                       error_codes::INVALID_CREDENTIALS);
            return;
        }
        
        // Generate JWT
        std::string token = auth_service_->generate_jwt(user_id, username);
        
        // Return complete user object (frontend needs gold_balance)
        nlohmann::json response_data = {
            {"token", token},
            {"user", {
                {"user_id", user_id},
                {"username", username},
                {"email", result[0]["email"].as<std::string>()},
                {"gold_balance", result[0]["gold_balance"].as<int64_t>()},
                {"trader_level", result[0]["trader_level"].as<std::string>()},
                {"total_trades", result[0]["total_trades"].as<int>()},
                {"xp", result[0]["xp"].as<int>()}
            }}
        };
        
        send_json(res, http::status::ok, response_data);
        
    } catch (const std::exception& e) {
        send_error(res, http::status::internal_server_error,
                   "INTERNAL_ERROR", e.what(), error_codes::INTERNAL_ERROR);
    }
}

// Order Handlers
void APIRouter::handle_place_order(const http_request& req, http_response& res) {
    try {
        // Authenticate
        auto user_id_opt = get_user_from_auth(req);
        if (!user_id_opt) {
            send_error(res, http::status::unauthorized,
                       "UNAUTHORIZED", "Invalid or missing token",
                       error_codes::UNAUTHORIZED);
            return;
        }

        // Rate limit: 10 orders/sec per user, burst 10
        if (!rate_limiter_.allow(*user_id_opt)) {
            int64_t retry_ms = rate_limiter_.retry_after_ms(*user_id_opt);
            res.set("Retry-After", std::to_string((retry_ms + 999) / 1000));
            send_error(res, http::status::too_many_requests,
                       "RATE_LIMIT_EXCEEDED", "Rate limit exceeded: max 10 orders/sec",
                       error_codes::RATE_LIMIT_EXCEEDED);
            return;
        }

        auto body = nlohmann::json::parse(req.body());

        // Input validation
        if (!body.contains("card_id") || !body.contains("type") ||
            !body.contains("mode")    || !body.contains("quantity")) {
            send_error(res, http::status::bad_request,
                       "MISSING_FIELDS", "card_id, type, mode and quantity are required",
                       error_codes::INVALID_ORDER);
            return;
        }

        std::string card_id  = body["card_id"].get<std::string>();
        std::string type_str = body["type"].get<std::string>();
        std::string mode_str = body["mode"].get<std::string>();
        int quantity         = body["quantity"].get<int>();

        if (card_id.empty()) {
            send_error(res, http::status::bad_request,
                       "INVALID_ORDER", "card_id must not be empty",
                       error_codes::INVALID_ORDER);
            return;
        }
        if (type_str != "BUY" && type_str != "SELL") {
            send_error(res, http::status::bad_request,
                       "INVALID_ORDER", "type must be BUY or SELL",
                       error_codes::INVALID_ORDER);
            return;
        }
        if (mode_str != "MARKET" && mode_str != "LIMIT") {
            send_error(res, http::status::bad_request,
                       "INVALID_ORDER", "mode must be MARKET or LIMIT",
                       error_codes::INVALID_ORDER);
            return;
        }
        if (quantity <= 0) {
            send_error(res, http::status::bad_request,
                       "INVALID_ORDER", "quantity must be greater than 0",
                       error_codes::INVALID_ORDER);
            return;
        }
        if (quantity > 1000) {
            send_error(res, http::status::bad_request,
                       "INVALID_ORDER", "quantity cannot exceed 1000 per order",
                       error_codes::INVALID_ORDER);
            return;
        }

        double price = 0.0;
        if (mode_str == "LIMIT") {
            if (!body.contains("price")) {
                send_error(res, http::status::bad_request,
                           "INVALID_ORDER", "price is required for LIMIT orders",
                           error_codes::INVALID_ORDER);
                return;
            }
            price = body["price"].get<double>();
            if (price <= 0.0) {
                send_error(res, http::status::bad_request,
                           "INVALID_ORDER", "price must be greater than 0",
                           error_codes::INVALID_ORDER);
                return;
            }
        }

        // Build order request
        services::PlaceOrderRequest order_req;
        order_req.user_id  = *user_id_opt;
        order_req.card_id  = card_id;
        order_req.quantity = quantity;
        order_req.type     = (type_str == "BUY") ? core::OrderType::BUY : core::OrderType::SELL;
        order_req.mode     = (mode_str == "MARKET") ? core::OrderMode::MARKET : core::OrderMode::LIMIT;
        order_req.price    = price;
        
        // Place order
        auto response = order_service_->place_order(order_req);
        
        // Build response
        nlohmann::json response_data = {
            {"order_id", response.order_id},
            {"status", core::order_status_to_string(response.status)},
            {"filled_quantity", response.filled_quantity},
            {"trades", nlohmann::json::array()}
        };
        
        for (const auto& trade : response.trades) {
            response_data["trades"].push_back({
                {"trade_id", trade.trade_id},
                {"price", trade.price},
                {"quantity", trade.quantity},
                {"total_value", trade.total_value}
            });
        }
        
        send_json(res, http::status::ok, response_data);
        
    } catch (const services::OrderValidationException& e) {
        send_error(res, http::status::bad_request,
                   "INVALID_ORDER", e.what(), error_codes::INVALID_ORDER);
    } catch (const std::exception& e) {
        send_error(res, http::status::internal_server_error,
                   "INTERNAL_ERROR", e.what(), error_codes::INTERNAL_ERROR);
    }
}

void APIRouter::handle_cancel_order(const http_request& req, http_response& res) {
    try {
        auto user_id_opt = get_user_from_auth(req);
        if (!user_id_opt) {
            send_error(res, http::status::unauthorized,
                       "UNAUTHORIZED", "Invalid or missing token",
                       error_codes::UNAUTHORIZED);
            return;
        }

        std::string path = std::string(req.target());
        size_t last_slash = path.find_last_of('/');
        std::string order_id = path.substr(last_slash + 1);

        bool cancelled = order_service_->cancel_order(order_id, *user_id_opt);

        if (cancelled) {
            send_json(res, http::status::ok, {{"success", true}});
        } else {
            send_error(res, http::status::not_found,
                       "ORDER_NOT_FOUND", "Order not found or cannot be cancelled (already filled or cancelled)",
                       error_codes::RESOURCE_NOT_FOUND);
        }

    } catch (const std::exception& e) {
        send_error(res, http::status::internal_server_error,
                   "INTERNAL_ERROR", e.what(), error_codes::INTERNAL_ERROR);
    }
}

void APIRouter::handle_get_user_orders(const http_request& req, http_response& res) {
    try {
        auto user_id_opt = get_user_from_auth(req);
        if (!user_id_opt) {
            send_error(res, http::status::unauthorized,
                       "UNAUTHORIZED", "Invalid or missing token",
                       error_codes::UNAUTHORIZED);
            return;
        }
        
        // Get orders
        auto orders = order_service_->get_user_orders(*user_id_opt);
        
        nlohmann::json orders_json = nlohmann::json::array();
        for (const auto& order : orders) {
            orders_json.push_back({
                {"order_id", order.order_id},
                {"card_id", order.card_id},
                {"type", core::order_type_to_string(order.type)},
                {"mode", core::order_mode_to_string(order.mode)},
                {"price", order.price},
                {"quantity", order.quantity},
                {"filled_quantity", order.filled_quantity},
                {"status", core::order_status_to_string(order.status)}
            });
        }
        
        send_json(res, http::status::ok, {{"orders", orders_json}});

    } catch (const std::exception& e) {
        send_error(res, http::status::internal_server_error,
                   "INTERNAL_ERROR", e.what(), error_codes::INTERNAL_ERROR);
    }
}

void APIRouter::handle_get_order(const http_request& req, http_response& res) {
    try {
        // Extract order ID from path
        std::string path = std::string(req.target());
        size_t last_slash = path.find_last_of('/');
        std::string order_id = path.substr(last_slash + 1);
        
        auto order_opt = order_service_->get_order(order_id);
        
        if (!order_opt) {
            send_error(res, http::status::not_found,
                       "ORDER_NOT_FOUND", "Order not found",
                       error_codes::RESOURCE_NOT_FOUND);
            return;
        }
        
        const auto& order = *order_opt;
        nlohmann::json order_json = {
            {"order_id", order.order_id},
            {"user_id", order.user_id},
            {"card_id", order.card_id},
            {"type", core::order_type_to_string(order.type)},
            {"mode", core::order_mode_to_string(order.mode)},
            {"price", order.price},
            {"quantity", order.quantity},
            {"filled_quantity", order.filled_quantity},
            {"status", core::order_status_to_string(order.status)}
        };
        
        send_json(res, http::status::ok, order_json);

    } catch (const std::exception& e) {
        send_error(res, http::status::internal_server_error,
                   "INTERNAL_ERROR", e.what(), error_codes::INTERNAL_ERROR);
    }
}

// User Handlers
void APIRouter::handle_get_portfolio(const http_request& req, http_response& res) {
    try {
        auto user_id_opt = get_user_from_auth(req);
        if (!user_id_opt) {
            send_error(res, http::status::unauthorized,
                       "UNAUTHORIZED", "Invalid or missing token",
                       error_codes::UNAUTHORIZED);
            return;
        }
        
        auto portfolio = user_service_->get_portfolio(*user_id_opt);
        
        nlohmann::json holdings_json = nlohmann::json::array();
        for (const auto& holding : portfolio.holdings) {
            holdings_json.push_back({
                {"card_id", holding.card_id},
                {"card_name", holding.card_name},
                {"quantity", holding.quantity},
                {"avg_purchase_price", holding.avg_purchase_price},
                {"current_market_price", holding.current_market_price},
                {"total_value", holding.total_value},
                {"unrealized_pnl", holding.unrealized_pnl},
                {"unrealized_pnl_percent", holding.unrealized_pnl_percent}
            });
        }
        
        nlohmann::json portfolio_json = {
            {"user_id", portfolio.user_id},
            {"gold_balance", portfolio.gold_balance},
            {"total_card_value", portfolio.total_card_value},
            {"total_portfolio_value", portfolio.total_portfolio_value},
            {"holdings", holdings_json}
        };
        
        send_json(res, http::status::ok, portfolio_json);

    } catch (const std::exception& e) {
        send_error(res, http::status::internal_server_error,
                   "INTERNAL_ERROR", e.what(), error_codes::INTERNAL_ERROR);
    }
}

void APIRouter::handle_get_stats(const http_request& req, http_response& res) {
    try {
        auto user_id_opt = get_user_from_auth(req);
        if (!user_id_opt) {
            send_error(res, http::status::unauthorized,
                       "UNAUTHORIZED", "Invalid or missing token",
                       error_codes::UNAUTHORIZED);
            return;
        }
        
        auto stats = user_service_->get_trading_stats(*user_id_opt);
        
        nlohmann::json stats_json = {
            {"total_trades", stats.total_trades},
            {"total_volume", stats.total_volume},
            {"buy_count", stats.buy_count},
            {"sell_count", stats.sell_count},
            {"trader_level", stats.trader_level},
            {"xp", stats.xp}
        };
        
        send_json(res, http::status::ok, stats_json);

    } catch (const std::exception& e) {
        send_error(res, http::status::internal_server_error,
                   "INTERNAL_ERROR", e.what(), error_codes::INTERNAL_ERROR);
    }
}

// Market data handlers
void APIRouter::handle_get_order_book(const http_request& req, http_response& res) {
    try {
        // Extract card ID from path
        std::string path = std::string(req.target());
        size_t last_slash = path.find_last_of('/');
        size_t second_last_slash = path.find_last_of('/', last_slash - 1);
        std::string card_id = path.substr(second_last_slash + 1, last_slash - second_last_slash - 1);
        
        auto snapshot = order_service_->get_order_book_snapshot(card_id);
        
        nlohmann::json bids_json = nlohmann::json::array();
        for (const auto& [price, quantity] : snapshot.bids) {
            bids_json.push_back({price, quantity});
        }
        
        nlohmann::json asks_json = nlohmann::json::array();
        for (const auto& [price, quantity] : snapshot.asks) {
            asks_json.push_back({price, quantity});
        }
        
        nlohmann::json orderbook_json = {
            {"card_id", snapshot.card_id},
            {"bids", bids_json},
            {"asks", asks_json}
        };
        
        send_json(res, http::status::ok, orderbook_json);

    } catch (const std::exception& e) {
        send_error(res, http::status::internal_server_error,
                   "INTERNAL_ERROR", e.what(), error_codes::INTERNAL_ERROR);
    }
}

void APIRouter::handle_get_card_trades(const http_request& req, http_response& res) {
    try {
        std::string path = std::string(req.target());
        size_t query_pos = path.find('?');
        std::string path_clean = (query_pos != std::string::npos) ? path.substr(0, query_pos) : path;

        size_t last_slash        = path_clean.find_last_of('/');
        size_t second_last_slash = path_clean.find_last_of('/', last_slash - 1);
        std::string card_id = path_clean.substr(second_last_slash + 1, last_slash - second_last_slash - 1);

        int limit  = 50;
        int offset = 0;
        if (query_pos != std::string::npos) {
            std::string qs = path.substr(query_pos + 1);
            for (size_t pos = 0; pos < qs.size(); ) {
                size_t eq  = qs.find('=', pos);
                if (eq == std::string::npos) break;
                size_t amp = qs.find('&', eq);
                if (amp == std::string::npos) amp = qs.size();
                std::string key = qs.substr(pos, eq - pos);
                std::string val = qs.substr(eq + 1, amp - eq - 1);
                if (key == "limit")  limit  = std::max(1, std::min(200, std::stoi(val)));
                if (key == "offset") offset = std::max(0, std::stoi(val));
                pos = amp + 1;
            }
        }

        auto trades = trade_service_->get_card_trades(card_id, limit, offset);

        nlohmann::json trades_json = nlohmann::json::array();
        for (const auto& trade : trades) {
            trades_json.push_back({
                {"trade_id",    trade.trade_id},
                {"price",       trade.price},
                {"quantity",    trade.quantity},
                {"total_value", trade.total_value}
            });
        }

        send_json(res, http::status::ok, {
            {"trades",  trades_json},
            {"limit",   limit},
            {"offset",  offset}
        });

    } catch (const std::exception& e) {
        send_error(res, http::status::internal_server_error,
                   "INTERNAL_ERROR", e.what(), error_codes::INTERNAL_ERROR);
    }
}

// Trade handlers
void APIRouter::handle_get_user_trades(const http_request& req, http_response& res) {
    try {
        auto user_id_opt = get_user_from_auth(req);
        if (!user_id_opt) {
            send_error(res, http::status::unauthorized,
                       "UNAUTHORIZED", "Invalid or missing token",
                       error_codes::UNAUTHORIZED);
            return;
        }

        std::string path = std::string(req.target());
        size_t query_pos = path.find('?');
        int limit  = 50;
        int offset = 0;
        if (query_pos != std::string::npos) {
            std::string qs = path.substr(query_pos + 1);
            for (size_t pos = 0; pos < qs.size(); ) {
                size_t eq  = qs.find('=', pos);
                if (eq == std::string::npos) break;
                size_t amp = qs.find('&', eq);
                if (amp == std::string::npos) amp = qs.size();
                std::string key = qs.substr(pos, eq - pos);
                std::string val = qs.substr(eq + 1, amp - eq - 1);
                if (key == "limit")  limit  = std::max(1, std::min(200, std::stoi(val)));
                if (key == "offset") offset = std::max(0, std::stoi(val));
                pos = amp + 1;
            }
        }

        auto trades = trade_service_->get_user_trades(*user_id_opt, limit, offset);

        nlohmann::json trades_json = nlohmann::json::array();
        for (const auto& trade : trades) {
            trades_json.push_back({
                {"trade_id",    trade.trade_id},
                {"card_id",     trade.card_id},
                {"price",       trade.price},
                {"quantity",    trade.quantity},
                {"total_value", trade.total_value},
                {"buyer_id",    trade.buyer_id},
                {"seller_id",   trade.seller_id}
            });
        }

        send_json(res, http::status::ok, {
            {"trades",  trades_json},
            {"limit",   limit},
            {"offset",  offset}
        });

    } catch (const std::exception& e) {
        send_error(res, http::status::internal_server_error,
                   "INTERNAL_ERROR", e.what(), error_codes::INTERNAL_ERROR);
    }
}

void APIRouter::handle_get_trade(const http_request& req, http_response& res) {
    try {
        // Extract trade ID from path
        std::string path = std::string(req.target());
        size_t last_slash = path.find_last_of('/');
        std::string trade_id = path.substr(last_slash + 1);
        
        auto trade_opt = trade_service_->get_trade(trade_id);
        
        if (!trade_opt) {
            send_error(res, http::status::not_found,
                       "TRADE_NOT_FOUND", "Trade not found",
                       error_codes::RESOURCE_NOT_FOUND);
            return;
        }
        
        const auto& trade = *trade_opt;
        nlohmann::json trade_json = {
            {"trade_id", trade.trade_id},
            {"card_id", trade.card_id},
            {"buyer_id", trade.buyer_id},
            {"seller_id", trade.seller_id},
            {"price", trade.price},
            {"quantity", trade.quantity},
            {"total_value", trade.total_value},
            {"merkle_hash", trade.merkle_hash}
        };
        
        send_json(res, http::status::ok, trade_json);

    } catch (const std::exception& e) {
        send_error(res, http::status::internal_server_error,
                   "INTERNAL_ERROR", e.what(), error_codes::INTERNAL_ERROR);
    }
}

std::optional<std::string> APIRouter::get_user_from_auth(const http_request& req) {
    auto auth_header = req[http::field::authorization];
    if (auth_header.empty()) {
        return std::nullopt;
    }
    
    std::string auth_str = std::string(auth_header);
    
    // Extract token from "Bearer <token>"
    if (auth_str.find("Bearer ") != 0) {
        return std::nullopt;
    }
    
    std::string token = auth_str.substr(7);  // Remove "Bearer "
    
    auto payload = auth_service_->verify_jwt(token);
    if (!payload) {
        return std::nullopt;
    }
    
    return payload->user_id;
}

void APIRouter::send_json(http_response& res, http::status status, const nlohmann::json& data) {
    res.result(status);
    res.set(http::field::content_type, "application/json");
    res.body() = data.dump();
}

void APIRouter::send_error(http_response& res, http::status status,
                           const std::string& error_key,
                           const std::string& message,
                           int code) {
    res.result(status);
    res.set(http::field::content_type, "application/json");
    res.body() = nlohmann::json{
        {"error",   error_key},
        {"message", message},
        {"code",    code}
    }.dump();
}

// Card Handlers
void APIRouter::handle_get_all_cards(const http_request& req, http_response& res) {
    try {
        pqxx::work txn(*db_->get_connection());
        auto result = txn.exec(
            "SELECT card_id, name, rarity, elixir_cost, current_market_price, max_level FROM cards ORDER BY name"
        );
        
        nlohmann::json cards_json = nlohmann::json::array();
        
        for (const auto& row : result) {
            nlohmann::json card;
            card["card_id"] = std::string(row["card_id"].c_str());
            card["name"] = std::string(row["name"].c_str());
            card["rarity"] = std::string(row["rarity"].c_str());
            card["price"] = row["current_market_price"].as<double>();
            card["max_level"] = row["max_level"].as<int>();
            
            if (row["elixir_cost"].is_null()) {
                card["elixir_cost"] = nullptr;
            } else {
                card["elixir_cost"] = std::string(row["elixir_cost"].c_str());
            }
            
            cards_json.push_back(std::move(card));
        }
        
        txn.commit();
        
        std::string json_str = cards_json.dump();
        res.result(http::status::ok);
        res.set(http::field::content_type, "application/json");
        res.body() = std::move(json_str);
        res.prepare_payload();
        
    } catch (const std::exception& e) {
        send_error(res, http::status::internal_server_error,
                   "INTERNAL_ERROR", e.what(), error_codes::INTERNAL_ERROR);
    }
}

void APIRouter::handle_get_card_details(const http_request& req, http_response& res) {
    fmt::print("[API] Fetching card details\n"); 
    try {
        std::string path = std::string(req.target());
        size_t last_slash = path.find_last_of('/');
        std::string card_id = path.substr(last_slash + 1);
        
        pqxx::work txn(*db_->get_connection());
        
        auto result = txn.exec(
            "SELECT card_id, name, rarity, elixir_cost, current_market_price, "
            "max_level, icon_url, total_supply, usage_rate, last_synced "
            "FROM cards WHERE card_id = " + txn.quote(card_id)
        );
        
        if (result.empty()) {
            txn.commit();
            send_error(res, http::status::not_found,
                       "CARD_NOT_FOUND", "Card not found",
                       error_codes::RESOURCE_NOT_FOUND);
            return;
        }
        
        const auto& row = result[0];
        nlohmann::json card_json = {
            {"card_id", row["card_id"].c_str()},
            {"name", row["name"].c_str()},
            {"rarity", row["rarity"].c_str()},
            {"elixir_cost", row["elixir_cost"].is_null() ? nullptr : row["elixir_cost"].c_str()},
            {"current_market_price", row["current_market_price"].as<double>()},
            {"max_level", row["max_level"].as<int>()},
            {"icon_url", row["icon_url"].is_null() ? "" : row["icon_url"].c_str()},
            {"total_supply", row["total_supply"].as<long>()},
            {"usage_rate", row["usage_rate"].as<double>()},
            {"last_synced", row["last_synced"].c_str()}
        };
        
        txn.commit();
        send_json(res, http::status::ok, card_json);

    } catch (const std::exception& e) {
        send_error(res, http::status::internal_server_error,
                   "INTERNAL_ERROR", e.what(), error_codes::INTERNAL_ERROR);
    }
}

// Price/Candle Handler
void APIRouter::handle_get_candles(const http_request& req, http_response& res) {
    try {
        // Extract card ID from path: /api/cards/:cardId/candles
        std::string path = std::string(req.target());
        
        // Remove query string if present
        size_t query_pos = path.find('?');
        std::string path_without_query = (query_pos != std::string::npos) 
            ? path.substr(0, query_pos) 
            : path;
        
        // Extract card_id (between /cards/ and /candles)
        size_t cards_pos = path_without_query.find("/cards/");
        size_t candles_pos = path_without_query.find("/candles");
        std::string card_id = path_without_query.substr(
            cards_pos + 7,  // Length of "/cards/"
            candles_pos - (cards_pos + 7)
        );
        
        // Parse query parameters
        std::string timeframe_str = "1m";  // Default
        int limit = 500;  // Default
        
        if (query_pos != std::string::npos) {
            std::string query_string = path.substr(query_pos + 1);
            
            // Simple query parameter parsing
            size_t pos = 0;
            while (pos < query_string.length()) {
                size_t eq_pos = query_string.find('=', pos);
                if (eq_pos == std::string::npos) break;
                
                size_t amp_pos = query_string.find('&', eq_pos);
                if (amp_pos == std::string::npos) amp_pos = query_string.length();
                
                std::string key = query_string.substr(pos, eq_pos - pos);
                std::string value = query_string.substr(eq_pos + 1, amp_pos - eq_pos - 1);
                
                if (key == "timeframe") {
                    timeframe_str = value;
                } else if (key == "limit") {
                    limit = std::stoi(value);
                }
                
                pos = amp_pos + 1;
            }
        }
        
        // Convert timeframe string to enum
        trading::Timeframe timeframe = trading::string_to_timeframe(timeframe_str);
        
        // Get candles from service
        auto candles = price_agg_service_->get_candles(card_id, timeframe, limit);
        
        // Build JSON response
        nlohmann::json candles_json = nlohmann::json::array();
        for (const auto& candle : candles) {
            auto timestamp_seconds = std::chrono::duration_cast<std::chrono::seconds>(
                candle.timestamp.time_since_epoch()
            ).count();
            
            candles_json.push_back({
                {"timestamp", timestamp_seconds},
                {"open", candle.open_price},
                {"high", candle.high_price},
                {"low", candle.low_price},
                {"close", candle.close_price},
                {"volume", candle.volume},
                {"trade_count", candle.trade_count}
            });
        }
        
        nlohmann::json response_data = {
            {"card_id", card_id},
            {"timeframe", timeframe_str},
            {"candles", candles_json}
        };
        
        send_json(res, http::status::ok, response_data);

    } catch (const std::exception& e) {
        send_error(res, http::status::internal_server_error,
                   "INTERNAL_ERROR", e.what(), error_codes::INTERNAL_ERROR);
    }
}

// Analytics Handler
void APIRouter::handle_get_analytics(const http_request& req, http_response& res) {
    try {
        std::string path = std::string(req.target());
        size_t query_pos = path.find('?');
        std::string path_clean = (query_pos != std::string::npos)
                                 ? path.substr(0, query_pos) : path;

        size_t cards_pos     = path_clean.find("/cards/");
        size_t analytics_pos = path_clean.find("/analytics");
        if (cards_pos == std::string::npos || analytics_pos == std::string::npos) {
            send_error(res, http::status::bad_request,
                       "INVALID_REQUEST", "Invalid path",
                       error_codes::INVALID_REQUEST);
            return;
        }
        std::string card_id = path_clean.substr(
            cards_pos + 7,
            analytics_pos - (cards_pos + 7)
        );

        auto snap = analytics_service_->get_analytics(card_id);

        nlohmann::json resp = {
            {"spread", {
                {"instantaneous", snap.spread.instantaneous},
                {"twas_1h",       snap.spread.twas_1h},
                {"relative_pct",  snap.spread.relative_pct}
            }},
            {"order_flow_imbalance", {
                {"1m", snap.order_flow_imbalance.window_1m},
                {"5m", snap.order_flow_imbalance.window_5m}
            }},
            {"price_impact_bps", snap.price_impact_bps},
            {"volatility", {
                {"1m", snap.volatility_1m},
                {"1h", snap.volatility_1h}
            }},
            {"vwap", snap.vwap}
        };

        send_json(res, http::status::ok, resp);

    } catch (const std::exception& e) {
        send_error(res, http::status::internal_server_error,
                   "INTERNAL_ERROR", e.what(), error_codes::INTERNAL_ERROR);
    }
}

void APIRouter::handle_get_card_price(const http_request& req, http_response& res) {
    try {
        std::string path = std::string(req.target());
        size_t query_pos = path.find('?');
        std::string path_clean = (query_pos != std::string::npos)
                                 ? path.substr(0, query_pos) : path;

        size_t cards_pos = path_clean.find("/cards/");
        size_t price_pos = path_clean.find("/price");
        if (cards_pos == std::string::npos || price_pos == std::string::npos) {
            send_error(res, http::status::bad_request,
                       "INVALID_REQUEST", "Invalid path",
                       error_codes::INVALID_REQUEST);
            return;
        }
        std::string card_id = path_clean.substr(
            cards_pos + 7,
            price_pos - (cards_pos + 7)
        );

        auto result = pricing_service_->compute_reference_price(card_id);

        nlohmann::json resp = {
            {"reference_price", result.reference_price},
            {"factors", {
                {"base",         result.factors.base},
                {"sd",           result.factors.sd},
                {"meta",         result.factors.meta},
                {"vol_discount", result.factors.vol_discount}
            }}
        };

        send_json(res, http::status::ok, resp);

    } catch (const std::exception& e) {
        send_error(res, http::status::internal_server_error,
                   "INTERNAL_ERROR", e.what(), error_codes::INTERNAL_ERROR);
    }
}

} // namespace api
} // namespace clash_trading