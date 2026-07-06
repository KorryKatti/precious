#!/bin/bash

COMPILER="./build/precious"
TESTS_DIR="tests"
PASS=0
FAIL=0
TOTAL=0

run_test() {
    local file="$1"
    local expected="$2"
    local name=$(basename "$file" .precious)
    TOTAL=$((TOTAL + 1))

    if ! $COMPILER "$file" > /dev/null 2>&1; then
        echo "FAIL [$name] - compilation error (expected exit: $expected)"
        FAIL=$((FAIL + 1))
        return
    fi

    set +e
    ./out > /dev/null 2>&1
    local actual=$?
    set -e

    if [ "$actual" -eq "$expected" ]; then
        echo "PASS [$name] - exit code: $actual"
        PASS=$((PASS + 1))
    else
        echo "FAIL [$name] - expected: $expected, got: $actual"
        FAIL=$((FAIL + 1))
    fi
}

run_test_output() {
    local file="$1"
    local expected_output="$2"
    local expected_exit="$3"
    local name=$(basename "$file" .precious)
    TOTAL=$((TOTAL + 1))

    if ! $COMPILER "$file" > /dev/null 2>&1; then
        echo "FAIL [$name] - compilation error"
        FAIL=$((FAIL + 1))
        return
    fi

    set +e
    local actual_output
    actual_output=$(./out 2>&1)
    local actual_exit=$?
    set -e

    if [ "$actual_exit" -ne "$expected_exit" ]; then
        echo "FAIL [$name] - exit code: expected $expected_exit, got $actual_exit"
        FAIL=$((FAIL + 1))
        return
    fi

    if [ "$actual_output" = "$expected_output" ]; then
        echo "PASS [$name] - output matches, exit: $actual_exit"
        PASS=$((PASS + 1))
    else
        echo "FAIL [$name] - output mismatch"
        echo "  expected: $expected_output"
        echo "  got:      $actual_output"
        FAIL=$((FAIL + 1))
    fi
}

echo "=== Precious Language Test Suite ==="
echo ""

# Basic arithmetic
run_test "$TESTS_DIR/01_basic_add.precious" 13
run_test "$TESTS_DIR/02_basic_sub.precious" 7
run_test "$TESTS_DIR/03_basic_mul.precious" 20
run_test "$TESTS_DIR/04_basic_div.precious" 5
run_test "$TESTS_DIR/05_literal_exit.precious" 42
run_test "$TESTS_DIR/06_inline_expr.precious" 5

# Precedence
run_test "$TESTS_DIR/07_precedence_mul_add.precious" 14
run_test "$TESTS_DIR/08_parens_override.precious" 20
run_test "$TESTS_DIR/09_mixed_precedence.precious" 17

# Scopes
run_test "$TESTS_DIR/10_scope_access.precious" 15
run_test "$TESTS_DIR/11_scope_inner_var.precious" 10

# If/elif/else
run_test "$TESTS_DIR/12_if_true.precious" 10
run_test "$TESTS_DIR/13_if_false_else.precious" 20
run_test "$TESTS_DIR/14_if_elif_else.precious" 30
run_test "$TESTS_DIR/15_multi_elif.precious" 30

# Comments
run_test "$TESTS_DIR/16_comments.precious" 42

# Complex expressions
run_test "$TESTS_DIR/17_complex_arithmetic.precious" 12
run_test "$TESTS_DIR/18_var_in_expr.precious" 21

# Nested scopes
run_test "$TESTS_DIR/19_nested_scopes.precious" 6

# More if/scope combos
run_test "$TESTS_DIR/20_if_with_inner_var.precious" 99
run_test "$TESTS_DIR/21_sub_chain.precious" 25
run_test "$TESTS_DIR/22_mul_chain.precious" 24
run_test "$TESTS_DIR/23_all_false_else.precious" 99
run_test "$TESTS_DIR/24_if_true_with_var.precious" 10

# Stack bug test
run_test "$TESTS_DIR/25_if_else_stack_bug.precious" 5

# Misc
run_test "$TESTS_DIR/26_scope_exit_val.precious" 30
run_test "$TESTS_DIR/27_nested_if.precious" 77
run_test "$TESTS_DIR/28_if_true_elif_skip.precious" 10
run_test "$TESTS_DIR/29_div_var.precious" 5
run_test "$TESTS_DIR/30_parens_multiply.precious" 49

# While loops
run_test "$TESTS_DIR/31_while_basic.precious" 5

# Say/print
run_test_output "$TESTS_DIR/32_say_basic.precious" "$(printf '42\n50\n300')" 0

# Complex bool + say
run_test_output "$TESTS_DIR/test_complex_bool.precious" "$(printf '48\n18\n3\n100')" 100

# Functions
run_test_output "$TESTS_DIR/33_fn_basic.precious" "7" 0
run_test_output "$TESTS_DIR/34_fn_say.precious" "99" 0
run_test_output "$TESTS_DIR/35_fn_multiple.precious" "$(printf '10\n20')" 0
run_test_output "$TESTS_DIR/36_fn_with_vars.precious" "5" 0
run_test_output "$TESTS_DIR/37_fn_call_in_say.precious" "$(printf '10\n42')" 0

echo ""
echo "=== Results: $PASS passed, $FAIL failed, $TOTAL total ==="

if [ "$FAIL" -eq 0 ]; then
    echo "All tests passed!"
else
    echo "Some tests failed."
    exit 1
fi
