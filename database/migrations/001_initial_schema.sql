-- Enable UUID extension
CREATE EXTENSION IF NOT EXISTS "uuid-ossp";

-- Users table
CREATE TABLE users (
    user_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    username VARCHAR(50) UNIQUE NOT NULL,
    email VARCHAR(255) UNIQUE NOT NULL,
    password_hash VARCHAR(255) NOT NULL,
    gold_balance BIGINT NOT NULL DEFAULT 100000,
    trader_level VARCHAR(20) NOT NULL DEFAULT 'CHALLENGER',
    total_trades INT NOT NULL DEFAULT 0,
    created_at TIMESTAMP NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMP NOT NULL DEFAULT NOW()
);

CREATE INDEX idx_users_username ON users(username);
CREATE INDEX idx_users_email ON users(email);

-- Cards table
CREATE TABLE cards (
    card_id VARCHAR(50) PRIMARY KEY,
    name VARCHAR(100) NOT NULL,
    rarity VARCHAR(20) NOT NULL,
    elixir_cost VARCHAR(10),
    card_type VARCHAR(20),
    current_market_price DECIMAL(10,2),
    total_supply BIGINT DEFAULT 0,
    usage_rate DECIMAL(5,4),
    last_synced TIMESTAMP
);

CREATE INDEX idx_cards_rarity ON cards(rarity);

-- User inventory
CREATE TABLE user_inventory (
    user_id UUID REFERENCES users(user_id) ON DELETE CASCADE,
    card_id VARCHAR(50) REFERENCES cards(card_id),
    quantity INT NOT NULL DEFAULT 0,
    avg_purchase_price DECIMAL(10,2),
    PRIMARY KEY (user_id, card_id)
);

CREATE INDEX idx_inventory_user ON user_inventory(user_id);

-- Orders table
CREATE TABLE orders (
    order_id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    user_id UUID REFERENCES users(user_id) ON DELETE CASCADE,
    card_id VARCHAR(50) REFERENCES cards(card_id),
    order_type VARCHAR(10) NOT NULL CHECK (order_type IN ('BUY', 'SELL')),
    order_mode VARCHAR(10) NOT NULL CHECK (order_mode IN ('MARKET', 'LIMIT')),
    price DECIMAL(10,2),
    quantity INT NOT NULL,
    filled_quantity INT NOT NULL DEFAULT 0,
    status VARCHAR(20) NOT NULL CHECK (status IN ('PENDING', 'PARTIAL', 'FILLED', 'CANCELLED')),
    created_at TIMESTAMP NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMP NOT NULL DEFAULT NOW()
);

CREATE INDEX idx_orders_user ON orders(user_id);
CREATE INDEX idx_orders_card_status ON orders(card_id, status);
CREATE INDEX idx_orders_created ON orders(created_at DESC);

-- Trades table
CREATE TABLE trades (
    trade_id VARCHAR(100) PRIMARY KEY,
    card_id VARCHAR(50) REFERENCES cards(card_id),
    buyer_id UUID REFERENCES users(user_id),
    seller_id UUID REFERENCES users(user_id),
    price DECIMAL(10,2) NOT NULL,
    quantity INT NOT NULL,
    total_value DECIMAL(12,2) NOT NULL,
    buyer_order_id UUID REFERENCES orders(order_id),
    seller_order_id UUID REFERENCES orders(order_id),
    merkle_hash VARCHAR(64) NOT NULL, 
    executed_at TIMESTAMP NOT NULL DEFAULT NOW()
);

CREATE INDEX idx_trades_card ON trades(card_id, executed_at DESC);
CREATE INDEX idx_trades_buyer ON trades(buyer_id);
CREATE INDEX idx_trades_seller ON trades(seller_id);
CREATE INDEX idx_trades_executed ON trades(executed_at DESC);
