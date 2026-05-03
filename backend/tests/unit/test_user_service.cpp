#include <gtest/gtest.h>
#include "services/user_service.hpp"
#include "database/postgres_client.hpp"
#include <memory>

using namespace clash_trading;

class UserServiceTest : public ::testing::Test {
protected:
    std::shared_ptr<database::PostgresClient> db;
    std::unique_ptr<services::UserService> user_service;
    
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
            
            user_service = std::make_unique<services::UserService>(db);
            
            // Clean up test data
            db->execute("DELETE FROM trades WHERE buyer_id LIKE 'test-user-%' OR seller_id LIKE 'test-user-%'");
            db->execute("DELETE FROM user_inventory WHERE user_id LIKE 'test-user-%'");
            db->execute("DELETE FROM users WHERE user_id LIKE 'test-user-%'");
            db->execute("DELETE FROM cards WHERE card_id LIKE 'test-card-%'");
            
            // Create test user
            db->execute(
                "INSERT INTO users (user_id, username, email, password_hash, gold_balance, trader_level, total_trades) "
                "VALUES ('test-user-1', 'testuser', 'test@example.com', 'hash123', 50000, 'CHALLENGER', 0)"
            );
            
            // Create test cards
            db->execute(
                "INSERT INTO cards (card_id, name, rarity, current_market_price) "
                "VALUES ('test-card-1', 'Test Knight', 'COMMON', 1000.00)"
            );
            db->execute(
                "INSERT INTO cards (card_id, name, rarity, current_market_price) "
                "VALUES ('test-card-2', 'Test Wizard', 'RARE', 2000.00)"
            );
            
            // Give user some cards with purchase prices
            db->execute(
                "INSERT INTO user_inventory (user_id, card_id, quantity, avg_purchase_price) "
                "VALUES ('test-user-1', 'test-card-1', 10, 800.00)"
            );
            db->execute(
                "INSERT INTO user_inventory (user_id, card_id, quantity, avg_purchase_price) "
                "VALUES ('test-user-1', 'test-card-2', 5, 1800.00)"
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
            db->execute("DELETE FROM trades WHERE buyer_id LIKE 'test-user-%' OR seller_id LIKE 'test-user-%'");
            db->execute("DELETE FROM user_inventory WHERE user_id LIKE 'test-user-%'");
            db->execute("DELETE FROM users WHERE user_id LIKE 'test-user-%'");
            db->execute("DELETE FROM cards WHERE card_id LIKE 'test-card-%'");
        } catch (const std::exception& e) {
            // Ignore cleanup errors
        }
    }
};

TEST_F(UserServiceTest, GetUser) {
    if (!db || !db->is_connected()) {
        GTEST_SKIP() << "Database not available";
    }
    
    auto user = user_service->get_user("test-user-1");
    
    ASSERT_TRUE(user.has_value());
    EXPECT_EQ(user->user_id, "test-user-1");
    EXPECT_EQ(user->username, "testuser");
    EXPECT_EQ(user->email, "test@example.com");
    EXPECT_EQ(user->gold_balance, 50000);
    EXPECT_EQ(user->trader_level, "CHALLENGER");
    EXPECT_EQ(user->total_trades, 0);
}

TEST_F(UserServiceTest, GetUserNotFound) {
    if (!db || !db->is_connected()) {
        GTEST_SKIP() << "Database not available";
    }
    
    auto user = user_service->get_user("nonexistent-user");
    
    EXPECT_FALSE(user.has_value());
}

