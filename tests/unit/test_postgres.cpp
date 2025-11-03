#include <gtest/gtest.h>
#include "database/postgres_client.hpp"

using namespace clash_trading::database;

TEST(PostgresTest, Connection) {
    std::string conn_str = "host=127.0.0.1 port=5432 dbname=clash_trading user=clash_user password=clash_pass_dev";
    
    PostgresClient client(conn_str);
    EXPECT_TRUE(client.is_connected());
}

TEST(PostgresTest, SimpleQuery) {
    std::string conn_str = "host=127.0.0.1 port=5432 dbname=clash_trading user=clash_user password=clash_pass_dev";
    PostgresClient client(conn_str);
    
    auto result = client.execute("SELECT 1 as test");
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0]["test"].as<int>(), 1);
}

TEST(PostgresTest, Transaction) {
    std::string conn_str = "host=127.0.0.1 port=5432 dbname=clash_trading user=clash_user password=clash_pass_dev";
    PostgresClient client(conn_str);
    
    int result = client.with_transaction([](pqxx::work& txn) {
        auto res = txn.exec("SELECT 2 + 2 as sum");
        return res[0]["sum"].as<int>();
    });
    
    EXPECT_EQ(result, 4);
}