#pragma once

#include "api/http_server.hpp"
#include "api/auth.hpp"
#include "services/order_service.hpp"
#include "services/trade_service.hpp"
#include "services/user_service.hpp"
#include "database/postgres_client.hpp"
#include <memory>
#include <nlohmann/json.hpp>

namespace clash_trading {
namespace api {

// API Router - registers all routes and handlers
class APIRouter {
private:
    std::shared_ptr<HTTPServer> server_;
    std::shared_ptr<AuthService> auth_service_;
    std::shared_ptr<database::PostgresClient> db_;
    std::shared_ptr<services::OrderService> order_service_;
    std::shared_ptr<services::TradeService> trade_service_;
    std::shared_ptr<services::UserService> user_service_;
    
public:
    APIRouter(std::shared_ptr<HTTPServer> server,
             std::shared_ptr<AuthService> auth_service,
             std::shared_ptr<database::PostgresClient> db,
             std::shared_ptr<services::OrderService> order_service,
             std::shared_ptr<services::TradeService> trade_service,
             std::shared_ptr<services::UserService> user_service);
    
    // Register all routes
    void register_routes();
    
private:
    // Auth Routes
    void handle_register(const http_request& req, http_response& res);
    void handle_login(const http_request& req, http_response& res);
    
    // Order Routes
    void handle_place_order(const http_request& req, http_response& res);
    void handle_cancel_order(const http_request& req, http_response& res);
    void handle_get_user_orders(const http_request& req, http_response& res);
    void handle_get_order(const http_request& req, http_response& res);
    
    // User Routes
    void handle_get_portfolio(const http_request& req, http_response& res);
    void handle_get_stats(const http_request& req, http_response& res);
    
    // Market Routes
    void handle_get_order_book(const http_request& req, http_response& res);
    void handle_get_card_trades(const http_request& req, http_response& res);
    
    // Trade Routes
    void handle_get_user_trades(const http_request& req, http_response& res);
    void handle_get_trade(const http_request& req, http_response& res);

    // Card Routes
    void handle_get_all_cards(const http_request& req, http_response& res);
    void handle_get_card_details(const http_request& req, http_response& res);
    
    
    // Extract user ID from JWT token in Authorization header
    std::optional<std::string> get_user_from_auth(const http_request& req);
    
    // Extract path parameter by name
    std::string extract_path_param(const std::string& path, const std::string& param_name) const;
    
    // Send JSON response
    void send_json(http_response& res, http::status status, const nlohmann::json& data);
    
    // Send error response
    void send_error(http_response& res, http::status status, const std::string& message);
};

} // namespace api
} // namespace clash_trading