TEST_F(UserServiceTest, GetPortfolio) {
    if (!db || !db->is_connected()) {
        GTEST_SKIP() << "Database not available";
    }
    
    auto portfolio = user_service->get_portfolio("test-user-1");
    
    // Verify basic info
    EXPECT_EQ(portfolio.user_id, "test-user-1");
    EXPECT_EQ(portfolio.gold_balance, 50000);
    
    // Verify holdings
    ASSERT_EQ(portfolio.holdings.size(), 2);
    
    // Check first holding (Test Knight)
    const auto& holding1 = portfolio.holdings[0];
    EXPECT_EQ(holding1.card_id, "test-card-1");
    EXPECT_EQ(holding1.card_name, "Test Knight");
    EXPECT_EQ(holding1.quantity, 10);
    EXPECT_DOUBLE_EQ(holding1.avg_purchase_price, 800.00);
    EXPECT_DOUBLE_EQ(holding1.current_market_price, 1000.00);
    EXPECT_DOUBLE_EQ(holding1.total_value, 10000.00); // 10 * 1000
    EXPECT_DOUBLE_EQ(holding1.unrealized_pnl, 2000.00); // (1000 - 800) * 10
    EXPECT_DOUBLE_EQ(holding1.unrealized_pnl_percent, 25.0); // 2000 / 8000 * 100
    
    // Check second holding (Test Wizard)
    const auto& holding2 = portfolio.holdings[1];
    EXPECT_EQ(holding2.card_id, "test-card-2");
    EXPECT_EQ(holding2.card_name, "Test Wizard");
    EXPECT_EQ(holding2.quantity, 5);
    EXPECT_DOUBLE_EQ(holding2.avg_purchase_price, 1800.00);
    EXPECT_DOUBLE_EQ(holding2.current_market_price, 2000.00);
    EXPECT_DOUBLE_EQ(holding2.total_value, 10000.00); // 5 * 2000
    EXPECT_DOUBLE_EQ(holding2.unrealized_pnl, 1000.00); // (2000 - 1800) * 5
    EXPECT_NEAR(holding2.unrealized_pnl_percent, 11.11, 0.01); // 1000 / 9000 * 100
    
    // Verify total calculations
    EXPECT_DOUBLE_EQ(portfolio.total_card_value, 20000.00); // 10000 + 10000
    EXPECT_DOUBLE_EQ(portfolio.total_portfolio_value, 70000.00); // 50000 + 20000
}

TEST_F(UserServiceTest, GetPortfolioEmptyInventory) {
    if (!db || !db->is_connected()) {
        GTEST_SKIP() << "Database not available";
    }
    
    // Create user with no inventory
    db->execute(
        "INSERT INTO users (user_id, username, email, password_hash, gold_balance) "
        "VALUES ('test-user-empty', 'emptyuser', 'empty@example.com', 'hash', 100000)"
    );
    
    auto portfolio = user_service->get_portfolio("test-user-empty");
    
    EXPECT_EQ(portfolio.user_id, "test-user-empty");
    EXPECT_EQ(portfolio.gold_balance, 100000);
    EXPECT_EQ(portfolio.holdings.size(), 0);
    EXPECT_DOUBLE_EQ(portfolio.total_card_value, 0.0);
    EXPECT_DOUBLE_EQ(portfolio.total_portfolio_value, 100000.0);
    
    // Cleanup
    db->execute("DELETE FROM users WHERE user_id = 'test-user-empty'");
}

TEST_F(UserServiceTest, GetTradingStatsNoTrades) {
    if (!db || !db->is_connected()) {
        GTEST_SKIP() << "Database not available";
    }
    
    auto stats = user_service->get_trading_stats("test-user-1");
    
    EXPECT_EQ(stats.total_trades, 0);
    EXPECT_EQ(stats.total_volume, 0);
    EXPECT_EQ(stats.buy_count, 0);
    EXPECT_EQ(stats.sell_count, 0);
    EXPECT_EQ(stats.trader_level, "CHALLENGER");
}

