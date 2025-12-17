#include "database/postgres_client.hpp"
#include <stdexcept>

namespace clash_trading {
namespace database {

PostgresClient::PostgresClient(const std::string& connection_string) 
    : connection_string_(connection_string) {
    try {
        conn_ = std::make_unique<pqxx::connection>(connection_string_);
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to connect to database: " + std::string(e.what()));
    }
}

PostgresClient::~PostgresClient() {
    if (conn_ && conn_->is_open()) {
        conn_->close();
    }
}

bool PostgresClient::is_connected() const {
    return conn_ && conn_->is_open();
}

pqxx::result PostgresClient::execute(const std::string& query) {
    pqxx::nontransaction ntxn(*conn_);
    return ntxn.exec(query);
}

void PostgresClient::insert_candle(const trading::Candle& candle) {
    pqxx::work txn(*conn_);
    
    auto timestamp_str = std::to_string(
        std::chrono::system_clock::to_time_t(candle.timestamp)
    );
    
    txn.exec_params(
        "INSERT INTO price_history "
        "(card_id, timeframe, timestamp, open_price, high_price, low_price, close_price, volume, trade_count) "
        "VALUES ($1, $2, to_timestamp($3), $4, $5, $6, $7, $8, $9) "
        "ON CONFLICT (card_id, timeframe, timestamp) DO UPDATE SET "
        "open_price = EXCLUDED.open_price, "
        "high_price = EXCLUDED.high_price, "
        "low_price = EXCLUDED.low_price, "
        "close_price = EXCLUDED.close_price, "
        "volume = EXCLUDED.volume, "
        "trade_count = EXCLUDED.trade_count",
        candle.card_id,
        trading::timeframe_to_string(candle.timeframe),
        timestamp_str,
        candle.open_price,
        candle.high_price,
        candle.low_price,
        candle.close_price,
        candle.volume,
        candle.trade_count
    );
    
    txn.commit();
}

void PostgresClient::insert_candles_batch(const std::vector<trading::Candle>& candles) {
    if (candles.empty()) return;
    
    pqxx::work txn(*conn_);
    
    for (const auto& candle : candles) {
        auto timestamp_str = std::to_string(
            std::chrono::system_clock::to_time_t(candle.timestamp)
        );
        
        txn.exec_params(
            "INSERT INTO price_history "
            "(card_id, timeframe, timestamp, open_price, high_price, low_price, close_price, volume, trade_count) "
            "VALUES ($1, $2, to_timestamp($3), $4, $5, $6, $7, $8, $9) "
            "ON CONFLICT (card_id, timeframe, timestamp) DO UPDATE SET "
            "open_price = EXCLUDED.open_price, "
            "high_price = EXCLUDED.high_price, "
            "low_price = EXCLUDED.low_price, "
            "close_price = EXCLUDED.close_price, "
            "volume = EXCLUDED.volume, "
            "trade_count = EXCLUDED.trade_count",
            candle.card_id,
            trading::timeframe_to_string(candle.timeframe),
            timestamp_str,
            candle.open_price,
            candle.high_price,
            candle.low_price,
            candle.close_price,
            candle.volume,
            candle.trade_count
        );
    }
    
    txn.commit();
}

std::vector<trading::Candle> PostgresClient::get_candles(
    const std::string& card_id,
    trading::Timeframe timeframe,
    int limit
) {
    pqxx::nontransaction txn(*conn_);
    
    auto result = txn.exec_params(
        "SELECT card_id, timeframe, timestamp, open_price, high_price, low_price, close_price, volume, trade_count "
        "FROM price_history "
        "WHERE card_id = $1 AND timeframe = $2 "
        "ORDER BY timestamp DESC "
        "LIMIT $3",
        card_id,
        trading::timeframe_to_string(timeframe),
        limit
    );
    
    std::vector<trading::Candle> candles;
    candles.reserve(result.size());
    
    for (const auto& row : result) {
        trading::Candle candle;
        candle.card_id = row["card_id"].as<std::string>();
        candle.timeframe = trading::string_to_timeframe(row["timeframe"].as<std::string>());
        
        auto timestamp_str = row["timestamp"].as<std::string>();
        std::tm tm = {};
        std::istringstream ss(timestamp_str);
        ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
        candle.timestamp = std::chrono::system_clock::from_time_t(std::mktime(&tm));
        
        candle.open_price = row["open_price"].as<double>();
        candle.high_price = row["high_price"].as<double>();
        candle.low_price = row["low_price"].as<double>();
        candle.close_price = row["close_price"].as<double>();
        candle.volume = row["volume"].as<int>();
        candle.trade_count = row["trade_count"].as<int>();
        
        candles.push_back(candle);
    }
    
    return candles;
}

std::vector<trading::Candle> PostgresClient::get_candles_range(
    const std::string& card_id,
    trading::Timeframe timeframe,
    std::chrono::system_clock::time_point start,
    std::chrono::system_clock::time_point end
) {
    pqxx::nontransaction txn(*conn_);
    
    auto start_str = std::to_string(std::chrono::system_clock::to_time_t(start));
    auto end_str = std::to_string(std::chrono::system_clock::to_time_t(end));
    
    auto result = txn.exec_params(
        "SELECT card_id, timeframe, timestamp, open_price, high_price, low_price, close_price, volume, trade_count "
        "FROM price_history "
        "WHERE card_id = $1 AND timeframe = $2 "
        "AND timestamp >= to_timestamp($3) AND timestamp <= to_timestamp($4) "
        "ORDER BY timestamp ASC",
        card_id,
        trading::timeframe_to_string(timeframe),
        start_str,
        end_str
    );
    
    std::vector<trading::Candle> candles;
    candles.reserve(result.size());
    
    for (const auto& row : result) {
        trading::Candle candle;
        candle.card_id = row["card_id"].as<std::string>();
        candle.timeframe = trading::string_to_timeframe(row["timeframe"].as<std::string>());
        
        auto timestamp_str = row["timestamp"].as<std::string>();
        std::tm tm = {};
        std::istringstream ss(timestamp_str);
        ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
        candle.timestamp = std::chrono::system_clock::from_time_t(std::mktime(&tm));
        
        candle.open_price = row["open_price"].as<double>();
        candle.high_price = row["high_price"].as<double>();
        candle.low_price = row["low_price"].as<double>();
        candle.close_price = row["close_price"].as<double>();
        candle.volume = row["volume"].as<int>();
        candle.trade_count = row["trade_count"].as<int>();
        
        candles.push_back(candle);
    }
    
    return candles;
}

std::optional<trading::Candle> PostgresClient::get_latest_candle(
    const std::string& card_id,
    trading::Timeframe timeframe
) {
    pqxx::nontransaction txn(*conn_);
    
    auto result = txn.exec_params(
        "SELECT card_id, timeframe, timestamp, open_price, high_price, low_price, close_price, volume, trade_count "
        "FROM price_history "
        "WHERE card_id = $1 AND timeframe = $2 "
        "ORDER BY timestamp DESC "
        "LIMIT 1",
        card_id,
        trading::timeframe_to_string(timeframe)
    );
    
    if (result.empty()) {
        return std::nullopt;
    }
    
    const auto& row = result[0];
    trading::Candle candle;
    candle.card_id = row["card_id"].as<std::string>();
    candle.timeframe = trading::string_to_timeframe(row["timeframe"].as<std::string>());
    
    auto timestamp_str = row["timestamp"].as<std::string>();
    std::tm tm = {};
    std::istringstream ss(timestamp_str);
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    candle.timestamp = std::chrono::system_clock::from_time_t(std::mktime(&tm));
    
    candle.open_price = row["open_price"].as<double>();
    candle.high_price = row["high_price"].as<double>();
    candle.low_price = row["low_price"].as<double>();
    candle.close_price = row["close_price"].as<double>();
    candle.volume = row["volume"].as<int>();
    candle.trade_count = row["trade_count"].as<int>();
    
    return candle;
}

} // namespace database
} // namespace clash_trading