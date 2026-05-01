#!/usr/bin/env bash
# Run unit tests under AddressSanitizer to detect heap/stack memory errors.
#
# What it tests (no database required):
#   - test_order:       Order struct, status transitions, field constraints
#   - test_order_book:  Matching engine — add_order, match_order, cancel_order, get_snapshot
#   - test_auth:        JWT generate/verify, password hash/verify
#
# TSAN and ASAN must NOT be combined — use separate build directories.
# ASAN_OPTIONS=detect_leaks=0 avoids macOS false positives from system allocators.
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
NCPU=$(sysctl -n hw.logicalcpu 2>/dev/null || nproc 2>/dev/null || echo 4)

# Suppress macOS leak-detection false positives (requires SIP disabled to work accurately)
export ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}"

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
echo "=== AddressSanitizer Build ==="
cmake -B "$BUILD_DIR" "$BACKEND_DIR" \
  -DCMAKE_BUILD_TYPE=Asan \
  -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" \
  -DVCPKG_INSTALLED_DIR="$BACKEND_DIR/build/vcpkg_installed" \
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
echo "Update the README 'Results' line with your findings."
