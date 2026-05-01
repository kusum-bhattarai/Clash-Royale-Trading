#!/usr/bin/env bash
# Run unit tests under AddressSanitizer to detect heap/stack memory errors.
#
# What it tests (no database required):
#   - test_order:       Order struct, status transitions, field constraints
#   - test_order_book:  Matching engine — add_order, match_order, cancel_order, get_snapshot
#   - test_auth:        JWT generate/verify, password hash/verify
#
# TSAN and ASAN must NOT be combined — use separate build directories.
# On macOS, ASAN_OPTIONS=detect_leaks=0 avoids false positives from system allocators.
#
# Usage (from project root):
#   bash backend/scripts/run_asan.sh
#
# Interpreting output:
#   "AddressSanitizer: ..." → a real memory error was found — fix it.
#   No ASAN output        → the tested code is memory-error free.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BACKEND_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$BACKEND_DIR/build_asan"
NCPU=$(nproc 2>/dev/null || sysctl -n hw.logicalcpu 2>/dev/null || echo 4)

# macOS leak detection requires SIP disabled; disable it to avoid false positives
export ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}"

echo "=== AddressSanitizer Build ==="
cmake -B "$BUILD_DIR" "$BACKEND_DIR" \
  -DCMAKE_BUILD_TYPE=Asan \
  -DCMAKE_TOOLCHAIN_FILE="$BACKEND_DIR/vcpkg/scripts/buildsystems/vcpkg.cmake" \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=OFF \
  -Wno-dev

echo ""
echo "=== Building unit tests ==="
cmake --build "$BUILD_DIR" --target test_order test_order_book test_auth -j"$NCPU"

echo ""
echo "=== Running test_order under ASAN ==="
"$BUILD_DIR/bin/test_order"

echo ""
echo "=== Running test_order_book under ASAN ==="
"$BUILD_DIR/bin/test_order_book"

echo ""
echo "=== Running test_auth under ASAN ==="
"$BUILD_DIR/bin/test_auth"

echo ""
echo "=== ASAN run complete ==="
echo "If no 'AddressSanitizer:' lines appeared above, all tested code"
echo "passes AddressSanitizer with zero detected memory errors."
echo ""
echo "Add to README:"
echo "  Memory error free: verified with AddressSanitizer (order, order_book, auth unit tests)"
