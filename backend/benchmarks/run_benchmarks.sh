#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BACKEND_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$BACKEND_DIR/build"
RESULTS_DIR="$SCRIPT_DIR/results"

mkdir -p "$BUILD_DIR" "$RESULTS_DIR"

echo "=== Building benchmarks (Release) ==="
cmake -S "$BACKEND_DIR" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE="$BACKEND_DIR/vcpkg/scripts/buildsystems/vcpkg.cmake" \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build "$BUILD_DIR" --target order_book_bench -j"$(nproc 2>/dev/null || sysctl -n hw.logicalcpu)"

TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
RESULT_FILE="$RESULTS_DIR/results_${TIMESTAMP}.json"

echo ""
echo "=== Running benchmarks ==="
"$BUILD_DIR/bin/order_book_bench" \
    --benchmark_format=json \
    --benchmark_out="$RESULT_FILE" \
    --benchmark_repetitions=3 \
    --benchmark_report_aggregates_only=true

echo ""
echo "=== Results saved to: $RESULT_FILE ==="
echo ""
echo "=== Key numbers for README ==="
# Print just the mean times for the headline benchmarks
python3 - "$RESULT_FILE" <<'EOF'
import json, sys
with open(sys.argv[1]) as f:
    data = json.load(f)
targets = ["BM_MarketOrderSingleFill", "BM_ConcurrentOrders/8"]
for b in data.get("benchmarks", []):
    name = b["name"]
    if any(t in name for t in targets) and "mean" in name:
        unit = b.get("time_unit", "ns")
        print(f"  {name}: {b['real_time']:.1f} {unit}/iter  |  {b.get('items_per_second', 0)/1e6:.2f}M items/sec")
EOF
