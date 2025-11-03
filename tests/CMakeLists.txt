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
target_link_libraries(test_order_book PRIVATE GTest::gtest GTest::gtest_main)
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