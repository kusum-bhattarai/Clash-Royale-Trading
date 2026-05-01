#!/usr/bin/env bash
# Run the matching engine under ThreadSanitizer to detect data races.
#
# What it tests:
#   - test_order_book: unit tests covering match_order, cancel_order, get_snapshot
#   - BM_ConcurrentOrders: N-thread throughput benchmark (shared_mutex hot path)
#
# TSAN and ASAN must NOT be combined — use separate build directories.
#
# Usage (from project root):
#   bash backend/scripts/run_tsan.sh
#
# Interpreting output:
#   "ThreadSanitizer: data race" → a real race was found — fix it.
#   No TSAN output → matching engine is race-condition free under this workload.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BACKEND_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$BACKEND_DIR/build_tsan"
NCPU=$(sysctl -n hw.logicalcpu 2>/dev/null || nproc 2>/dev/null || echo 4)

# Locate vcpkg toolchain: prefer VCPKG_ROOT env, then ~/vcpkg, then read from existing build cache
find_toolchain() {
    if [[ -n "${VCPKG_ROOT:-}" && -f "$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" ]]; then
        echo "$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
        return
    fi
    if [[ -f "$HOME/vcpkg/scripts/buildsystems/vcpkg.cmake" ]]; then
        echo "$HOME/vcpkg/scripts/buildsystems/vcpkg.cmake"
        return
    fi
    # Fall back to what the existing build used
    local cache="$BACKEND_DIR/build/CMakeCache.txt"
    if [[ -f "$cache" ]]; then
        grep -m1 "VCPKG_ROOT\|CMAKE_TOOLCHAIN_FILE" "$cache" \
          | grep -o '"[^"]*vcpkg.cmake"' | tr -d '"' || true
    fi
}

TOOLCHAIN=$(find_toolchain)
if [[ -z "$TOOLCHAIN" ]]; then
    echo "ERROR: Could not find vcpkg toolchain. Set VCPKG_ROOT or install vcpkg to ~/vcpkg."
    exit 1
fi
echo "Using toolchain: $TOOLCHAIN"

echo ""
echo "=== ThreadSanitizer Build ==="
cmake -B "$BUILD_DIR" "$BACKEND_DIR" \
  -DCMAKE_BUILD_TYPE=Tsan \
  -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" \
  -DVCPKG_INSTALLED_DIR="$BACKEND_DIR/build/vcpkg_installed" \
  -Wno-dev

echo ""
echo "=== Building test_order_book + order_book_bench ==="
cmake --build "$BUILD_DIR" --target test_order_book order_book_bench -j"$NCPU"

echo ""
echo "=== Running OrderBook unit tests under TSAN ==="
"$BUILD_DIR/bin/test_order_book"

echo ""
echo "=== Running BM_ConcurrentOrders under TSAN ==="
# 0.5s gives TSAN enough iterations to surface latent races
"$BUILD_DIR/bin/order_book_bench" \
  --benchmark_filter="BM_ConcurrentOrders" \
  --benchmark_min_time=0.5s

echo ""
echo "=== TSAN run complete ==="
echo "If no 'ThreadSanitizer: data race' lines appeared above, the matching engine"
echo "passes ThreadSanitizer with zero detected races."
echo ""
echo "Update the README 'Results' line with your findings."
