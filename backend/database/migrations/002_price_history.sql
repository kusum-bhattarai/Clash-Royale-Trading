-- Price history table for candlestick charts
CREATE TABLE price_history (
    id SERIAL PRIMARY KEY,
    card_id VARCHAR(50) NOT NULL REFERENCES cards(card_id),
    timeframe VARCHAR(10) NOT NULL, -- '1m', '5m', '15m', '1h', '4h', '1d'
    timestamp TIMESTAMP NOT NULL,
    open_price DECIMAL(10,2) NOT NULL,
    high_price DECIMAL(10,2) NOT NULL,
    low_price DECIMAL(10,2) NOT NULL,
    close_price DECIMAL(10,2) NOT NULL,
    volume INT NOT NULL DEFAULT 0,
    trade_count INT NOT NULL DEFAULT 0,
    UNIQUE(card_id, timeframe, timestamp)
);

CREATE INDEX idx_price_history_card_timeframe ON price_history(card_id, timeframe, timestamp DESC);
CREATE INDEX idx_price_history_timestamp ON price_history(timestamp DESC);