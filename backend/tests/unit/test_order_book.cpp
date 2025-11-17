#include <gtest/gtest.h>
#include "core/order_book.hpp"

using namespace clash_trading::core;

// Helper to create an order
Order create_order(const std::string& order_id, const std::string& user_id,
                   OrderType type, OrderMode mode, double price, int quantity) {
    Order order;
    order.order_id = order_id;
    order.user_id = user_id;
    order.card_id = "mega_knight";
    order.type = type;
    order.mode = mode;
    order.price = price;
    order.quantity = quantity;
    order.filled_quantity = 0;
    order.status = OrderStatus::PENDING;
    return order;
}

TEST(OrderBookTest, Construction) {
    OrderBook book("mega_knight");
    EXPECT_EQ(book.get_best_bid(), 0.0);
    EXPECT_EQ(book.get_best_ask(), 0.0);
    EXPECT_EQ(book.get_order_count(), 0);
}

TEST(OrderBookTest, MarketBuyMatchesSell) {
    OrderBook book("mega_knight");
    
    // Add a sell order at $1250
    Order sell = create_order("sell1", "user1", OrderType::SELL, OrderMode::LIMIT, 1250.0, 10);
    book.match_order(sell);
    
    // Market buy should match
    Order buy = create_order("buy1", "user2", OrderType::BUY, OrderMode::MARKET, 0.0, 10);
    auto trades = book.match_order(buy);
    
    ASSERT_EQ(trades.size(), 1);
    EXPECT_EQ(trades[0].price, 1250.0);
    EXPECT_EQ(trades[0].quantity, 10);
    EXPECT_TRUE(buy.is_filled());
}

TEST(OrderBookTest, LimitBuyMatchesBetterSell) {
    OrderBook book("mega_knight");
    
    // Add sell at $1200
    Order sell = create_order("sell1", "user1", OrderType::SELL, OrderMode::LIMIT, 1200.0, 10);
    book.match_order(sell);
    
    // Buy willing to pay $1250 should match at $1200
    Order buy = create_order("buy1", "user2", OrderType::BUY, OrderMode::LIMIT, 1250.0, 10);
    auto trades = book.match_order(buy);
    
    ASSERT_EQ(trades.size(), 1);
    EXPECT_EQ(trades[0].price, 1200.0);  // Executes at sell price
    EXPECT_TRUE(buy.is_filled());
}

TEST(OrderBookTest, LimitBuyNoMatchWorseSell) {
    OrderBook book("mega_knight");
    
    // Add sell at $1300
    Order sell = create_order("sell1", "user1", OrderType::SELL, OrderMode::LIMIT, 1300.0, 10);
    book.match_order(sell);
    
    // Buy at $1250 won't match
    Order buy = create_order("buy1", "user2", OrderType::BUY, OrderMode::LIMIT, 1250.0, 10);
    auto trades = book.match_order(buy);
    
    EXPECT_EQ(trades.size(), 0);
    EXPECT_FALSE(buy.is_filled());
    EXPECT_EQ(book.get_order_count(), 2);  // Both sitting in book
}

TEST(OrderBookTest, PartialFill) {
    OrderBook book("mega_knight");
    
    // Sell 5 cards
    Order sell = create_order("sell1", "user1", OrderType::SELL, OrderMode::LIMIT, 1250.0, 5);
    book.match_order(sell);
    
    // Buy 10 cards (only 5 available)
    Order buy = create_order("buy1", "user2", OrderType::BUY, OrderMode::MARKET, 0.0, 10);
    auto trades = book.match_order(buy);
    
    ASSERT_EQ(trades.size(), 1);
    EXPECT_EQ(trades[0].quantity, 5);
    EXPECT_EQ(buy.filled_quantity, 5);
    EXPECT_EQ(buy.status, OrderStatus::PARTIAL);
}

TEST(OrderBookTest, MultipleMatches) {
    OrderBook book("mega_knight");
    
    // Add two sell orders
    Order sell1 = create_order("sell1", "user1", OrderType::SELL, OrderMode::LIMIT, 1250.0, 5);
    Order sell2 = create_order("sell2", "user2", OrderType::SELL, OrderMode::LIMIT, 1260.0, 5);
    book.match_order(sell1);
    book.match_order(sell2);
    
    // Market buy 10 should match both
    Order buy = create_order("buy1", "user3", OrderType::BUY, OrderMode::MARKET, 0.0, 10);
    auto trades = book.match_order(buy);
    
    ASSERT_EQ(trades.size(), 2);
    EXPECT_EQ(trades[0].price, 1250.0);
    EXPECT_EQ(trades[0].quantity, 5);
    EXPECT_EQ(trades[1].price, 1260.0);
    EXPECT_EQ(trades[1].quantity, 5);
    EXPECT_TRUE(buy.is_filled());
}

TEST(OrderBookTest, BestBidAsk) {
    OrderBook book("mega_knight");
    
    Order buy1 = create_order("buy1", "user1", OrderType::BUY, OrderMode::LIMIT, 1200.0, 10);
    Order buy2 = create_order("buy2", "user2", OrderType::BUY, OrderMode::LIMIT, 1250.0, 10);
    Order sell1 = create_order("sell1", "user3", OrderType::SELL, OrderMode::LIMIT, 1300.0, 10);
    Order sell2 = create_order("sell2", "user4", OrderType::SELL, OrderMode::LIMIT, 1280.0, 10);
    
    book.match_order(buy1);
    book.match_order(buy2);
    book.match_order(sell1);
    book.match_order(sell2);
    
    EXPECT_EQ(book.get_best_bid(), 1250.0);  // Highest buy
    EXPECT_EQ(book.get_best_ask(), 1280.0);  // Lowest sell
    EXPECT_EQ(book.get_spread(), 30.0);      // 1280 - 1250
}

TEST(OrderBookTest, CancelOrder) {
    OrderBook book("mega_knight");
    
    Order buy = create_order("buy1", "user1", OrderType::BUY, OrderMode::LIMIT, 1250.0, 10);
    book.match_order(buy);
    
    EXPECT_EQ(book.get_order_count(), 1);
    
    bool cancelled = book.cancel_order("buy1");
    EXPECT_TRUE(cancelled);
    EXPECT_EQ(book.get_order_count(), 0);
    
    // Try cancelling again
    bool cancelled_again = book.cancel_order("buy1");
    EXPECT_FALSE(cancelled_again);
}

TEST(OrderBookTest, Snapshot) {
    OrderBook book("mega_knight");
    
    Order buy = create_order("buy1", "user1", OrderType::BUY, OrderMode::LIMIT, 1250.0, 10);
    Order sell = create_order("sell1", "user2", OrderType::SELL, OrderMode::LIMIT, 1300.0, 5);
    book.match_order(buy);
    book.match_order(sell);
    
    auto snapshot = book.get_snapshot();
    
    EXPECT_EQ(snapshot.card_id, "mega_knight");
    ASSERT_EQ(snapshot.bids.size(), 1);
    EXPECT_EQ(snapshot.bids[0].first, 1250.0);
    EXPECT_EQ(snapshot.bids[0].second, 10);
    ASSERT_EQ(snapshot.asks.size(), 1);
    EXPECT_EQ(snapshot.asks[0].first, 1300.0);
    EXPECT_EQ(snapshot.asks[0].second, 5);
}