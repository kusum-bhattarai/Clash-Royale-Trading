#include "api/routes.hpp"
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
                     std::shared_ptr<services::UserService> user_service)
    : server_(server)
    , auth_service_(auth_service)
    , db_(db)
    , order_service_(order_service)
    , trade_service_(trade_service)
    , user_service_(user_service) {}

void APIRouter::register_routes() {
    using namespace std::placeholders;
    
    fmt::print("\n📋 Registering API routes...\n");
    
    // Authentication routes
    server_->register_route(http::verb::post, "/api/auth/register",
        std::bind(&APIRouter::handle_register, this, _1, _2));
    server_->register_route(http::verb::post, "/api/auth/login",
        std::bind(&APIRouter::handle_login, this, _1, _2));
    
    // Order routes
    server_->register_route(http::verb::post, "/api/orders",
        std::bind(&APIRouter::handle_place_order, this, _1, _2));
    server_->register_route(http::verb::delete_, "/api/orders/:orderId",
        std::bind(&APIRouter::handle_cancel_order, this, _1, _2));
    server_->register_route(http::verb::get, "/api/orders/:orderId",
        std::bind(&APIRouter::handle_get_order, this, _1, _2));
    
    // User routes
    server_->register_route(http::verb::get, "/api/users/:userId/orders",
        std::bind(&APIRouter::handle_get_user_orders, this, _1, _2));
    server_->register_route(http::verb::get, "/api/users/:userId/portfolio",
        std::bind(&APIRouter::handle_get_portfolio, this, _1, _2));
    server_->register_route(http::verb::get, "/api/users/:userId/stats",
        std::bind(&APIRouter::handle_get_stats, this, _1, _2));
    server_->register_route(http::verb::get, "/api/users/:userId/trades",
        std::bind(&APIRouter::handle_get_user_trades, this, _1, _2));
    
    // Market data routes
    server_->register_route(http::verb::get, "/api/cards/:cardId/orderbook",
        std::bind(&APIRouter::handle_get_order_book, this, _1, _2));
    server_->register_route(http::verb::get, "/api/cards/:cardId/trades",
        std::bind(&APIRouter::handle_get_card_trades, this, _1, _2));
    
    // Trade routes
    server_->register_route(http::verb::get, "/api/trades/:tradeId",
        std::bind(&APIRouter::handle_get_trade, this, _1, _2));

    // Card routes
    server_->register_route(http::verb::get, "/api/cards",
        std::bind(&APIRouter::handle_get_all_cards, this, _1, _2));
    server_->register_route(http::verb::get, "/api/cards/:cardId",
        std::bind(&APIRouter::handle_get_card_details, this, _1, _2));
    
    fmt::print("✅ All routes registered!\n\n");
}

