#pragma once

#include <string>
#include <chrono>
#include <vector>

namespace trading {

enum class Timeframe {
    ONE_MINUTE,
    FIVE_MINUTES,
    FIFTEEN_MINUTES,
    ONE_HOUR,
    FOUR_HOURS,
    ONE_DAY
};

inline std::string timeframe_to_string(Timeframe tf) {
    switch (tf) {
        case Timeframe::ONE_MINUTE: return "1m";
        case Timeframe::FIVE_MINUTES: return "5m";
        case Timeframe::FIFTEEN_MINUTES: return "15m";
        case Timeframe::ONE_HOUR: return "1h";
        case Timeframe::FOUR_HOURS: return "4h";
        case Timeframe::ONE_DAY: return "1d";
        default: return "1m";
    }
}

inline Timeframe string_to_timeframe(const std::string& str) {
    if (str == "1m") return Timeframe::ONE_MINUTE;
    if (str == "5m") return Timeframe::FIVE_MINUTES;
    if (str == "15m") return Timeframe::FIFTEEN_MINUTES;
    if (str == "1h") return Timeframe::ONE_HOUR;
    if (str == "4h") return Timeframe::FOUR_HOURS;
    if (str == "1d") return Timeframe::ONE_DAY;
    return Timeframe::ONE_MINUTE;
}

inline std::chrono::seconds timeframe_to_seconds(Timeframe tf) {
    switch (tf) {
        case Timeframe::ONE_MINUTE: return std::chrono::seconds(60);
        case Timeframe::FIVE_MINUTES: return std::chrono::seconds(300);
        case Timeframe::FIFTEEN_MINUTES: return std::chrono::seconds(900);
        case Timeframe::ONE_HOUR: return std::chrono::seconds(3600);
        case Timeframe::FOUR_HOURS: return std::chrono::seconds(14400);
        case Timeframe::ONE_DAY: return std::chrono::seconds(86400);
        default: return std::chrono::seconds(60);
    }
}

struct Candle {
    std::string card_id;
    Timeframe timeframe;
    std::chrono::system_clock::time_point timestamp;
    double open_price;
    double high_price;
    double low_price;
    double close_price;
    int volume;
    int trade_count;

    Candle() 
        : open_price(0.0)
        , high_price(0.0)
        , low_price(0.0)
        , close_price(0.0)
        , volume(0)
        , trade_count(0) 
    {}
};

struct CandleBuffer {
    std::string card_id;
    Timeframe timeframe;
    std::chrono::system_clock::time_point period_start;
    double open_price;
    double high_price;
    double low_price;
    double close_price;
    int volume;
    int trade_count;
    bool has_data;

    CandleBuffer(const std::string& card_id, Timeframe tf)
        : card_id(card_id)
        , timeframe(tf)
        , open_price(0.0)
        , high_price(0.0)
        , low_price(0.0)
        , close_price(0.0)
        , volume(0)
        , trade_count(0)
        , has_data(false)
    {}

    void update(double price, int quantity, std::chrono::system_clock::time_point trade_time) {
        if (!has_data) {
            open_price = price;
            high_price = price;
            low_price = price;
            close_price = price;
            period_start = align_to_period(trade_time);
            has_data = true;
        } else {
            if (price > high_price) high_price = price;
            if (price < low_price) low_price = price;
            close_price = price;
        }
        volume += quantity;
        trade_count++;
    }

    bool should_flush(std::chrono::system_clock::time_point current_time) const {
        if (!has_data) return false;
        auto period_duration = timeframe_to_seconds(timeframe);
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            current_time - period_start
        );
        return elapsed >= period_duration;
    }

    Candle get_candle() const {
        Candle candle;
        candle.card_id = card_id;
        candle.timeframe = timeframe;
        candle.timestamp = period_start;
        candle.open_price = open_price;
        candle.high_price = high_price;
        candle.low_price = low_price;
        candle.close_price = close_price;
        candle.volume = volume;
        candle.trade_count = trade_count;
        return candle;
    }

    void reset() {
        has_data = false;
        open_price = 0.0;
        high_price = 0.0;
        low_price = 0.0;
        close_price = 0.0;
        volume = 0;
        trade_count = 0;
    }

private:
    std::chrono::system_clock::time_point align_to_period(
        std::chrono::system_clock::time_point trade_time
    ) const {
        auto time_t = std::chrono::system_clock::to_time_t(trade_time);
        auto period_seconds = timeframe_to_seconds(timeframe).count();
        auto aligned_time_t = (time_t / period_seconds) * period_seconds;
        return std::chrono::system_clock::from_time_t(aligned_time_t);
    }
};

} // namespace trading