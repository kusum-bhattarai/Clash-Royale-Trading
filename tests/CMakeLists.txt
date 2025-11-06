find_package(GTest REQUIRED)

# Test 1: Order
add_executable(test_order unit/test_order.cpp)
target_link_libraries(test_order PRIVATE GTest::gtest GTest::gtest_main)
target_include_directories(test_order PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME OrderTests COMMAND test_order)

# Test 2: OrderBook
add_executable(test_order_book 
    unit/test_order_book.cpp
    ${CMAKE_SOURCE_DIR}/src/core/order_book.cpp
)
target_link_libraries(test_order_book PRIVATE 
    GTest::gtest 
    GTest::gtest_main
    OpenSSL::Crypto  # ADD THIS - needed for SHA256 in Trade
)
target_include_directories(test_order_book PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME OrderBookTests COMMAND test_order_book)

# Test 3: Postgres
add_executable(test_postgres 
    unit/test_postgres.cpp
    ${CMAKE_SOURCE_DIR}/src/database/postgres_client.cpp
)
target_link_libraries(test_postgres PRIVATE 
    GTest::gtest 
    GTest::gtest_main
    libpqxx::pqxx
)
target_include_directories(test_postgres PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME PostgresTests COMMAND test_postgres)

# Test 4: TradeService
add_executable(test_trade_service
    unit/test_trade_service.cpp
    ${CMAKE_SOURCE_DIR}/src/services/trade_service.cpp
    ${CMAKE_SOURCE_DIR}/src/database/postgres_client.cpp
    ${CMAKE_SOURCE_DIR}/src/core/order_book.cpp
)
target_link_libraries(test_trade_service PRIVATE 
    GTest::gtest 
    GTest::gtest_main
    libpqxx::pqxx
    OpenSSL::Crypto
)
target_include_directories(test_trade_service PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME TradeServiceTests COMMAND test_trade_service)

# Test 5: UserService
add_executable(test_user_service
    unit/test_user_service.cpp
    ${CMAKE_SOURCE_DIR}/src/services/user_service.cpp
    ${CMAKE_SOURCE_DIR}/src/database/postgres_client.cpp
)
target_link_libraries(test_user_service PRIVATE 
    GTest::gtest 
    GTest::gtest_main
    libpqxx::pqxx
)
target_include_directories(test_user_service PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME UserServiceTests COMMAND test_user_service)