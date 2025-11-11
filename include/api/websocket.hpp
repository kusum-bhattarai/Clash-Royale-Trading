#pragma once

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio.hpp>
#include <memory>
#include <string>
#include <set>
#include <unordered_map>
#include <mutex>
#include <nlohmann/json.hpp>

namespace clash_trading {
namespace api {

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = net::ip::tcp;

// Forward declaration
class WebSocketServer;

// WebSocket session represents a single client connection
class WebSocketSession : public std::enable_shared_from_this<WebSocketSession> {
private:
    websocket::stream<tcp::socket> ws_;
    beast::flat_buffer buffer_;
    std::set<std::string> subscribed_channels_;
    WebSocketServer& server_;
    std::string session_id_;
    
public:
    WebSocketSession(tcp::socket socket, WebSocketServer& server);
    
    void run();
    void send(const std::string& message);
    void subscribe(const std::string& channel);
    void unsubscribe(const std::string& channel);
    const std::set<std::string>& get_channels() const { return subscribed_channels_; }
    const std::string& get_id() const { return session_id_; }
    
private:
    void on_accept(beast::error_code ec);
    void do_read();
    void on_read(beast::error_code ec, std::size_t bytes_transferred);
    void handle_message(const std::string& message);
    void do_write(const std::string& message);
};

// WebSocket Server manages all active sessions
class WebSocketServer {
private:
    net::io_context& ioc_;
    tcp::acceptor acceptor_;
    
    // Channel -> Sessions mapping
    std::unordered_map<std::string, std::set<std::shared_ptr<WebSocketSession>>> channels_;
    std::mutex channels_mutex_;
    
    // All active sessions
    std::set<std::shared_ptr<WebSocketSession>> sessions_;
    std::mutex sessions_mutex_;
    
public:
    WebSocketServer(net::io_context& ioc, 
                   const std::string& address,
                   unsigned short port);
    
    void run();
    
    // Subscribe session to a channel
    void subscribe_session(std::shared_ptr<WebSocketSession> session, 
                          const std::string& channel);
    
    // Unsubscribe session from a channel
    void unsubscribe_session(std::shared_ptr<WebSocketSession> session, 
                            const std::string& channel);
    
    // Remove session completely (on disconnect)
    void remove_session(std::shared_ptr<WebSocketSession> session);
    
    // Broadcast message to all subscribers of a channel
    void broadcast_to_channel(const std::string& channel, 
                             const nlohmann::json& message);
    
    // Broadcast to all connected clients
    void broadcast_all(const nlohmann::json& message);
    
private:
    void do_accept();
    void on_accept(beast::error_code ec, tcp::socket socket);
};

} // namespace api
} // namespace clash_trading