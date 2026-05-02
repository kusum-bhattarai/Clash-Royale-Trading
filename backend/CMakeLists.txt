cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "Vcpkg toolchain file")
endif()

# Project definition
project(ClashRoyaleTrading 
    VERSION 1.0.0
    DESCRIPTION "High-performance card trading platform"
    LANGUAGES CXX
)

# C++20 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler flags for Clang on macOS ARM
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -DNDEBUG")
endif()

# Sanitizer build types — use separate build dirs, never combine Tsan + Asan
# cmake .. -DCMAKE_BUILD_TYPE=Tsan  →  ThreadSanitizer
# cmake .. -DCMAKE_BUILD_TYPE=Asan  →  AddressSanitizer
set(CMAKE_CXX_FLAGS_TSAN  "-fsanitize=thread  -g -O1" CACHE STRING "Flags for ThreadSanitizer build")
set(CMAKE_EXE_LINKER_FLAGS_TSAN  "-fsanitize=thread"  CACHE STRING "Linker flags for ThreadSanitizer build")
set(CMAKE_CXX_FLAGS_ASAN  "-fsanitize=address -g -O1" CACHE STRING "Flags for AddressSanitizer build")
set(CMAKE_EXE_LINKER_FLAGS_ASAN  "-fsanitize=address" CACHE STRING "Linker flags for AddressSanitizer build")

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# vcpkg integration (will be used once I set it up)
if(DEFINED ENV{VCPKG_ROOT})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "Vcpkg toolchain file")
endif()

# Find packages
find_package(fmt CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(libpqxx CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(CURL REQUIRED)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# Main executable
add_executable(clash_trading
    src/main.cpp
    src/utils/config.cpp
    src/utils/logger.cpp
    src/core/order_book.cpp
    src/database/postgres_client.cpp
    src/services/order_service.cpp
    src/services/trade_service.cpp
    src/services/user_service.cpp
    src/services/analytics_service.cpp
    src/services/dynamic_pricing_service.cpp
    src/services/card_sync_service.cpp
    src/services/price_aggregation_service.cpp
    src/api/auth.cpp
    src/api/http_server.cpp
    src/api/rate_limiter.cpp
    src/api/routes.cpp
    src/api/websocket.cpp
    src/external/clash_royale_api.cpp
)

# Find Boost (needed for Beast)
find_package(Boost REQUIRED COMPONENTS system)

# Link libraries
target_link_libraries(clash_trading PRIVATE
    fmt::fmt
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    libpqxx::pqxx
    OpenSSL::Crypto
    OpenSSL::SSL
    Boost::system
    CURL::libcurl
)

# libuuid: implicit on macOS, explicit on Linux
if(UNIX AND NOT APPLE)
    target_link_libraries(clash_trading PRIVATE uuid)
endif()

target_include_directories(clash_trading PRIVATE ${CMAKE_SOURCE_DIR}/include)
enable_testing()
add_subdirectory(tests)
add_subdirectory(benchmarks)

# Print configuration summary
message(STATUS "=== Clash Royale Trading Platform ===")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "C++ Standard: C++${CMAKE_CXX_STANDARD}")
message(STATUS "System: ${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_PROCESSOR}")