#include <gtest/gtest.h>
#include "services/trade_service.hpp"
#include "database/postgres_client.hpp"
#include "services/price_aggregation_service.hpp"
#include "core/order_book.hpp"
#include <memory>

using namespace clash_trading;

class TradeServiceTest : public ::testing::Test {
protected:
    std::shared_ptr<database::PostgresClient> db;
    std::shared_ptr<services::PriceAggregationService> price_agg_service;
    std::unique_ptr<services::TradeService> trade_service;
    
    void SetUp() override {
        // Use the same connection string as other tests
        std::string connection_string = 
            "host=127.0.0.1 port=5432 dbname=clash_trading "
            "user=clash_user password=clash_pass_dev";
        
        try {
            db = std::make_shared<database::PostgresClient>(connection_string);
            
            if (!db->is_connected()) {
                GTEST_SKIP() << "Database not available. Skipping tests.";
                return;
            }
            
            price_agg_service = std::make_shared<services::PriceAggregationService>(db);
            trade_service = std::make_unique<services::TradeService>(db, price_agg_service);
            
            // Clean up test data
            db->execute("DELETE FROM trades WHERE trade_id LIKE 'test-%'");
            db->execute("DELETE FROM user_inventory WHERE user_id LIKE 'test-%'");
            db->execute("DELETE FROM users WHERE user_id LIKE 'test-%'");
            
            // Create test users with balances
            db->execute(
                "INSERT INTO users (user_id, username, email, password_hash, gold_balance) "
                "VALUES ('test-buyer', 'buyer', 'buyer@test.com', 'hash1', 100000)"
            );
            db->execute(
                "INSERT INTO users (user_id, username, email, password_hash, gold_balance) "
                "VALUES ('test-seller', 'seller', 'seller@test.com', 'hash2', 50000)"
            );
            
            // Give seller some cards
            db->execute(
                "INSERT INTO user_inventory (user_id, card_id, quantity) "
                "VALUES ('test-seller', 'mega_knight', 10)"
            );
        } catch (const std::exception& e) {
            GTEST_SKIP() << "Database setup failed: " << e.what() 
                         << "\nMake sure PostgreSQL is running with docker-compose up";
        }
    }
    
    void TearDown() override {
        if (!db || !db->is_connected()) {
            return;
        }
        
        try {
            // Clean up test data
            db->execute("DELETE FROM trades WHERE trade_id LIKE 'test-%'");
            db->execute("DELETE FROM user_inventory WHERE user_id LIKE 'test-%'");
            db->execute("DELETE FROM users WHERE user_id LIKE 'test-%'");
        } catch (const std::exception& e) {
            // Ignore cleanup errors
        }
    }
};

TEST_F(TradeServiceTest, ExecuteTradeSuccess) {
    if (!db || !db->is_connected()) {
        GTEST_SKIP() << "Database not available";
    }
    
    core::Trade trade;
    trade.trade_id = "test-trade-1";
    trade.card_id = "mega_knight";
    trade.buyer_id = "test-buyer";
    trade.seller_id = "test-seller";
    trade.price = 1000.0;
    trade.quantity = 2;
    trade.total_value = 2000.0;
    trade.buyer_order_id = "buy-order-1";
    trade.seller_order_id = "sell-order-1";
    trade.merkle_hash = trade.calculate_merkle_hash();
    
    // Execute trade
    ASSERT_NO_THROW(trade_service->execute_trade(trade));
    
    // Verify balances updated
    auto buyer_result = db->execute(
        "SELECT gold_balance FROM users WHERE user_id = 'test-buyer'"
    );
    EXPECT_EQ(buyer_result[0][0].as<int64_t>(), 98000); // 100000 - 2000
    
    auto seller_result = db->execute(
        "SELECT gold_balance FROM users WHERE user_id = 'test-seller'"
    );
    EXPECT_EQ(seller_result[0][0].as<int64_t>(), 52000); // 50000 + 2000
    
    // Verify inventory updated
    auto seller_inv = db->execute(
        "SELECT quantity FROM user_inventory "
        "WHERE user_id = 'test-seller' AND card_id = 'mega_knight'"
    );
    EXPECT_EQ(seller_inv[0][0].as<int>(), 8); // 10 - 2
    
    auto buyer_inv = db->execute(
        "SELECT quantity FROM user_inventory "
        "WHERE user_id = 'test-buyer' AND card_id = 'mega_knight'"
    );
    EXPECT_EQ(buyer_inv[0][0].as<int>(), 2);
}

