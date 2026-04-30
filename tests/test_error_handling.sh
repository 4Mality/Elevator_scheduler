#!/bin/bash
# Verifies that scheduler_os fails gracefully on bad inputs.
# Run from the project root: bash tests/test_error_handling.sh

BINARY="./scheduler_os"
PASS=0
FAIL=0

check() {
    local desc="$1"
    local expected_exit="$2"
    shift 2
    "$BINARY" "$@" 2>/dev/null
    local got=$?
    if [ "$got" -ne 0 ] && [ "$expected_exit" -ne 0 ]; then
        echo "PASS: $desc"
        ((PASS++))
    elif [ "$got" -eq 0 ] && [ "$expected_exit" -eq 0 ]; then
        echo "PASS: $desc"
        ((PASS++))
    else
        echo "FAIL: $desc (expected exit $expected_exit, got $got)"
        ((FAIL++))
    fi
}

if [ ! -f "$BINARY" ]; then
    echo "ERROR: $BINARY not found. Run make first."
    exit 1
fi

# No arguments
check "no arguments exits non-zero" 1

# Missing port
check "missing port exits non-zero" 1 tests/simple.bldg

# Missing building file
check "missing building file exits non-zero" 1 9999

# Building file does not exist
check "nonexistent building file exits non-zero" 1 nonexistent.bldg 5432

echo ""
echo "Results: $PASS passed, $FAIL failed"
[ "$FAIL" -eq 0 ] && exit 0 || exit 1
