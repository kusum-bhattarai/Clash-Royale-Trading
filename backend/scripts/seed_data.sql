-- Seed Data Script for Clash Royale Trading Platform

-- Create a second test user 
INSERT INTO users (user_id, username, email, password_hash, gold_balance, trader_level, total_trades)
VALUES 
    ('11111111-1111-1111-1111-111111111111', 'trader_alice', 'alice@test.com', 
     '5e884898da28047151d0e56f8dc6292773603d0d6aabbdd62a11ef721d1542d8', -- password: 'password123'
     100000, 'CHALLENGER', 0),
    ('22222222-2222-2222-2222-222222222222', 'trader_bob', 'bob@test.com',
     '5e884898da28047151d0e56f8dc6292773603d0d6aabbdd62a11ef721d1542d8', -- password: 'password123'
     100000, 'CHALLENGER', 0)
ON CONFLICT (username) DO NOTHING;

-- Give Alice initial inventory of some popular cards
INSERT INTO user_inventory (user_id, card_id, quantity, avg_purchase_price)
VALUES
    -- Alice owns cards to sell
    ('11111111-1111-1111-1111-111111111111', '26000000', 50, 100.00),  -- Knight
    ('11111111-1111-1111-1111-111111111111', '26000001', 30, 100.00),  -- Archers
    ('11111111-1111-1111-1111-111111111111', '26000072', 20, 2000.00), -- Mega Knight
    ('11111111-1111-1111-1111-111111111111', '28000000', 10, 10000.00) -- Princess
ON CONFLICT (user_id, card_id) DO UPDATE 
    SET quantity = user_inventory.quantity + EXCLUDED.quantity;

-- Give Bob initial inventory of different cards
INSERT INTO user_inventory (user_id, card_id, quantity, avg_purchase_price)
VALUES
    -- Bob owns cards to sell
    ('22222222-2222-2222-2222-222222222222', '26000002', 40, 100.00),  -- Goblins
    ('22222222-2222-2222-2222-222222222222', '26000010', 25, 500.00),  -- Giant
    ('22222222-2222-2222-2222-222222222222', '28000001', 15, 10000.00), -- Ice Wizard
    ('22222222-2222-2222-2222-222222222222', '28000011', 8, 10000.00)   -- Electro Wizard
ON CONFLICT (user_id, card_id) DO UPDATE 
    SET quantity = user_inventory.quantity + EXCLUDED.quantity;

-- Create some initial LIMIT SELL orders from Alice (to provide liquidity)
INSERT INTO orders (order_id, user_id, card_id, order_type, order_mode, price, quantity, filled_quantity, status)
VALUES
    ('aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa', '11111111-1111-1111-1111-111111111111', 
     '26000000', 'SELL', 'LIMIT', 120.00, 10, 0, 'PENDING'),  -- Selling 10 Knights @ 120
    ('aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaab', '11111111-1111-1111-1111-111111111111',
     '26000072', 'SELL', 'LIMIT', 2200.00, 5, 0, 'PENDING')   -- Selling 5 Mega Knights @ 2200
ON CONFLICT (order_id) DO NOTHING;

-- Create some initial LIMIT BUY orders from Bob (to provide liquidity)
INSERT INTO orders (order_id, user_id, card_id, order_type, order_mode, price, quantity, filled_quantity, status)
VALUES
    ('bbbbbbbb-bbbb-bbbb-bbbb-bbbbbbbbbbbb', '22222222-2222-2222-2222-222222222222',
     '26000001', 'BUY', 'LIMIT', 110.00, 8, 0, 'PENDING'),    -- Buying 8 Archers @ 110
    ('bbbbbbbb-bbbb-bbbb-bbbb-bbbbbbbbbbbc', '22222222-2222-2222-2222-222222222222',
     '28000000', 'BUY', 'LIMIT', 11000.00, 3, 0, 'PENDING')   -- Buying 3 Princess @ 11000
ON CONFLICT (order_id) DO NOTHING;

-- Verify the data
SELECT 'Users created:' as status;
SELECT user_id, username, gold_balance FROM users WHERE username IN ('trader_alice', 'trader_bob');

SELECT 'Alice inventory:' as status;
SELECT ui.card_id, c.name, ui.quantity, ui.avg_purchase_price 
FROM user_inventory ui 
JOIN cards c ON ui.card_id = c.card_id 
WHERE ui.user_id = '11111111-1111-1111-1111-111111111111';

SELECT 'Bob inventory:' as status;
SELECT ui.card_id, c.name, ui.quantity, ui.avg_purchase_price 
FROM user_inventory ui 
JOIN cards c ON ui.card_id = c.card_id 
WHERE ui.user_id = '22222222-2222-2222-2222-222222222222';

SELECT 'Active orders:' as status;
SELECT o.order_id, u.username, c.name, o.order_type, o.order_mode, o.price, o.quantity, o.status
FROM orders o
JOIN users u ON o.user_id = u.user_id
JOIN cards c ON o.card_id = c.card_id
WHERE o.status = 'PENDING';