// Auth Handlers
void APIRouter::handle_register(const http_request& req, http_response& res) {
    try {
        auto body = nlohmann::json::parse(req.body());
        
        std::string username = body["username"];
        std::string email = body["email"];
        std::string password = body["password"];
        
        // Validate input
        if (username.empty() || email.empty() || password.empty()) {
            send_error(res, http::status::bad_request, "Missing required fields");
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
        std::string query = 
            "INSERT INTO users (user_id, username, email, password_hash, gold_balance) "
            "VALUES ('" + user_id + "', '" + username + "', '" + email + "', '" + 
            password_hash + "', 100000)";
        
        db_->execute(query);
        
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
        send_error(res, http::status::internal_server_error, e.what());
    }
}

void APIRouter::handle_login(const http_request& req, http_response& res) {
    try {
        auto body = nlohmann::json::parse(req.body());
        
        std::string username = body["username"];
        std::string password = body["password"];
        
        if (username.empty() || password.empty()) {
            send_error(res, http::status::bad_request, "Missing username or password");
            return;
        }
        
        // Get user from database
        std::string query = 
            "SELECT user_id, username, password_hash FROM users WHERE username = '" + 
            username + "'";
        
        pqxx::result result = db_->execute(query);
        
        if (result.empty()) {
            send_error(res, http::status::unauthorized, "Invalid credentials");
            return;
        }
        
        std::string user_id = result[0]["user_id"].as<std::string>();
        std::string stored_hash = result[0]["password_hash"].as<std::string>();
        
        // Verify password
        if (!auth_service_->verify_password(password, stored_hash)) {
            send_error(res, http::status::unauthorized, "Invalid credentials");
            return;
        }
        
        // Generate JWT
        std::string token = auth_service_->generate_jwt(user_id, username);
        
        nlohmann::json response_data = {
            {"token", token},
            {"user", {
                {"user_id", user_id},
                {"username", username}
            }}
        };
        
        send_json(res, http::status::ok, response_data);
        
    } catch (const std::exception& e) {
        send_error(res, http::status::internal_server_error, e.what());
    }
}

// Order Handlers
void APIRouter::handle_place_order(const http_request& req, http_response& res) {
    try {
        // Authenticate
        auto user_id_opt = get_user_from_auth(req);
        if (!user_id_opt) {
            send_error(res, http::status::unauthorized, "Invalid or missing token");
            return;
        }
        
        auto body = nlohmann::json::parse(req.body());
        
        // Build order request
        services::PlaceOrderRequest order_req;
        order_req.user_id = *user_id_opt;
        order_req.card_id = body["card_id"];
        order_req.quantity = body["quantity"];
        
        std::string type_str = body["type"];
        order_req.type = (type_str == "BUY") ? core::OrderType::BUY : core::OrderType::SELL;
        
        std::string mode_str = body["mode"];
        order_req.mode = (mode_str == "MARKET") ? core::OrderMode::MARKET : core::OrderMode::LIMIT;
        
        if (order_req.mode == core::OrderMode::LIMIT) {
            order_req.price = body["price"];
        }
        
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
        send_error(res, http::status::bad_request, e.what());
    } catch (const std::exception& e) {
        send_error(res, http::status::internal_server_error, e.what());
    }
}

void APIRouter::handle_cancel_order(const http_request& req, http_response& res) {
    try {
        // Authenticate
        auto user_id_opt = get_user_from_auth(req);
        if (!user_id_opt) {
            send_error(res, http::status::unauthorized, "Invalid or missing token");
            return;
        }
        
        // Extract order ID from path (simplified - just get last segment)
        std::string path = std::string(req.target());
        size_t last_slash = path.find_last_of('/');
        std::string order_id = path.substr(last_slash + 1);
        
        // Cancel order
        bool cancelled = order_service_->cancel_order(order_id, *user_id_opt);
        
        if (cancelled) {
            send_json(res, http::status::ok, {{"success", true}});
        } else {
            send_error(res, http::status::not_found, "Order not found or already cancelled");
        }
        
    } catch (const std::exception& e) {
        send_error(res, http::status::internal_server_error, e.what());
    }
}

void APIRouter::handle_get_user_orders(const http_request& req, http_response& res) {
    try {
        // Authenticate
        auto user_id_opt = get_user_from_auth(req);
        if (!user_id_opt) {
            send_error(res, http::status::unauthorized, "Invalid or missing token");
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
        send_error(res, http::status::internal_server_error, e.what());
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
            send_error(res, http::status::not_found, "Order not found");
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
        send_error(res, http::status::internal_server_error, e.what());
    }
}

// User Handlers
void APIRouter::handle_get_portfolio(const http_request& req, http_response& res) {
    try {
        // Authenticate
        auto user_id_opt = get_user_from_auth(req);
        if (!user_id_opt) {
            send_error(res, http::status::unauthorized, "Invalid or missing token");
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
        send_error(res, http::status::internal_server_error, e.what());
    }
}

void APIRouter::handle_get_stats(const http_request& req, http_response& res) {
    try {
        // Authenticate
        auto user_id_opt = get_user_from_auth(req);
        if (!user_id_opt) {
            send_error(res, http::status::unauthorized, "Invalid or missing token");
            return;
        }
        
        auto stats = user_service_->get_trading_stats(*user_id_opt);
        
        nlohmann::json stats_json = {
            {"total_trades", stats.total_trades},
            {"total_volume", stats.total_volume},
            {"buy_count", stats.buy_count},
            {"sell_count", stats.sell_count},
            {"trader_level", stats.trader_level}
        };
        
        send_json(res, http::status::ok, stats_json);
        
    } catch (const std::exception& e) {
        send_error(res, http::status::internal_server_error, e.what());
    }
}

// Market dta handlers
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
        send_error(res, http::status::internal_server_error, e.what());
    }
}

void APIRouter::handle_get_card_trades(const http_request& req, http_response& res) {
    try {
        // Extract card ID from path
        std::string path = std::string(req.target());
        size_t last_slash = path.find_last_of('/');
        size_t second_last_slash = path.find_last_of('/', last_slash - 1);
        std::string card_id = path.substr(second_last_slash + 1, last_slash - second_last_slash - 1);
        
        auto trades = trade_service_->get_card_trades(card_id, 50);
        
        nlohmann::json trades_json = nlohmann::json::array();
        for (const auto& trade : trades) {
            trades_json.push_back({
                {"trade_id", trade.trade_id},
                {"price", trade.price},
                {"quantity", trade.quantity},
                {"total_value", trade.total_value}
            });
        }
        
        send_json(res, http::status::ok, {{"trades", trades_json}});
        
    } catch (const std::exception& e) {
        send_error(res, http::status::internal_server_error, e.what());
    }
}

// Trade handlers
void APIRouter::handle_get_user_trades(const http_request& req, http_response& res) {
    try {
        // Authenticate
        auto user_id_opt = get_user_from_auth(req);
        if (!user_id_opt) {
            send_error(res, http::status::unauthorized, "Invalid or missing token");
            return;
        }
        
        auto trades = trade_service_->get_user_trades(*user_id_opt, 50);
        
        nlohmann::json trades_json = nlohmann::json::array();
        for (const auto& trade : trades) {
            trades_json.push_back({
                {"trade_id", trade.trade_id},
                {"card_id", trade.card_id},
                {"price", trade.price},
                {"quantity", trade.quantity},
                {"total_value", trade.total_value},
                {"buyer_id", trade.buyer_id},
                {"seller_id", trade.seller_id}
            });
        }
        
        send_json(res, http::status::ok, {{"trades", trades_json}});
        
    } catch (const std::exception& e) {
        send_error(res, http::status::internal_server_error, e.what());
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
            send_error(res, http::status::not_found, "Trade not found");
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
        send_error(res, http::status::internal_server_error, e.what());
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

void APIRouter::send_error(http_response& res, http::status status, const std::string& message) {
    res.result(status);
    res.set(http::field::content_type, "application/json");
    res.body() = nlohmann::json{{"error", message}}.dump();
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
        send_error(res, http::status::internal_server_error, e.what());
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
            send_error(res, http::status::not_found, "Card not found");
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
        send_error(res, http::status::internal_server_error, e.what());
    }
}

} // namespace api
} // namespace clash_trading