TEST_F(UserServiceTest, GetTradingStatsWithTrades) {
    if (!db || !db->is_connected()) {
        GTEST_SKIP() << "Database not available";
    }
    
    // Create another test user for trading
    db->execute(
        "INSERT INTO users (user_id, username, email, password_hash, gold_balance) "
        "VALUES ('test-user-2', 'trader2', 'trader2@example.com', 'hash', 50000)"
    );
    
    // Create some trades where test-user-1 is buyer
    db->execute(
        "INSERT INTO trades (trade_id, card_id, buyer_id, seller_id, price, quantity, total_value, merkle_hash) "
        "VALUES ('test-trade-1', 'test-card-1', 'test-user-1', 'test-user-2', 1000, 5, 5000, 'hash1')"
    );
    db->execute(
        "INSERT INTO trades (trade_id, card_id, buyer_id, seller_id, price, quantity, total_value, merkle_hash) "
        "VALUES ('test-trade-2', 'test-card-2', 'test-user-1', 'test-user-2', 2000, 3, 6000, 'hash2')"
    );
    
    // Create a trade where test-user-1 is seller
    db->execute(
        "INSERT INTO trades (trade_id, card_id, buyer_id, seller_id, price, quantity, total_value, merkle_hash) "
        "VALUES ('test-trade-3', 'test-card-1', 'test-user-2', 'test-user-1', 1000, 2, 2000, 'hash3')"
    );
    
    auto stats = user_service->get_trading_stats("test-user-1");
    
    EXPECT_EQ(stats.total_trades, 3);
    EXPECT_EQ(stats.total_volume, 13000); // 5000 + 6000 + 2000
    EXPECT_EQ(stats.buy_count, 2);
    EXPECT_EQ(stats.sell_count, 1);
    EXPECT_EQ(stats.trader_level, "CHALLENGER");
    
    // Cleanup
    db->execute("DELETE FROM users WHERE user_id = 'test-user-2'");
}

TEST_F(UserServiceTest, ComputeRankFromXP) {
    EXPECT_EQ(services::UserService::compute_rank_from_xp(0),     "Goblin Stadium");
    EXPECT_EQ(services::UserService::compute_rank_from_xp(50),    "Goblin Stadium");
    EXPECT_EQ(services::UserService::compute_rank_from_xp(99),    "Goblin Stadium");

    EXPECT_EQ(services::UserService::compute_rank_from_xp(100),   "Challenger");
    EXPECT_EQ(services::UserService::compute_rank_from_xp(300),   "Challenger");
    EXPECT_EQ(services::UserService::compute_rank_from_xp(499),   "Challenger");

    EXPECT_EQ(services::UserService::compute_rank_from_xp(500),   "Master");
    EXPECT_EQ(services::UserService::compute_rank_from_xp(1000),  "Master");
    EXPECT_EQ(services::UserService::compute_rank_from_xp(1999),  "Master");

    EXPECT_EQ(services::UserService::compute_rank_from_xp(2000),  "Grand Champion");
    EXPECT_EQ(services::UserService::compute_rank_from_xp(5000),  "Grand Champion");
    EXPECT_EQ(services::UserService::compute_rank_from_xp(9999),  "Grand Champion");

    EXPECT_EQ(services::UserService::compute_rank_from_xp(10000), "Ultimate Champion");
    EXPECT_EQ(services::UserService::compute_rank_from_xp(50000), "Ultimate Champion");
}

TEST_F(UserServiceTest, PortfolioWithNullPrices) {
    if (!db || !db->is_connected()) {
        GTEST_SKIP() << "Database not available";
    }
    
    // Create card with NULL market price
    db->execute(
        "INSERT INTO cards (card_id, name, rarity, current_market_price) "
        "VALUES ('test-card-null', 'Unpriced Card', 'COMMON', NULL)"
    );
    
    // Add to user inventory with NULL avg_purchase_price
    db->execute(
        "INSERT INTO user_inventory (user_id, card_id, quantity, avg_purchase_price) "
        "VALUES ('test-user-1', 'test-card-null', 3, NULL)"
    );
    
    auto portfolio = user_service->get_portfolio("test-user-1");
    
    // Should handle NULLs gracefully
    bool found = false;
    for (const auto& holding : portfolio.holdings) {
        if (holding.card_id == "test-card-null") {
            found = true;
            EXPECT_EQ(holding.quantity, 3);
            EXPECT_DOUBLE_EQ(holding.avg_purchase_price, 0.0);
            EXPECT_DOUBLE_EQ(holding.current_market_price, 0.0);
            EXPECT_DOUBLE_EQ(holding.total_value, 0.0);
        }
    }
    EXPECT_TRUE(found);
    
    // Cleanup
    db->execute("DELETE FROM user_inventory WHERE card_id = 'test-card-null'");
    db->execute("DELETE FROM cards WHERE card_id = 'test-card-null'");
}