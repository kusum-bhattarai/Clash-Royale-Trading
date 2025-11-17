#include <gtest/gtest.h>
#include "core/order.hpp"
#include <thread>
#include <chrono>

using namespace clash_trading::core;

// Test Order construction and default values
TEST(OrderTest, DefaultConstruction) {
    Order order;
    
    EXPECT_EQ(order.type, OrderType::BUY);
    EXPECT_EQ(order.mode, OrderMode::MARKET);
    EXPECT_EQ(order.price, 0.0);
    EXPECT_EQ(order.quantity, 0);
    EXPECT_EQ(order.filled_quantity, 0);
    EXPECT_EQ(order.status, OrderStatus::PENDING);
    EXPECT_FALSE(order.is_filled());
}

// Test Order with custom values
TEST(OrderTest, CustomValues) {
    Order order;
    order.order_id = "order_123";
    order.user_id = "user_456";
    order.card_id = "mega_knight";
    order.type = OrderType::BUY;
    order.mode = OrderMode::LIMIT;
    order.price = 1250.0;
    order.quantity = 10;
    order.filled_quantity = 0;
    order.status = OrderStatus::PENDING;
    
    EXPECT_EQ(order.order_id, "order_123");
    EXPECT_EQ(order.card_id, "mega_knight");
    EXPECT_EQ(order.quantity, 10);
    EXPECT_EQ(order.remaining_quantity(), 10);
    EXPECT_TRUE(order.can_match());
}

// Test filled quantity logic
TEST(OrderTest, FilledQuantityLogic) {
    Order order;
    order.quantity = 10;
    order.filled_quantity = 0;
    
    EXPECT_FALSE(order.is_filled());
    EXPECT_EQ(order.remaining_quantity(), 10);
    
    // Partially fill
    order.filled_quantity = 5;
    order.status = OrderStatus::PARTIAL;
    EXPECT_FALSE(order.is_filled());
    EXPECT_EQ(order.remaining_quantity(), 5);
    EXPECT_TRUE(order.can_match());
    
    // Fully fill
    order.filled_quantity = 10;
    order.status = OrderStatus::FILLED;
    EXPECT_TRUE(order.is_filled());
    EXPECT_EQ(order.remaining_quantity(), 0);
    EXPECT_FALSE(order.can_match());
}

// Test price-time priority for BUY orders
TEST(OrderTest, BuyOrderPriority) {
    Order buy1, buy2, buy3;
    
    buy1.type = OrderType::BUY;
    buy1.card_id = "mega_knight";
    buy1.price = 1250.0;
    buy1.timestamp = std::chrono::system_clock::now();
    
    // Sleep to ensure different timestamp
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    buy2.type = OrderType::BUY;
    buy2.card_id = "mega_knight";
    buy2.price = 1250.0;
    buy2.timestamp = std::chrono::system_clock::now();
    
    buy3.type = OrderType::BUY;
    buy3.card_id = "mega_knight";
    buy3.price = 1300.0;  // Higher price
    buy3.timestamp = std::chrono::system_clock::now();
    
    // buy1 has priority over buy2 (same price, earlier timestamp)
    EXPECT_TRUE(buy1.has_priority_over(buy2));
    EXPECT_FALSE(buy2.has_priority_over(buy1));
    
    // buy3 has priority over buy1 (higher price)
    EXPECT_TRUE(buy3.has_priority_over(buy1));
    EXPECT_FALSE(buy1.has_priority_over(buy3));
}

// Test price-time priority for SELL orders
TEST(OrderTest, SellOrderPriority) {
    Order sell1, sell2, sell3;
    
    sell1.type = OrderType::SELL;
    sell1.card_id = "mega_knight";
    sell1.price = 1250.0;
    sell1.timestamp = std::chrono::system_clock::now();
    
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    sell2.type = OrderType::SELL;
    sell2.card_id = "mega_knight";
    sell2.price = 1250.0;
    sell2.timestamp = std::chrono::system_clock::now();
    
    sell3.type = OrderType::SELL;
    sell3.card_id = "mega_knight";
    sell3.price = 1200.0;  // Lower price (better for sellers)
    sell3.timestamp = std::chrono::system_clock::now();
    
    // sell1 has priority over sell2 (same price, earlier timestamp)
    EXPECT_TRUE(sell1.has_priority_over(sell2));
    EXPECT_FALSE(sell2.has_priority_over(sell1));
    
    // sell3 has priority over sell1 (lower price is better for SELL)
    EXPECT_TRUE(sell3.has_priority_over(sell1));
    EXPECT_FALSE(sell1.has_priority_over(sell3));
}

// Test string conversion functions
TEST(OrderTest, StringConversions) {
    // OrderType conversions
    EXPECT_EQ(order_type_to_string(OrderType::BUY), "BUY");
    EXPECT_EQ(order_type_to_string(OrderType::SELL), "SELL");
    
    auto buy_type = string_to_order_type("BUY");
    ASSERT_TRUE(buy_type.has_value());
    EXPECT_EQ(buy_type.value(), OrderType::BUY);
    
    auto invalid_type = string_to_order_type("INVALID");
    EXPECT_FALSE(invalid_type.has_value());
    
    // OrderMode conversions
    EXPECT_EQ(order_mode_to_string(OrderMode::MARKET), "MARKET");
    EXPECT_EQ(order_mode_to_string(OrderMode::LIMIT), "LIMIT");
    
    // OrderStatus conversions
    EXPECT_EQ(order_status_to_string(OrderStatus::PENDING), "PENDING");
    EXPECT_EQ(order_status_to_string(OrderStatus::FILLED), "FILLED");
}

// Test can_match logic
TEST(OrderTest, CanMatchLogic) {
    Order order;
    order.quantity = 10;
    order.filled_quantity = 0;
    order.status = OrderStatus::PENDING;
    
    EXPECT_TRUE(order.can_match());
    
    // Cancelled order cannot match
    order.status = OrderStatus::CANCELLED;
    EXPECT_FALSE(order.can_match());
    
    // Filled order cannot match
    order.status = OrderStatus::FILLED;
    order.filled_quantity = 10;
    EXPECT_FALSE(order.can_match());
    
    // Partial order can match
    order.status = OrderStatus::PARTIAL;
    order.filled_quantity = 5;
    EXPECT_TRUE(order.can_match());
}