#include "api/websocket.hpp"
#include "utils/logger.hpp"
#include <fmt/core.h>
#include <random>

namespace clash_trading {
namespace api {

// WebSocketSession Implementation
WebSocketSession::WebSocketSession(tcp::socket socket, WebSocketServer& server)
    : ws_(std::move(socket)), server_(server) {
    
    // Generate unique session ID
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 999999);
    session_id_ = "ws_" + std::to_string(dis(gen));
}

void WebSocketSession::run() {
    // Accept WebSocket handshake
    ws_.async_accept(
        [self = shared_from_this()](beast::error_code ec) {
            self->on_accept(ec);
        });
}

void WebSocketSession::on_accept(beast::error_code ec) {
    if (ec) {
        fmt::print(stderr, "WebSocket accept error: {}\n", ec.message());
        return;
    }
    
    fmt::print("✅ WebSocket client connected: {}\n", session_id_);
    
    // Start reading messages
    do_read();
}

void WebSocketSession::do_read() {
    ws_.async_read(buffer_,
        [self = shared_from_this()](beast::error_code ec, std::size_t bytes) {
            self->on_read(ec, bytes);
        });
}

void WebSocketSession::on_read(beast::error_code ec, std::size_t bytes_transferred) {
    boost::ignore_unused(bytes_transferred);
    
    if (ec == websocket::error::closed) {
        fmt::print("🔌 WebSocket client disconnected: {}\n", session_id_);
        server_.remove_session(shared_from_this());
        return;
    }
    
    if (ec) {
        fmt::print(stderr, "WebSocket read error: {}\n", ec.message());
        server_.remove_session(shared_from_this());
        return;
    }
    
    // Handle the message
    std::string message = beast::buffers_to_string(buffer_.data());
    buffer_.consume(buffer_.size());
    
    handle_message(message);
    
    // Continue reading
    do_read();
}

void WebSocketSession::handle_message(const std::string& message) {
    try {
        auto json = nlohmann::json::parse(message);
        
        std::string type = json["type"];
        
        if (type == "subscribe") {
            std::string channel = json["channel"];
            subscribe(channel);
            
            // Send confirmation
            nlohmann::json response = {
                {"type", "subscribed"},
                {"channel", channel}
            };
            send(response.dump());
            
        } else if (type == "unsubscribe") {
            std::string channel = json["channel"];
            unsubscribe(channel);
            
            // Send confirmation
            nlohmann::json response = {
                {"type", "unsubscribed"},
                {"channel", channel}
            };
            send(response.dump());
            
        } else if (type == "ping") {
            nlohmann::json response = {{"type", "pong"}};
            send(response.dump());
        }
        
    } catch (const std::exception& e) {
        fmt::print(stderr, "Error handling WebSocket message: {}\n", e.what());
    }
}

void WebSocketSession::send(const std::string& message) {
    ws_.async_write(net::buffer(message),
        [self = shared_from_this()](beast::error_code ec, std::size_t bytes) {
            if (ec) {
                fmt::print(stderr, "WebSocket write error: {}\n", ec.message());
            }
        });
}

void WebSocketSession::subscribe(const std::string& channel) {
    subscribed_channels_.insert(channel);
    server_.subscribe_session(shared_from_this(), channel);
    fmt::print("📺 Session {} subscribed to {}\n", session_id_, channel);
}

void WebSocketSession::unsubscribe(const std::string& channel) {
    subscribed_channels_.erase(channel);
    server_.unsubscribe_session(shared_from_this(), channel);
    fmt::print("📺 Session {} unsubscribed from {}\n", session_id_, channel);
}

// WebSocketServer Implementation
WebSocketServer::WebSocketServer(net::io_context& ioc, 
                                const std::string& address,
                                unsigned short port)
    : ioc_(ioc)
    , acceptor_(ioc, tcp::endpoint(net::ip::make_address(address), port)) {
    
    fmt::print("🔌 WebSocket Server listening on {}:{}\n", address, port);
}

void WebSocketServer::run() {
    do_accept();
}

void WebSocketServer::do_accept() {
    acceptor_.async_accept(
        [this](beast::error_code ec, tcp::socket socket) {
            on_accept(ec, std::move(socket));
        });
}

void WebSocketServer::on_accept(beast::error_code ec, tcp::socket socket) {
    if (ec) {
        fmt::print(stderr, "WebSocket accept error: {}\n", ec.message());
    } else {
        // Create new session and start it
        auto session = std::make_shared<WebSocketSession>(std::move(socket), *this);
        
        {
            std::lock_guard<std::mutex> lock(sessions_mutex_);
            sessions_.insert(session);
        }
        
        session->run();
    }
    
    // Accept next connection
    do_accept();
}

void WebSocketServer::subscribe_session(std::shared_ptr<WebSocketSession> session, 
                                        const std::string& channel) {
    std::lock_guard<std::mutex> lock(channels_mutex_);
    channels_[channel].insert(session);
}

void WebSocketServer::unsubscribe_session(std::shared_ptr<WebSocketSession> session, 
                                          const std::string& channel) {
    std::lock_guard<std::mutex> lock(channels_mutex_);
    
    auto it = channels_.find(channel);
    if (it != channels_.end()) {
        it->second.erase(session);
        
        // Remove channel if no subscribers
        if (it->second.empty()) {
            channels_.erase(it);
        }
    }
}

void WebSocketServer::remove_session(std::shared_ptr<WebSocketSession> session) {
    // Unsubscribe from all channels
    {
        std::lock_guard<std::mutex> lock(channels_mutex_);
        for (const auto& channel : session->get_channels()) {
            auto it = channels_.find(channel);
            if (it != channels_.end()) {
                it->second.erase(session);
                if (it->second.empty()) {
                    channels_.erase(it);
                }
            }
        }
    }
    
    // Remove from sessions list
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        sessions_.erase(session);
    }
}

void WebSocketServer::broadcast_to_channel(const std::string& channel,
                                           const nlohmann::json& message) {
    std::lock_guard<std::mutex> lock(channels_mutex_);

    auto it = channels_.find(channel);
    if (it != channels_.end()) {
        std::string msg_str = message.dump();
        LOG_DEBUG("[WS] channel={} subscribers={} payload_bytes={}",
                  channel, it->second.size(), msg_str.size());
        for (auto& session : it->second) {
            session->send(msg_str);
        }
    }
}

void WebSocketServer::broadcast_all(const nlohmann::json& message) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    
    std::string msg_str = message.dump();
    
    for (auto& session : sessions_) {
        session->send(msg_str);
    }
}

} // namespace api
} // namespace clash_trading