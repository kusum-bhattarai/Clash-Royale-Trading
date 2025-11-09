#include "api/http_server.hpp"
#include <iostream>
#include <fmt/core.h>

namespace clash_trading {
namespace api {

HTTPSession::HTTPSession(tcp::socket socket, HTTPServer& server)
    : socket_(std::move(socket)), server_(server) {}

void HTTPSession::run() {
    do_read();
}

void HTTPSession::do_read() {
    request_ = {};  // Clear previous request
    
    http::async_read(socket_, buffer_, request_,
        [self = shared_from_this()](beast::error_code ec, std::size_t bytes) {
            self->on_read(ec, bytes);
        });
}

void HTTPSession::on_read(beast::error_code ec, std::size_t bytes_transferred) {
    boost::ignore_unused(bytes_transferred);
    
    if (ec == http::error::end_of_stream) {
        socket_.shutdown(tcp::socket::shutdown_send, ec);
        return;
    }
    
    if (ec) {
        fmt::print(stderr, "Read error: {}\n", ec.message());
        return;
    }
    
    // Handle the request
    handle_request();
    
    // Send response
    do_write();
}

void HTTPSession::handle_request() {
    try {
        server_.handle_request(request_, response_);
    } catch (const std::exception& e) {
        // Internal server error
        response_.result(http::status::internal_server_error);
        response_.set(http::field::content_type, "application/json");
        response_.body() = R"({"error": "Internal server error"})";
        response_.prepare_payload();
    }
}

void HTTPSession::do_write() {
    bool close = response_.need_eof();
    
    http::async_write(socket_, response_,
        [self = shared_from_this(), close](beast::error_code ec, std::size_t bytes) {
            self->on_write(ec, bytes, close);
        });
}

void HTTPSession::on_write(beast::error_code ec, std::size_t bytes_transferred, bool close) {
    boost::ignore_unused(bytes_transferred);
    
    if (ec) {
        fmt::print(stderr, "Write error: {}\n", ec.message());
        return;
    }
    
    if (close) {
        socket_.shutdown(tcp::socket::shutdown_send, ec);
        return;
    }
    
    // Read next request
    do_read();
}

HTTPServer::HTTPServer(net::io_context& ioc, 
                      const std::string& address,
                      unsigned short port,
                      bool enable_cors)
    : ioc_(ioc)
    , acceptor_(ioc, tcp::endpoint(net::ip::make_address(address), port))
    , enable_cors_(enable_cors)
    , allowed_origins_("*") {
    
    fmt::print("🌐 HTTP Server listening on {}:{}\n", address, port);
}

void HTTPServer::run() {
    do_accept();
}

void HTTPServer::do_accept() {
    acceptor_.async_accept(
        [this](beast::error_code ec, tcp::socket socket) {
            on_accept(ec, std::move(socket));
        });
}

void HTTPServer::on_accept(beast::error_code ec, tcp::socket socket) {
    if (ec) {
        fmt::print(stderr, "Accept error: {}\n", ec.message());
    } else {
        // Create session and run it
        std::make_shared<HTTPSession>(std::move(socket), *this)->run();
    }
    
    // Accept next connection
    do_accept();
}

void HTTPServer::register_route(http::verb method, 
                               const std::string& path, 
                               RouteHandler handler) {
    std::string key = get_route_key(method, path);
    routes_[key] = handler;
    fmt::print("Registered route: {} {}\n", std::string(http::to_string(method)), path);
}

void HTTPServer::handle_request(const http_request& req, http_response& res) {
    // Handle OPTIONS for CORS preflight
    if (req.method() == http::verb::options) {
        res.result(http::status::ok);
        add_cors_headers(res);
        res.prepare_payload();
        return;
    }
    
    // Find route handler
    std::string route_key = get_route_key(req.method(), std::string(req.target()));
    
    auto it = routes_.find(route_key);
    if (it != routes_.end()) {
        // Call route handler
        it->second(req, res);
        
        // Add CORS headers
        if (enable_cors_) {
            add_cors_headers(res);
        }
        
        res.prepare_payload();
    } else {
        // 404 Not Found
        res.result(http::status::not_found);
        res.set(http::field::content_type, "application/json");
        res.body() = R"({"error": "Route not found"})";
        
        if (enable_cors_) {
            add_cors_headers(res);
        }
        
        res.prepare_payload();
    }
}

std::string HTTPServer::get_route_key(http::verb method, const std::string& path) const {
    // Extract just the path (remove query parameters)
    size_t query_pos = path.find('?');
    std::string clean_path = (query_pos != std::string::npos) 
        ? path.substr(0, query_pos) 
        : path;
    
    return std::string(http::to_string(method)) + " " + clean_path;
}

void HTTPServer::add_cors_headers(http_response& res) {
    res.set(http::field::access_control_allow_origin, allowed_origins_);
    res.set(http::field::access_control_allow_methods, "GET, POST, PUT, DELETE, OPTIONS");
    res.set(http::field::access_control_allow_headers, "Content-Type, Authorization");
}

http_response HTTPServer::create_error_response(http::status status, 
                                               const std::string& message) {
    http_response res{status, 11};
    res.set(http::field::content_type, "application/json");
    res.body() = R"({"error": ")" + message + R"("})";
    return res;
}

} // namespace api
} // namespace clash_trading