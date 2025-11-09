#include <gtest/gtest.h>
#include "services/order_service.hpp"
#include "services/trade_service.hpp"
#include "services/user_service.hpp"
#include "database/postgres_client.hpp"
#include <memory>

using namespace clash_trading;

class OrderServiceTest : public ::testing::Test {
protected:
    std::shared_ptr<database::PostgresClient> db;
    std::shared_ptr<services::TradeService> trade_service;
    std::shared_ptr<services::UserService> user_service;
    std::unique_ptr<services::OrderService> order_service;
    
    void SetUp() override {
        std::string connection_string = 
            "host=127.0.0.1 port=5432 dbname=clash_trading "
            "user=clash_user password=clash_pass_dev";
        
        try {
            db = std::make_shared<database::PostgresClient>(connection_string);
            
            if (!db->is_connected()) {
                GTEST_SKIP() << "Database not available. Skipping tests.";
                return;
            }
            
            // Create services
            trade_service = std::make_shared<services::TradeService>(db);
            user_service = std::make_shared<services::UserService>(db);
            order_service = std::make_unique<services::OrderService>(
                db, trade_service, user_service
            );
            
            // Clean up test data
            cleanup_test_data();
            
            // Create test users
            db->execute(
                "INSERT INTO users (user_id, username, email, password_hash, gold_balance) "
                "VALUES ('test-buyer-os', 'buyer_os', 'buyer_os@test.com', 'hash1', 100000)"
            );
            db->execute(
                "INSERT INTO users (user_id, username, email, password_hash, gold_balance) "
                "VALUES ('test-seller-os', 'seller_os', 'seller_os@test.com', 'hash2', 50000)"
            );
            
            // Create test card
            db->execute(
                "INSERT INTO cards (card_id, name, rarity, current_market_price) "
                "VALUES ('test-card-os', 'Order Test Card', 'COMMON', 1000.00)"
            );
            
            // Give seller some cards
            db->execute(
                "INSERT INTO user_inventory (user_id, card_id, quantity, avg_purchase_price) "
                "VALUES ('test-seller-os', 'test-card-os', 20, 800.00)"
            );
            
        } catch (const std::exception& e) {
            GTEST_SKIP() << "Database setup failed: " << e.what();
        }
    }
    
    void TearDown() override {
        if (!db || !db->is_connected()) {
            return;
        }
        
        try {
            cleanup_test_data();
        } catch (const std::exception& e) {
            // Ignore cleanup errors
        }
    }
    
    void cleanup_test_data() {
        db->execute("DELETE FROM trades WHERE trade_id LIKE 'test-%' OR trade_id LIKE 'trade_%'");
        db->execute("DELETE FROM orders WHERE user_id::text LIKE 'test-%'");
        db->execute("DELETE FROM user_inventory WHERE user_id::text LIKE 'test-%'");
        db->execute("DELETE FROM users WHERE user_id::text LIKE 'test-%'");
        db->execute("DELETE FROM cards WHERE card_id LIKE 'test-card-os%'");
    }
};

TEST_F(OrderServiceTest, PlaceLimitBuyOrder) {
    if (!db || !db->is_connected()) {
        GTEST_SKIP() << "Database not available";
    }
    
    services::PlaceOrderRequest request;
    request.user_id = "test-buyer-os";
    request.card_id = "test-card-os";
    request.type = core::OrderType::BUY;
    request.mode = core::OrderMode::LIMIT;
    request.price = 900.0;
    request.quantity = 5;
    
    auto response = order_service->place_order(request);
    
    // Verify response
    EXPECT_FALSE(response.order_id.empty());
    EXPECT_EQ(response.status, core::OrderStatus::PENDING); // No match yet
    EXPECT_EQ(response.filled_quantity, 0);
    EXPECT_EQ(response.trades.size(), 0);
    
    // Verify order in database
    auto order = order_service->get_order(response.order_id);
    ASSERT_TRUE(order.has_value());
    EXPECT_EQ(order->user_id, "test-buyer-os");
    EXPECT_EQ(order->card_id, "test-card-os");
    EXPECT_EQ(order->quantity, 5);
    EXPECT_EQ(order->status, core::OrderStatus::PENDING);
}

TEST_F(OrderServiceTest, PlaceLimitSellOrder) {
    if (!db || !db->is_connected()) {
        GTEST_SKIP() << "Database not available";
    }
    
    services::PlaceOrderRequest request;
    request.user_id = "test-seller-os";
    request.card_id = "test-card-os";
    request.type = core::OrderType::SELL;
    request.mode = core::OrderMode::LIMIT;
    request.price = 1100.0;
    request.quantity = 3;
    
    auto response = order_service->place_order(request);
    
    EXPECT_FALSE(response.order_id.empty());
    EXPECT_EQ(response.status, core::OrderStatus::PENDING);
    EXPECT_EQ(response.filled_quantity, 0);
}

