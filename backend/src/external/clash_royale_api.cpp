#include "external/clash_royale_api.hpp"
#include <fmt/core.h>
#include <stdexcept>
#include <curl/curl.h>

namespace clash_trading {
namespace external {

// Callback for curl to write response data
static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

ClashRoyaleAPI::ClashRoyaleAPI(const std::string& base_url,
                               const std::string& api_key,
                               int rate_limit,
                               int timeout_seconds)
    : base_url_(base_url)
    , api_key_(api_key)
    , rate_limit_per_second_(rate_limit)
    , timeout_seconds_(timeout_seconds) {
    
    if (api_key_.empty()) {
        throw std::runtime_error("Clash Royale API key is required");
    }
    
    // Initialize curl globally
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

std::vector<CardData> ClashRoyaleAPI::fetch_all_cards() {
    try {
        std::string response = make_request("/cards");
        
        auto json = nlohmann::json::parse(response);
        std::vector<CardData> cards;
        
        if (json.contains("items") && json["items"].is_array()) {
            for (const auto& item : json["items"]) {
                cards.push_back(parse_card(item));
            }
        }
        
        fmt::print("Fetched {} cards from Clash Royale API\n", cards.size());
        return cards;
        
    } catch (const std::exception& e) {
        fmt::print(stderr, "Failed to fetch cards: {}\n", e.what());
        throw;
    }
}

std::optional<CardData> ClashRoyaleAPI::fetch_card(const std::string& card_id) {
    try {
        std::string endpoint = "/cards/" + card_id;
        std::string response = make_request(endpoint);
        
        auto json = nlohmann::json::parse(response);
        return parse_card(json);
        
    } catch (const std::exception& e) {
        fmt::print(stderr, "Failed to fetch card {}: {}\n", card_id, e.what());
        return std::nullopt;
    }
}

std::string ClashRoyaleAPI::make_request(const std::string& endpoint) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("Failed to initialize curl");
    }
    
    std::string response_data;
    std::string url = base_url_ + endpoint;
    
    // Set up headers
    struct curl_slist* headers = nullptr;
    std::string auth_header = "Authorization: Bearer " + api_key_;
    headers = curl_slist_append(headers, auth_header.c_str());
    headers = curl_slist_append(headers, "Accept: application/json");
    
    // Configure curl
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, (long)timeout_seconds_);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "ClashRoyaleTrading/1.0");
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    
    // Perform request
    CURLcode res = curl_easy_perform(curl);
    
    // Check for errors
    if (res != CURLE_OK) {
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        throw std::runtime_error(
            fmt::format("Curl request failed: {}", curl_easy_strerror(res)));
    }
    
    // Check HTTP status code
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    if (http_code != 200) {
        throw std::runtime_error(
            fmt::format("HTTP {} - Request failed for {}", http_code, endpoint));
    }
    
    return response_data;
}

CardData ClashRoyaleAPI::parse_card(const nlohmann::json& json) {
    CardData card;
    
    // ID might be integer or string
    if (json["id"].is_number()) {
        card.id = std::to_string(json["id"].get<int>());
    } else {
        card.id = json.value("id", "");
    }
    
    card.name = json.value("name", "");
    card.max_level = json.value("maxLevel", 14);
    
    if (json.contains("rarity")) {
        card.rarity = json["rarity"];
    } else {
        card.rarity = "Common";
    }
    
    if (json.contains("iconUrls") && json["iconUrls"].contains("medium")) {
        card.icon_url = json["iconUrls"]["medium"];
    }
    
    if (json.contains("elixirCost")) {
        card.elixir_cost = json["elixirCost"];
    }
    
    return card;
}

} // namespace external
} // namespace clash_trading