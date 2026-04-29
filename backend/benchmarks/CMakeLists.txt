find_package(benchmark CONFIG REQUIRED)

add_executable(order_book_bench
    order_book_bench.cpp
    ${CMAKE_SOURCE_DIR}/src/core/order_book.cpp
)

target_link_libraries(order_book_bench PRIVATE
    benchmark::benchmark
    benchmark::benchmark_main
    OpenSSL::Crypto
)

target_include_directories(order_book_bench PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_compile_options(order_book_bench PRIVATE
    $<$<CONFIG:Release>:-O3 -DNDEBUG>
)
