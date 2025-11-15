#include <iostream>
#include <memory>
#include <thread>
#include <fmt/core.h>
#include <boost/asio.hpp>

#include "utils/config.hpp"
#include "database/postgres_client.hpp"
#include "services/order_service.hpp"
#include "services/trade_service.hpp"
#include "services/user_service.hpp"
#include "api/auth.hpp"
#include "api/http_server.hpp"
#include "api/routes.hpp"

#include "api/websocket.hpp"
#include "api/event_broadcaster.hpp"

using namespace clash_trading;
namespace net = boost::asio; 

int main(int argc, char* argv[]) {
    try {
        // Banner
        std::cout << R"(
   _____ _           _       _____                   _      
  / ____| |         | |     |  __ \                 | |     
 | |    | | __ _ ___| |__   | |__) |___  _   _  __ _| | ___ 
 | |    | |/ _` / __| '_ \  |  _  // _ \| | | |/ _` |  / _ \
 | |____| | (_| \__ \ | | | | | \ \ (_) | |_| | (_| |    __/
  \_____|_|\__,_|___/_| |_| |_|  \_\___/ \__, |\__,_|__\___|
                                          __/ |            
                                         |___/             
        )" << std::endl;
        
        fmt::print("🏆 Clash Royale Trading Platform v1.0.0\n");
        fmt::print("High-Performance Card Trading Engine\n\n");
        
        // Load configuration
        auto& config = clash::Config::instance();
        config.load_from_file(".env");
        
        fmt::print("Configuration:\n");
        fmt::print("  Database: {}:{}/{}\n", 
                  config.postgres_config().host,
                  config.postgres_config().port,
                  config.postgres_config().database);
        fmt::print("  Redis: {}:{}\n", 
                  config.redis_config().host,
                  config.redis_config().port);
        fmt::print("  Server: {}:{}\n\n", 
                  config.server_host(),
                  config.server_port());
        
        // Initialize database connection
        fmt::print("Connecting to PostgreSQL...\n");
        auto db = std::make_shared<database::PostgresClient>(
            config.postgres_config().connection_string()
        );
        
        if (!db->is_connected()) {
            fmt::print(stderr, "Failed to connect to database!\n");
            return 1;
        }
        fmt::print("Database connected!\n\n");
        
        // Initialize services
        fmt::print("Initializing services...\n");
        
        auto trade_service = std::make_shared<services::TradeService>(db);
        fmt::print("  TradeService ready\n");
        
        auto user_service = std::make_shared<services::UserService>(db);
        fmt::print("  UserService ready\n");
        
        auto order_service = std::make_shared<services::OrderService>(
            db, trade_service, user_service
        );
        fmt::print("  OrderService ready\n");
        
        auto auth_service = std::make_shared<api::AuthService>(
            config.jwt_secret()
        );
        fmt::print("  AuthService ready\n\n");
        
        // Create HTTP server
        fmt::print("Starting HTTP server...\n");
        
        net::io_context ioc{1};  // Single thread for now
        
        auto http_server = std::make_shared<api::HTTPServer>(
            ioc,
            config.server_host(),
            config.server_port(),
            true  // Enable CORS
        );
        
        // Register API routes
        api::APIRouter router(
            http_server,
            auth_service,
            db,
            order_service,
            trade_service,
            user_service
        );
        
        router.register_routes();
        
        // Start server
        http_server->run();

        fmt::print("Starting WebSocket server...\n");
        
        auto ws_server = std::make_shared<api::WebSocketServer>(
            ioc,
            config.server_host(),
            8081  // WebSocket on port 8081
        );
        
        ws_server->run();
        fmt::print("WebSocket server ready on ws://{}:{}\n\n", 
                  config.server_host(), 8081);
        
        // Create event broadcaster
        auto broadcaster = std::make_shared<api::EventBroadcaster>(ws_server);
        
        // CONNECT SERVICES TO BROADCASTER
        
        // Trade events -> WebSocket
        trade_service->set_trade_callback([broadcaster](const core::Trade& trade) {
            broadcaster->broadcast_trade(trade);
            // Also broadcast portfolio updates to both users
            broadcaster->broadcast_portfolio_update(trade.buyer_id);
            broadcaster->broadcast_portfolio_update(trade.seller_id);
        });
        
        // Order book events -> WebSocket
        order_service->set_orderbook_callback(
            [broadcaster](const std::string& card_id, const core::OrderBookSnapshot& snapshot) {
                broadcaster->broadcast_orderbook(card_id, snapshot);
            });
        
        // Order filled events -> WebSocket
        order_service->set_order_filled_callback(
            [broadcaster](const std::string& user_id, const std::string& order_id, 
                         const std::string& status, int filled_qty) {
                broadcaster->broadcast_order_filled(user_id, order_id, status, filled_qty);
            });
        
        fmt::print("Event broadcasting configured\n\n");
        
        fmt::print("\nHTTP Server ready on http://{}:{}\n", 
                  config.server_host(), config.server_port());
        fmt::print("WebSocket Server ready on ws://{}:{}\n", 
                  config.server_host(), 8081);
        fmt::print("Press Ctrl+C to stop\n\n");
        
        fmt::print("Available endpoints:\n");
        fmt::print("  POST   /api/auth/register    - Register new user\n");
        fmt::print("  POST   /api/auth/login       - Login user\n");
        fmt::print("  POST   /api/orders           - Place order\n");
        fmt::print("  DELETE /api/orders/:id       - Cancel order\n");
        fmt::print("  GET    /api/users/:id/portfolio - Get portfolio\n");
        fmt::print("  GET    /api/users/:id/orders - Get user orders\n");
        fmt::print("  GET    /api/cards/:id/orderbook - Get order book\n");
        fmt::print("  ...and more!\n\n");
        
        // Run the I/O service
        ioc.run();
        
    } catch (const std::exception& e) {
        fmt::print(stderr, "\nFatal error: {}\n", e.what());
        return 1;
    }
    
    return 0;
}