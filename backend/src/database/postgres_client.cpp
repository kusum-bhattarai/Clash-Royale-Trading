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

} // namespace database
} // namespace clash_trading