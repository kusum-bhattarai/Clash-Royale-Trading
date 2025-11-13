#pragma once

#include "api/websocket.hpp"
#include "core/order_book.hpp"
#include <nlohmann/json.hpp>
#include <memory>

namespace clash_trading {
namespace api {

// Helper class to broadcast trading events to WebSocket clients
class EventBroadcaster {
private:
    std::shared_ptr<WebSocketServer> ws_server_;
    
public:
    explicit EventBroadcaster(std::shared_ptr<WebSocketServer> ws_server)
        : ws_server_(ws_server) {}
    
    // Broadcast trade execution
    void broadcast_trade(const core::Trade& trade) {
        nlohmann::json message = {
            {"type", "trade_executed"},
            {"trade_id", trade.trade_id},
            {"card_id", trade.card_id},
            {"price", trade.price},
            {"quantity", trade.quantity},
            {"total_value", trade.total_value},
            {"buyer_id", trade.buyer_id},
            {"seller_id", trade.seller_id}
        };
        
        // Broadcast to card-specific channel
        ws_server_->broadcast_to_channel("trades:" + trade.card_id, message);
        
        // Broadcast to global trades channel
        ws_server_->broadcast_to_channel("trades:global", message);
    }
    
    // Broadcast order book update
    void broadcast_orderbook(const std::string& card_id, 
                            const core::OrderBookSnapshot& snapshot) {
        nlohmann::json bids = nlohmann::json::array();
        for (const auto& [price, quantity] : snapshot.bids) {
            bids.push_back({price, quantity});
        }
        
        nlohmann::json asks = nlohmann::json::array();
        for (const auto& [price, quantity] : snapshot.asks) {
            asks.push_back({price, quantity});
        }
        
        nlohmann::json message = {
            {"type", "orderbook_update"},
            {"card_id", card_id},
            {"bids", bids},
            {"asks", asks}
        };
        
        ws_server_->broadcast_to_channel("orderbook:" + card_id, message);
    }
    
    // Broadcast order filled notification to user
    void broadcast_order_filled(const std::string& user_id, 
                               const std::string& order_id,
                               const std::string& status,
                               int filled_quantity) {
        nlohmann::json message = {
            {"type", "order_filled"},
            {"order_id", order_id},
            {"status", status},
            {"filled_quantity", filled_quantity}
        };
        
        ws_server_->broadcast_to_channel("user:" + user_id, message);
    }
    
    // Broadcast portfolio update
    void broadcast_portfolio_update(const std::string& user_id) {
        nlohmann::json message = {
            {"type", "portfolio_update"},
            {"user_id", user_id}
        };
        
        ws_server_->broadcast_to_channel("user:" + user_id, message);
    }
};

} // namespace api
} // namespace clash_trading