#!/bin/bash
# Parity test: C++ vs Rust side-by-side comparison
set -e

echo "=== SagaTools Parity Test (C++ vs Rust) ==="
echo ""

# Paths
CPP_TOOLS="../SagaTools/bin/tools"
RUST_TOOLS="./target/debug/sagatools"

# Check if binaries exist
if [ ! -f "$CPP_TOOLS" ]; then
    echo "ERROR: C++ binary not found at $CPP_TOOLS"
    exit 1
fi

if [ ! -f "$RUST_TOOLS" ]; then
    echo "ERROR: Rust binary not found at $RUST_TOOLS"
    exit 1
fi

# Commands to test
COMMANDS=("--help" "--version" "list" "which forge")

PASSED=0
FAILED=0

for cmd in "${COMMANDS[@]}"; do
    echo -n "Testing: tools $cmd ... "
    
    # Run C++ version
    cpp_out=$($CPP_TOOLS $cmd 2>&1)
    cpp_exit=$?
    
    # Run Rust version  
    rust_out=$($RUST_TOOLS $cmd 2>&1)
    rust_exit=$?
    
    # Compare exit codes
    if [ $cpp_exit -ne $rust_exit ]; then
        echo "FAIL (exit code mismatch: C++=$cpp_exit, Rust=$rust_exit)"
        FAILED=$((FAILED + 1))
        continue
    fi
    
    # Compare output (normalize line endings)
    if [ "$cpp_out" != "$rust_out" ]; then
        echo "FAIL (output differs)"
        echo "--- C++ output ---"
        echo "$cpp_out"
        echo "--- Rust output ---"
        echo "$rust_out"
        FAILED=$((FAILED + 1))
        continue
    fi
    
    echo "PASS"
    PASSED=$((PASSED + 1))
done

echo ""
echo "=== Results: $PASSED passed, $FAILED failed ==="

if [ $FAILED -gt 0 ]; then
    exit 1
fi
