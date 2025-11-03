#pragma once

#include <string>
#include <chrono>

namespace clash_trading {

// Represents an executed trade between a buyer and seller
// Each trade has a cryptographic hash (Merkle hash) for integrity verification

struct Trade {
    std::string trade_id;           // UUID
    std::string card_id;            // Which card was traded
    std::string buyer_id;           // User who bought
    std::string seller_id;          // User who sold
    double price;                   // Price per card
    int quantity;                   // Number of cards traded
    double total_value;             // price * quantity
    std::string buyer_order_id;     // Original buy order ID
    std::string seller_order_id;    // Original sell order ID
    std::string merkle_hash;        // SHA-256 hash for integrity
    std::chrono::system_clock::time_point executed_at;

    std::string calculate_merkle_hash() const;
    bool verify_integrity() const;
};

} // namespace clash_trading