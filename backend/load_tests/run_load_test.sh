#!/usr/bin/env bash
# Run the k6 order-placement load test.
# Prerequisites: k6 installed (brew install k6), backend server running on :8080,
# and a seeded load_test_user account.
#
# Usage:
#   bash backend/load_tests/run_load_test.sh
#   bash backend/load_tests/run_load_test.sh --base-url http://localhost:8080

set -euo pipefail

BASE_URL="${BASE_URL:-http://localhost:8080}"
RESULTS_DIR="$(dirname "$0")/results"
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")

# Parse args
while [[ $# -gt 0 ]]; do
  case $1 in
    --base-url) BASE_URL="$2"; shift 2 ;;
    *) echo "Unknown arg: $1"; exit 1 ;;
  esac
done

# Check k6 is installed
if ! command -v k6 &>/dev/null; then
  echo "k6 not found. Install with: brew install k6"
  exit 1
fi

# Ensure load_test_user exists
echo "→ Registering load_test_user (ok if 409)..."
curl -s -o /dev/null -X POST "$BASE_URL/api/v1/auth/register" \
  -H 'Content-Type: application/json' \
  -d '{"username":"load_test_user","password":"load_test_pass","email":"load@test.local","starting_gold":100000000}'

# Obtain JWT
echo "→ Logging in as load_test_user..."
TOKEN=$(curl -s -X POST "$BASE_URL/api/v1/auth/login" \
  -H 'Content-Type: application/json' \
  -d '{"username":"load_test_user","password":"load_test_pass"}' \
  | grep -o '"token":"[^"]*"' | cut -d'"' -f4)

if [[ -z "$TOKEN" ]]; then
  echo "Failed to obtain JWT. Is the server running on $BASE_URL?"
  exit 1
fi
echo "→ Got token (truncated): ${TOKEN:0:40}..."

# Run k6
echo "→ Running k6 load test..."
k6 run \
  --env TOKEN="$TOKEN" \
  --env BASE_URL="$BASE_URL" \
  --out json="$RESULTS_DIR/${TIMESTAMP}_raw.json" \
  "$(dirname "$0")/order_placement.js"

echo ""
echo "Results saved to $RESULTS_DIR/${TIMESTAMP}_raw.json"
echo "Summary saved to $RESULTS_DIR/latest.json"
