#include "services/price_aggregation_service.hpp"
#include <iostream>

namespace clash_trading {
namespace services {

PriceAggregationService::PriceAggregationService(std::shared_ptr<database::PostgresClient> db)
    : db_(db) {
    // Initialize all timeframes we want to track
    tracked_timeframes_ = {
        trading::Timeframe::ONE_MINUTE,
        trading::Timeframe::FIVE_MINUTES,
        trading::Timeframe::FIFTEEN_MINUTES,
        trading::Timeframe::ONE_HOUR,
        trading::Timeframe::FOUR_HOURS,
        trading::Timeframe::ONE_DAY
    };
}

void PriceAggregationService::on_trade(
    const std::string& card_id,
    double price,
    int quantity,
    std::chrono::system_clock::time_point trade_time
) {
    std::lock_guard<std::mutex> lock(buffers_mutex_);
    
    // Update all timeframe buffers for this card
    for (const auto& timeframe : tracked_timeframes_) {
        // Initialize buffer if it doesn't exist
        if (candle_buffers_[card_id].find(timeframe) == candle_buffers_[card_id].end()) {
            initialize_buffer(card_id, timeframe);
        }
        
        auto& buffer = candle_buffers_[card_id][timeframe];
        
        // Check if we need to flush the current candle before updating
        if (buffer.should_flush(trade_time)) {
            flush_candle(card_id, timeframe);
            buffer.reset();
        }
        
        // Update the buffer with this trade
        buffer.update(price, quantity, trade_time);
    }
}

void PriceAggregationService::flush_all_candles() {
    std::lock_guard<std::mutex> lock(buffers_mutex_);
    
    for (auto& [card_id, timeframe_map] : candle_buffers_) {
        for (auto& [timeframe, buffer] : timeframe_map) {
            if (buffer.has_data) {
                try {
                    auto candle = buffer.get_candle();
                    db_->insert_candle(candle);
                    std::cout << "Flushed candle for " << card_id 
                              << " (" << trading::timeframe_to_string(timeframe) << ")" << std::endl;
                } catch (const std::exception& e) {
                    std::cerr << "Error flushing candle: " << e.what() << std::endl;
                }
            }
        }
    }
}

std::vector<trading::Candle> PriceAggregationService::get_candles(
    const std::string& card_id,
    trading::Timeframe timeframe,
    int limit
) {
    return db_->get_candles(card_id, timeframe, limit);
}

std::vector<trading::Candle> PriceAggregationService::get_candles_range(
    const std::string& card_id,
    trading::Timeframe timeframe,
    std::chrono::system_clock::time_point start,
    std::chrono::system_clock::time_point end
) {
    return db_->get_candles_range(card_id, timeframe, start, end);
}

void PriceAggregationService::flush_candle(const std::string& card_id, trading::Timeframe timeframe) {
    auto& buffer = candle_buffers_[card_id][timeframe];
    
    if (!buffer.has_data) {
        return;
    }
    
    try {
        auto candle = buffer.get_candle();
        db_->insert_candle(candle);
        std::cout << "Saved candle: " << card_id 
                  << " (" << trading::timeframe_to_string(timeframe) << ") "
                  << "O:" << candle.open_price << " H:" << candle.high_price 
                  << " L:" << candle.low_price << " C:" << candle.close_price 
                  << " V:" << candle.volume << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error saving candle: " << e.what() << std::endl;
    }
}

void PriceAggregationService::initialize_buffer(const std::string& card_id, trading::Timeframe timeframe) {
    candle_buffers_[card_id][timeframe] = trading::CandleBuffer(card_id, timeframe);
}

} // namespace services
} // namespace clash_trading