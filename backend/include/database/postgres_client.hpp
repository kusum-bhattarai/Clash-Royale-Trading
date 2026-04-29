#pragma once

#include <pqxx/pqxx>
#include <memory>
#include <string>
#include <optional>
#include "core/candle.hpp"
#include <vector>
#include <chrono>

namespace clash_trading {
namespace database {

class PostgresClient {
private:
    std::unique_ptr<pqxx::connection> conn_;
    std::string connection_string_;
    
public:
    explicit PostgresClient(const std::string& connection_string);
    ~PostgresClient();
    
    // Test connection
    bool is_connected() const;

    pqxx::connection* get_connection() { return conn_.get(); }
    
    // Execute raw query (for testing/setup)
    pqxx::result execute(const std::string& query);
    
    // Transaction support
    template<typename Func>
    auto with_transaction(Func&& func) -> decltype(func(std::declval<pqxx::work&>()));

    // Candle/Price History methods
    void insert_candle(const trading::Candle& candle);
    void insert_candles_batch(const std::vector<trading::Candle>& candles);
    std::vector<trading::Candle> get_candles(
        const std::string& card_id,
        trading::Timeframe timeframe,
        int limit = 500
    );
    std::vector<trading::Candle> get_candles_range(
        const std::string& card_id,
        trading::Timeframe timeframe,
        std::chrono::system_clock::time_point start,
        std::chrono::system_clock::time_point end
    );
    std::optional<trading::Candle> get_latest_candle(
        const std::string& card_id,
        trading::Timeframe timeframe
    );
};

template<typename Func>
auto PostgresClient::with_transaction(Func&& func) -> decltype(func(std::declval<pqxx::work&>())) {
    pqxx::work txn(*conn_);
    try {
        if constexpr (std::is_void_v<decltype(func(txn))>) {
            func(txn);
            txn.commit();
        } else {
            auto result = func(txn);
            txn.commit();
            return result;
        }
    } catch (...) {
        txn.abort();
        throw;
    }
}

} // namespace database
} // namespace clash_trading