TEST_F(TradeServiceTest, InsufficientBalance) {
    if (!db || !db->is_connected()) {
        GTEST_SKIP() << "Database not available";
    }
    
    core::Trade trade;
    trade.trade_id = "test-trade-2";
    trade.card_id = "mega_knight";
    trade.buyer_id = "test-buyer";
    trade.seller_id = "test-seller";
    trade.price = 200000.0; // More than buyer has!
    trade.quantity = 1;
    trade.total_value = 200000.0;
    trade.buyer_order_id = "buy-order-1";
    trade.seller_order_id = "sell-order-1";
    trade.merkle_hash = trade.calculate_merkle_hash();
    
    // Should throw exception
    EXPECT_THROW(
        trade_service->execute_trade(trade), 
        services::TradeExecutionException
    );
    
    // Verify balances unchanged (transaction rolled back)
    auto buyer_result = db->execute(
        "SELECT gold_balance FROM users WHERE user_id = 'test-buyer'"
    );
    EXPECT_EQ(buyer_result[0][0].as<int64_t>(), 100000);
}

TEST_F(TradeServiceTest, InsufficientInventory) {
    if (!db || !db->is_connected()) {
        GTEST_SKIP() << "Database not available";
    }
    
    core::Trade trade;
    trade.trade_id = "test-trade-3";
    trade.card_id = "mega_knight";
    trade.buyer_id = "test-buyer";
    trade.seller_id = "test-seller";
    trade.price = 1000.0;
    trade.quantity = 20; // Seller only has 10!
    trade.total_value = 20000.0;
    trade.buyer_order_id = "buy-order-1";
    trade.seller_order_id = "sell-order-1";
    trade.merkle_hash = trade.calculate_merkle_hash();
    
    // Should throw exception
    EXPECT_THROW(
        trade_service->execute_trade(trade), 
        services::TradeExecutionException
    );
    
    // Verify inventory unchanged (transaction rolled back)
    auto seller_inv = db->execute(
        "SELECT quantity FROM user_inventory "
        "WHERE user_id = 'test-seller' AND card_id = 'mega_knight'"
    );
    EXPECT_EQ(seller_inv[0][0].as<int>(), 10);
}

TEST_F(TradeServiceTest, MerkleHashVerification) {
    if (!db || !db->is_connected()) {
        GTEST_SKIP() << "Database not available";
    }
    
    core::Trade trade;
    trade.trade_id = "test-trade-4";
    trade.card_id = "mega_knight";
    trade.buyer_id = "test-buyer";
    trade.seller_id = "test-seller";
    trade.price = 1000.0;
    trade.quantity = 2;
    trade.total_value = 2000.0;
    trade.buyer_order_id = "buy-order-1";
    trade.seller_order_id = "sell-order-1";
    trade.merkle_hash = trade.calculate_merkle_hash();
    
    // Verify hash integrity before execution
    EXPECT_TRUE(trade.verify_integrity());
    
    // Tamper with trade
    trade.price = 2000.0;
    EXPECT_FALSE(trade.verify_integrity());
}

TEST_F(TradeServiceTest, BatchTradeExecution) {
    if (!db || !db->is_connected()) {
        GTEST_SKIP() << "Database not available";
    }
    
    std::vector<core::Trade> trades;
    
    for (int i = 0; i < 3; i++) {
        core::Trade trade;
        trade.trade_id = "test-batch-" + std::to_string(i);
        trade.card_id = "mega_knight";
        trade.buyer_id = "test-buyer";
        trade.seller_id = "test-seller";
        trade.price = 1000.0;
        trade.quantity = 1;
        trade.total_value = 1000.0;
        trade.buyer_order_id = "buy-order-" + std::to_string(i);
        trade.seller_order_id = "sell-order-" + std::to_string(i);
        trade.merkle_hash = trade.calculate_merkle_hash();
        trades.push_back(trade);
    }
    
    ASSERT_NO_THROW(trade_service->execute_trades(trades));
    
    // Verify all 3 trades executed atomically
    auto buyer_result = db->execute(
        "SELECT gold_balance FROM users WHERE user_id = 'test-buyer'"
    );
    EXPECT_EQ(buyer_result[0][0].as<int64_t>(), 97000); // 100000 - 3000
    
    auto seller_inv = db->execute(
        "SELECT quantity FROM user_inventory "
        "WHERE user_id = 'test-seller' AND card_id = 'mega_knight'"
    );
    EXPECT_EQ(seller_inv[0][0].as<int>(), 7); // 10 - 3
}