#pragma once

#include <pqxx/pqxx>
#include <memory>
#include <string>
#include <optional>

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
    
    // Execute raw query (for testing/setup)
    pqxx::result execute(const std::string& query);
    
    // Transaction support
    template<typename Func>
    auto with_transaction(Func&& func) -> decltype(func(std::declval<pqxx::work&>()));
};

template<typename Func>
auto PostgresClient::with_transaction(Func&& func) -> decltype(func(std::declval<pqxx::work&>())) {
    pqxx::work txn(*conn_);
    try {
        auto result = func(txn);
        txn.commit();
        return result;
    } catch (...) {
        txn.abort();
        throw;
    }
}

} // namespace database
} // namespace clash_trading