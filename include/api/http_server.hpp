#pragma once

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio.hpp>
#include <memory>
#include <string>
#include <functional>
#include <unordered_map>

namespace clash_trading {
namespace api {

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = net::ip::tcp;

// Type aliases for HTTP
using http_request = http::request<http::string_body>;
using http_response = http::response<http::string_body>;

// Forward declarations
class HTTPServer;

// HTTP session handles a single client connection
class HTTPSession : public std::enable_shared_from_this<HTTPSession> {
private:
    tcp::socket socket_;
    beast::flat_buffer buffer_;
    http_request request_;
    http_response response_;
    HTTPServer& server_;
    
public:
    HTTPSession(tcp::socket socket, HTTPServer& server);
    
    void run();
    
private:
    void do_read();
    void on_read(beast::error_code ec, std::size_t bytes_transferred);
    void handle_request();
    void do_write();
    void on_write(beast::error_code ec, std::size_t bytes_transferred, bool close);
};

// Route handler function type
using RouteHandler = std::function<void(const http_request&, http_response&)>;

// HTTP Server using Boost.Beast
class HTTPServer {
private:
    net::io_context& ioc_;
    tcp::acceptor acceptor_;
    
    // Route registry: "METHOD /path" -> handler
    std::unordered_map<std::string, RouteHandler> routes_;
    
    // CORS configuration
    bool enable_cors_;
    std::string allowed_origins_;
    
public:
    HTTPServer(net::io_context& ioc, 
              const std::string& address,
              unsigned short port,
              bool enable_cors = true);
    
    // Start accepting connections
    void run();
    
    // Register a route handler
    void register_route(http::verb method, 
                       const std::string& path, 
                       RouteHandler handler);
    
    // Handle incoming request (called by HTTPSession)
    void handle_request(const http_request& req, http_response& res);
    
private:
    void do_accept();
    void on_accept(beast::error_code ec, tcp::socket socket);
    
    // Extract route key from request
    std::string get_route_key(http::verb method, const std::string& path) const;
    
    // Add CORS headers to response
    void add_cors_headers(http_response& res);
    
    // Create error response
    http_response create_error_response(http::status status, 
                                       const std::string& message);
};

} // namespace api
} // namespace clash_trading