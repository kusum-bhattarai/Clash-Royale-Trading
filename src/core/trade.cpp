#include "core/trade.hpp"
#include <openssl/sha.h>
#include <sstream>
#include <iomanip>

namespace clash_trading {

std::string Trade::calculate_merkle_hash() const {
    // Concatenate all trade data for hashing
    std::ostringstream oss;
    oss << trade_id 
        << buyer_id 
        << seller_id 
        << card_id 
        << std::fixed << std::setprecision(2) << price 
        << quantity 
        << buyer_order_id 
        << seller_order_id;
    
    std::string data = oss.str();
    
    // Calculate SHA-256 hash
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(data.c_str()), 
           data.length(), 
           hash);
    
    // Convert to hex string
    std::ostringstream hex_stream;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        hex_stream << std::hex << std::setw(2) << std::setfill('0') 
                   << static_cast<int>(hash[i]);
    }
    
    return hex_stream.str();
}

bool Trade::verify_integrity() const {
    return calculate_merkle_hash() == merkle_hash;
}

} // namespace clash_trading