TEST_F(OrderServiceTest, MatchingOrders) {
    if (!db || !db->is_connected()) {
        GTEST_SKIP() << "Database not available";
    }
    
    // Place sell order first
    services::PlaceOrderRequest sell_request;
    sell_request.user_id = "test-seller-os";
    sell_request.card_id = "test-card-os";
    sell_request.type = core::OrderType::SELL;
    sell_request.mode = core::OrderMode::LIMIT;
    sell_request.price = 1000.0;
    sell_request.quantity = 5;
    
    auto sell_response = order_service->place_order(sell_request);
    EXPECT_EQ(sell_response.status, core::OrderStatus::PENDING);
    
    // Place matching buy order
    services::PlaceOrderRequest buy_request;
    buy_request.user_id = "test-buyer-os";
    buy_request.card_id = "test-card-os";
    buy_request.type = core::OrderType::BUY;
    buy_request.mode = core::OrderMode::LIMIT;
    buy_request.price = 1000.0;
    buy_request.quantity = 5;
    
    auto buy_response = order_service->place_order(buy_request);
    
    // Verify buy order matched
    EXPECT_EQ(buy_response.status, core::OrderStatus::FILLED);
    EXPECT_EQ(buy_response.filled_quantity, 5);
    EXPECT_EQ(buy_response.trades.size(), 1);
    
    // Verify trade details
    const auto& trade = buy_response.trades[0];
    EXPECT_EQ(trade.buyer_id, "test-buyer-os");
    EXPECT_EQ(trade.seller_id, "test-seller-os");
    EXPECT_EQ(trade.quantity, 5);
    EXPECT_DOUBLE_EQ(trade.price, 1000.0);
    EXPECT_DOUBLE_EQ(trade.total_value, 5000.0);
    
    // Verify balances updated
    auto buyer = user_service->get_user("test-buyer-os");
    EXPECT_EQ(buyer->gold_balance, 95000); // 100000 - 5000
    
    auto seller = user_service->get_user("test-seller-os");
    EXPECT_EQ(seller->gold_balance, 55000); // 50000 + 5000
    
    // Verify inventory updated
    auto seller_portfolio = user_service->get_portfolio("test-seller-os");
    bool found = false;
    for (const auto& holding : seller_portfolio.holdings) {
        if (holding.card_id == "test-card-os") {
            EXPECT_EQ(holding.quantity, 15); // 20 - 5
            found = true;
        }
    }
    EXPECT_TRUE(found);
    
    auto buyer_portfolio = user_service->get_portfolio("test-buyer-os");
    found = false;
    for (const auto& holding : buyer_portfolio.holdings) {
        if (holding.card_id == "test-card-os") {
            EXPECT_EQ(holding.quantity, 5);
            found = true;
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(OrderServiceTest, PartialFill) {
    if (!db || !db->is_connected()) {
        GTEST_SKIP() << "Database not available";
    }
    
    // Place small sell order
    services::PlaceOrderRequest sell_request;
    sell_request.user_id = "test-seller-os";
    sell_request.card_id = "test-card-os";
    sell_request.type = core::OrderType::SELL;
    sell_request.mode = core::OrderMode::LIMIT;
    sell_request.price = 1000.0;
    sell_request.quantity = 3;
    
    order_service->place_order(sell_request);
    
    // Place larger buy order
    services::PlaceOrderRequest buy_request;
    buy_request.user_id = "test-buyer-os";
    buy_request.card_id = "test-card-os";
    buy_request.type = core::OrderType::BUY;
    buy_request.mode = core::OrderMode::LIMIT;
    buy_request.price = 1000.0;
    buy_request.quantity = 10;
    
    auto buy_response = order_service->place_order(buy_request);
    
    // Should be partially filled
    EXPECT_EQ(buy_response.status, core::OrderStatus::PARTIAL);
    EXPECT_EQ(buy_response.filled_quantity, 3);
    EXPECT_EQ(buy_response.trades.size(), 1);
}

TEST_F(OrderServiceTest, InsufficientBalance) {
    if (!db || !db->is_connected()) {
        GTEST_SKIP() << "Database not available";
    }
    
    services::PlaceOrderRequest request;
    request.user_id = "test-buyer-os";
    request.card_id = "test-card-os";
    request.type = core::OrderType::BUY;
    request.mode = core::OrderMode::LIMIT;
    request.price = 1000.0;
    request.quantity = 200; // Requires 200,000 gold, user only has 100,000
    
    EXPECT_THROW(
        order_service->place_order(request),
        services::OrderValidationException
    );
}

TEST_F(OrderServiceTest, InsufficientInventory) {
    if (!db || !db->is_connected()) {
        GTEST_SKIP() << "Database not available";
    }
    
    services::PlaceOrderRequest request;
    request.user_id = "test-seller-os";
    request.card_id = "test-card-os";
    request.type = core::OrderType::SELL;
    request.mode = core::OrderMode::LIMIT;
    request.price = 1000.0;
    request.quantity = 30; // User only has 20
    
    EXPECT_THROW(
        order_service->place_order(request),
        services::OrderValidationException
    );
}

TEST_F(OrderServiceTest, CancelOrder) {
    if (!db || !db->is_connected()) {
        GTEST_SKIP() << "Database not available";
    }
    
    // Place an order
    services::PlaceOrderRequest request;
    request.user_id = "test-buyer-os";
    request.card_id = "test-card-os";
    request.type = core::OrderType::BUY;
    request.mode = core::OrderMode::LIMIT;
    request.price = 900.0;
    request.quantity = 5;
    
    auto response = order_service->place_order(request);
    EXPECT_EQ(response.status, core::OrderStatus::PENDING);
    
    // Cancel the order
    bool cancelled = order_service->cancel_order(response.order_id, "test-buyer-os");
    EXPECT_TRUE(cancelled);
    
    // Verify order status updated
    auto order = order_service->get_order(response.order_id);
    ASSERT_TRUE(order.has_value());
    EXPECT_EQ(order->status, core::OrderStatus::CANCELLED);
}

TEST_F(OrderServiceTest, CancelOrderUnauthorized) {
    if (!db || !db->is_connected()) {
        GTEST_SKIP() << "Database not available";
    }
    
    // Place an order as buyer
    services::PlaceOrderRequest request;
    request.user_id = "test-buyer-os";
    request.card_id = "test-card-os";
    request.type = core::OrderType::BUY;
    request.mode = core::OrderMode::LIMIT;
    request.price = 900.0;
    request.quantity = 5;
    
    auto response = order_service->place_order(request);
    
    // Try to cancel as different user
    EXPECT_THROW(
        order_service->cancel_order(response.order_id, "test-seller-os"),
        std::runtime_error
    );
}

TEST_F(OrderServiceTest, GetUserOrders) {
    if (!db || !db->is_connected()) {
        GTEST_SKIP() << "Database not available";
    }
    
    // Place multiple orders
    for (int i = 0; i < 3; i++) {
        services::PlaceOrderRequest request;
        request.user_id = "test-buyer-os";
        request.card_id = "test-card-os";
        request.type = core::OrderType::BUY;
        request.mode = core::OrderMode::LIMIT;
        request.price = 900.0 + i * 10;
        request.quantity = 2;
        
        order_service->place_order(request);
    }
    
    auto orders = order_service->get_user_orders("test-buyer-os");
    
    EXPECT_GE(orders.size(), 3);
}

TEST_F(OrderServiceTest, GetOrderBookSnapshot) {
    if (!db || !db->is_connected()) {
        GTEST_SKIP() << "Database not available";
    }
    
    // Place some orders
    services::PlaceOrderRequest buy_request;
    buy_request.user_id = "test-buyer-os";
    buy_request.card_id = "test-card-os";
    buy_request.type = core::OrderType::BUY;
    buy_request.mode = core::OrderMode::LIMIT;
    buy_request.price = 950.0;
    buy_request.quantity = 5;
    order_service->place_order(buy_request);
    
    services::PlaceOrderRequest sell_request;
    sell_request.user_id = "test-seller-os";
    sell_request.card_id = "test-card-os";
    sell_request.type = core::OrderType::SELL;
    sell_request.mode = core::OrderMode::LIMIT;
    sell_request.price = 1050.0;
    sell_request.quantity = 3;
    order_service->place_order(sell_request);
    
    // Get snapshot
    auto snapshot = order_service->get_order_book_snapshot("test-card-os");
    
    EXPECT_EQ(snapshot.card_id, "test-card-os");
    EXPECT_GE(snapshot.bids.size(), 1);
    EXPECT_GE(snapshot.asks.size(), 1);
    
    // Verify bid
    EXPECT_DOUBLE_EQ(snapshot.bids[0].first, 950.0);
    EXPECT_EQ(snapshot.bids[0].second, 5);
    
    // Verify ask
    EXPECT_DOUBLE_EQ(snapshot.asks[0].first, 1050.0);
    EXPECT_EQ(snapshot.asks[0].second, 3);
}