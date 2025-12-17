#pragma once

#include "core/candle.hpp"
#include "database/postgres_client.hpp"
#include <string>
#include <map>
#include <memory>
#include <mutex>
#include <chrono>
#include <vector>

namespace clash_trading {
namespace services {

class PriceAggregationService {
private:
    std::shared_ptr<database::PostgresClient> db_;
    
    // Map: card_id -> timeframe -> CandleBuffer
    std::map<std::string, std::map<trading::Timeframe, trading::CandleBuffer>> candle_buffers_;
    std::mutex buffers_mutex_;
    
    // All timeframes we're tracking
    std::vector<trading::Timeframe> tracked_timeframes_;
    
public:
    explicit PriceAggregationService(std::shared_ptr<database::PostgresClient> db);
    
    // Called after each trade execution
    void on_trade(
        const std::string& card_id,
        double price,
        int quantity,
        std::chrono::system_clock::time_point trade_time
    );
    
    // Manually flush all pending candles (useful for shutdown)
    void flush_all_candles();
    
    // Get candles for a card
    std::vector<trading::Candle> get_candles(
        const std::string& card_id,
        trading::Timeframe timeframe,
        int limit = 500
    );
    
    // Get candles within a time range
    std::vector<trading::Candle> get_candles_range(
        const std::string& card_id,
        trading::Timeframe timeframe,
        std::chrono::system_clock::time_point start,
        std::chrono::system_clock::time_point end
    );
    
private:
    void flush_candle(const std::string& card_id, trading::Timeframe timeframe);
    void initialize_buffer(const std::string& card_id, trading::Timeframe timeframe);
};

} // namespace services
} // namespace clash_trading