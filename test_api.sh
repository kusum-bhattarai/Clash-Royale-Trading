#!/bin/bash

API_URL="http://localhost:8080"

echo "========================================="
echo "Testing Clash Royale Trading Platform API"
echo "========================================="
echo ""

# Test 1: Register a new user
echo "1. Registering test user..."
REGISTER_RESPONSE=$(curl -s -X POST "$API_URL/api/auth/register" \
  -H "Content-Type: application/json" \
  -d '{
    "username": "testuser",
    "email": "test@example.com",
    "password": "password123"
  }')

echo "Response: $REGISTER_RESPONSE"
echo ""

# Extract token
TOKEN=$(echo $REGISTER_RESPONSE | grep -o '"token":"[^"]*' | cut -d'"' -f4)

if [ -z "$TOKEN" ]; then
    echo "ERROR: Failed to get token. Registration might have failed."
    echo "Trying to login with existing user..."
    
    # Try login instead
    LOGIN_RESPONSE=$(curl -s -X POST "$API_URL/api/auth/login" \
      -H "Content-Type: application/json" \
      -d '{
        "username": "testuser",
        "password": "password123"
      }')
    
    echo "Login Response: $LOGIN_RESPONSE"
    TOKEN=$(echo $LOGIN_RESPONSE | grep -o '"token":"[^"]*' | cut -d'"' -f4)
fi

echo "Token: $TOKEN"
echo ""

# Extract user_id
USER_ID=$(echo $REGISTER_RESPONSE | grep -o '"user_id":"[^"]*' | cut -d'"' -f4)
if [ -z "$USER_ID" ]; then
    USER_ID=$(echo $LOGIN_RESPONSE | grep -o '"user_id":"[^"]*' | cut -d'"' -f4)
fi

echo "User ID: $USER_ID"
echo ""

# Test 2: Get all cards
echo "2. Fetching all cards..."
CARDS_RESPONSE=$(curl -s -X GET "$API_URL/api/cards")
CARD_COUNT=$(echo $CARDS_RESPONSE | grep -o '"card_id"' | wc -l)
echo "Found $CARD_COUNT cards"
echo ""

# Get first card ID for testing
FIRST_CARD_ID=$(echo $CARDS_RESPONSE | grep -o '"card_id":"[^"]*' | head -1 | cut -d'"' -f4)
echo "First card ID: $FIRST_CARD_ID"
echo ""

# Test 3: Get user portfolio
echo "3. Getting user portfolio..."
PORTFOLIO_RESPONSE=$(curl -s -X GET "$API_URL/api/users/$USER_ID/portfolio" \
  -H "Authorization: Bearer $TOKEN")
echo "Portfolio: $PORTFOLIO_RESPONSE"
echo ""

# Test 4: Get order book for a card
echo "4. Getting order book for card: $FIRST_CARD_ID"
ORDERBOOK_RESPONSE=$(curl -s -X GET "$API_URL/api/cards/$FIRST_CARD_ID/orderbook")
echo "Order Book: $ORDERBOOK_RESPONSE"
echo ""

echo "========================================="
echo "API Test Complete!"
echo "========================================="
echo ""
echo "Save this token for testing: $TOKEN"
echo "Save this user_id for testing: $USER_ID"