#!/bin/bash

COMPILER="./build/precious"
TESTS_DIR="tests"
PASS=0
FAIL=0
SKIP=0
TOTAL=0

# A local HTTP server started for a test has to go away again, whatever happens.
# This is the belt to the braces in run_test_http_server()'s suspenders.
SERVER_PID=""
cleanup_server() {
    if [ -n "$SERVER_PID" ]; then
        # `|| true` on both: a killed server makes `kill` and `wait` report
        # failure, and under `set -e` that would take the script down with it.
        kill "$SERVER_PID" 2> /dev/null || true
        wait "$SERVER_PID" 2> /dev/null || true
        SERVER_PID=""
    fi
}
trap cleanup_server EXIT

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
    ./out/$name > /dev/null 2>&1
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
    actual_output=$(./out/$name 2>&1)
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

run_test_stdin() {
    local file="$1"
    local stdin_data="$2"
    local expected_output="$3"
    local expected_exit="$4"
    local name=$(basename "$file" .precious)
    TOTAL=$((TOTAL + 1))

    if ! $COMPILER "$file" > /dev/null 2>&1; then
        echo "FAIL [$name] - compilation error"
        FAIL=$((FAIL + 1))
        return
    fi

    set +e
    local actual_output
    actual_output=$(printf '%s' "$stdin_data" | ./out/$name 2>&1)
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

run_expect_rejected() {
    local file="$1"
    local name=$(basename "$file" .precious)
    TOTAL=$((TOTAL + 1))

    # The compiler must exit non-zero: either the parser rejected the program,
    # or g++ rejected what the compiler generated.
    if $COMPILER "$file" > /dev/null 2>&1; then
        echo "FAIL [$name] - was accepted, but it should have been rejected"
        FAIL=$((FAIL + 1))
        return
    fi

    echo "PASS [$name] - correctly rejected"
    PASS=$((PASS + 1))
}

# A test that needs an HTTP server of its own, so the networking happy path can
# be checked without touching the internet.
#
# It serves a throwaway directory on a fixed port, waits for the port to actually
# accept connections rather than guessing with a sleep, and takes the server down
# again afterwards. A test that is skipped says so, and is counted as skipped --
# never as passed.
run_test_http_server() {
    local file="$1"
    local port="$2"
    local expected_output="$3"
    local name=$(basename "$file" .precious)
    TOTAL=$((TOTAL + 1))

    if ! command -v python3 > /dev/null 2>&1; then
        echo "SKIP [$name] - no python3, so no local server to talk to"
        SKIP=$((SKIP + 1))
        return
    fi

    if ! $COMPILER "$file" > /dev/null 2>&1; then
        echo "FAIL [$name] - compilation error"
        FAIL=$((FAIL + 1))
        return
    fi

    local fixture
    fixture=$(mktemp -d)
    printf 'the ring is precious\n' > "$fixture/page.txt"

    python3 -m http.server "$port" --directory "$fixture" > /dev/null 2>&1 &
    SERVER_PID=$!

    # Wait for the port, up to ten seconds. Opening a connection is the only
    # reliable signal that the server is up.
    #
    # The `:` matters: `exec 3<>/dev/tcp/...` would do the same job, but when the
    # redirection fails a non-interactive shell *exits* -- which would take this
    # whole script down on the first failed attempt. `:` fails quietly instead.
    local waited=0
    until (: < /dev/tcp/127.0.0.1/"$port") 2> /dev/null; do
        sleep 0.2
        waited=$((waited + 1))
        if [ "$waited" -gt 50 ]; then
            cleanup_server
            rm -rf "$fixture"
            echo "FAIL [$name] - the local server never came up on port $port"
            FAIL=$((FAIL + 1))
            return
        fi
    done

    set +e
    local actual_output
    actual_output=$(timeout 30 ./out/"$name" 2>&1)
    local actual_exit=$?
    set -e

    cleanup_server
    rm -rf "$fixture"

    if [ "$actual_exit" -ne 0 ]; then
        echo "FAIL [$name] - exit code: expected 0, got $actual_exit"
        FAIL=$((FAIL + 1))
        return
    fi

    if [ "$actual_output" = "$expected_output" ]; then
        echo "PASS [$name] - output matches, exit: 0"
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

# Function parameters (5b)
run_test_output "$TESTS_DIR/38_fn_params.precious" "5" 0

# Function return values (5c)
run_test_output "$TESTS_DIR/39_fn_return.precious" "5" 0
run_test_output "$TESTS_DIR/40_fn_return_mixed.precious" "$(printf '30\n42')" 0
run_test_output "$TESTS_DIR/41_fn_return_if.precious" "$(printf '20\n30')" 0
run_test_output "$TESTS_DIR/61_fn_return_type_annotation.precious" "hi" 0

# String literals
run_test_output "$TESTS_DIR/42_string_literal.precious" "$(printf 'hello world\n42')" 0

# Type annotations (Feature 7)
run_test_output "$TESTS_DIR/43_type_string_explicit.precious" "hi" 0
run_test_output "$TESTS_DIR/44_type_string_inferred.precious" "hi" 0
run_test_output "$TESTS_DIR/45_type_number_explicit.precious" "42" 0
run_test_output "$TESTS_DIR/46_type_number_inferred.precious" "42" 0
run_test_output "$TESTS_DIR/47_type_mixed.precious" "$(printf '10\nhey')" 0
run_test_output "$TESTS_DIR/48_type_string_reassign.precious" "b" 0

# Function parameter type annotations
run_test_output "$TESTS_DIR/49_fn_param_string.precious" "precious" 0
run_test_output "$TESTS_DIR/50_fn_param_numbers.precious" "30" 0
run_test_output "$TESTS_DIR/51_fn_param_return.precious" "10" 0
run_test_output "$TESTS_DIR/52_fn_param_string_var.precious" "gollum" 0

# Arrays
run_test_output "$TESTS_DIR/53_array_literal.precious" "$(printf '10\n20\n30')" 0
run_test_output "$TESTS_DIR/54_array_assignment.precious" "$(printf '2\n99')" 0
run_test_output "$TESTS_DIR/55_array_string.precious" "$(printf 'hello\nworld')" 0
run_test_output "$TESTS_DIR/56_array_variable_index.precious" "20" 0
run_test_output "$TESTS_DIR/58_fn_array_param_simple.precious" "10" 0
run_test_output "$TESTS_DIR/59_fn_array_param_sum.precious" "20" 0
run_test_output "$TESTS_DIR/60_fn_array_param_modify.precious" "2" 0


# Modulo
run_test_output "$TESTS_DIR/57_modulo.precious" "$(printf '1\n0\n3')" 0

# Switch / Case
run_test_output "$TESTS_DIR/71_switch_matching_case.precious" "20" 0
run_test_output "$TESTS_DIR/72_switch_to_default.precious" "99" 0
run_test_output "$TESTS_DIR/73_switch_multiple_cases.precious" "300" 0
run_test_output "$TESTS_DIR/74_switch_no_default.precious" "0" 0
run_test_output "$TESTS_DIR/75_switch_var_expr.precious" "60" 0
run_test_output "$TESTS_DIR/76_switch_nested.precious" "20" 0

# For loop
run_test "$TESTS_DIR/80_for_loop.precious" 10

# For-each loop
run_test "$TESTS_DIR/81_for_each.precious" 100

# String concat
run_test_output "$TESTS_DIR/82_string_concat.precious" "hello world" 0

# String indexing
run_test_output "$TESTS_DIR/83_string_index.precious" "$(printf 'h\no')" 0

# Array push/pop
run_test_output "$TESTS_DIR/84_array_push_pop.precious" "$(printf '4\n3')" 0

# String concat reassignment
run_test_output "$TESTS_DIR/85_string_concat_reassign.precious" "foobarbaz" 0

# Array push with strings
run_test_output "$TESTS_DIR/86_array_push_string.precious" "$(printf 'foo\nworld')" 0

# Compound assignment (+=, -=, *=, /=, %=)
run_test_output "$TESTS_DIR/87_compound_ops.precious" "2" 2
run_test "$TESTS_DIR/88_compound_loop.precious" 55
run_test_output "$TESTS_DIR/89_compound_string.precious" "hello precious" 6

# Increment / decrement (++, --)
run_test_output "$TESTS_DIR/90_incdec_post.precious" "$(printf '5\n6\n6\n5')" 0
run_test_output "$TESTS_DIR/91_incdec_pre.precious" "$(printf '6\n5')" 0
run_test_output "$TESTS_DIR/92_incdec_statement.precious" "0" 0
run_test_output "$TESTS_DIR/93_incdec_for_loop.precious" "10" 0
run_test_output "$TESTS_DIR/94_incdec_loops.precious" "$(printf '3\n2\n1\nliftoff\n203\n-3\n4')" 0

# Input from stdin (ask)
run_test_stdin "$TESTS_DIR/95_ask_string.precious" "$(printf 'precious\nhello\n')" "hello, precious!" 0
run_test_stdin "$TESTS_DIR/96_ask_number.precious" "$(printf '17\n25\n')" "$(printf 'first? second? 17\n25\n42')" 0
run_test_stdin "$TESTS_DIR/97_ask_mixed.precious" "$(printf 'gollum\n6\n7\n99\n')" "$(printf 'gollum\n42\n99')" 0
run_test_stdin "$TESTS_DIR/98_ask_expression.precious" "$(printf '21\nword\n')" "$(printf 'value? 42\ninline? word')" 21

# Every reader in the standard library: word, number, decimal, letter
run_test_stdin "$TESTS_DIR/107_ask_types.precious" "$(printf 'frodo\n7\n2.5\ng\nARGH\nquiet\n')" "$(printf 'name? number? decimal? letter? gollum says: frodo\n7\n2.5\ng\nARGH\nquiet')" 0

# Gollum mutter comments (;; and ;;;)
run_test "$TESTS_DIR/99_mutter_comments.precious" 42

# the_ring hidden global state
run_test_output "$TESTS_DIR/100_the_ring.precious" "$(printf '0\n5\n6\n60')" 60

# len()
run_test_output "$TESTS_DIR/101_len.precious" "$(printf '5\n3\n2\n6\nh\ne\nl\nl\no')" 5

# str() / num() / decimal()
run_test_output "$TESTS_DIR/102_conversions.precious" "$(printf '42\n-7\n2.5\nprecious\n124\n0\n3\n3.14\n6.28\n2024')" 42

# Power operator **
run_test_output "$TESTS_DIR/103_power.precious" "$(printf '1024\n27\n512\n32\n54\n49\n1')" 8

# Character literals
run_test_output "$TESTS_DIR/104_char_literal.precious" "g
G
104
1
1
1
1
39
92
g
o
l
l
u
m" 103

# Gollum numerals
run_test_output "$TESTS_DIR/105_gollum_numerals.precious" "$(printf '7\n21\n90\n0\n17\n80\n6')" 30

# import "networking" -- header and linker flags for a native package
run_test_output "$TESTS_DIR/106_import_networking.precious" "one ring to " 12

# http_get / http_code against a port nothing is listening on: fails instantly
# and offline, so the suite never depends on the internet
run_test_output "$TESTS_DIR/108_http_refused.precious" "$(printf '0\n0\n1\n0')" 0

# http_get / http_code against a local server this script starts and stops.
# The port is fixed because a Precious program cannot read a command-line
# argument yet, so the test file has to know where to look.
run_test_http_server "$TESTS_DIR/109_http_get_local.precious" 8731 "200
21
the ring is precious

404
1"

# Type checker: types used the way they should be must still compile
run_test_stdin "$TESTS_DIR/type_ok_mixed.precious" "$(printf 'frodo\n9\n')" "$(printf '1\n3\np\nhello there\n1\nfrodo\nfrodo\nsam\n60\n12\n2\n1\nwho? how many? frodo\n9\n0\n1')" 0

# Programs the compiler must refuse
echo ""
echo "--- Error Tests ---"
for err_test in "$TESTS_DIR"/err_*.precious; do
    run_expect_rejected "$err_test"
done
# Not named err_* but is an error test: `my x` twice in one block. Redeclaring
# in the same block is rejected; only a nested block may reuse the name.
run_expect_rejected "$TESTS_DIR/11_scope_shadow.precious"

# DSA / LeetCode problems
echo ""
echo "--- DSA Tests ---"
run_test_output "$TESTS_DIR/dsa/01_kadane.precious" "6" 6
run_test_output "$TESTS_DIR/dsa/02_container_most_water.precious" "49" 49
run_test_output "$TESTS_DIR/dsa/03_best_time_buy_sell.precious" "5" 5
run_test_output "$TESTS_DIR/dsa/04_climbing_stairs.precious" "8" 8
run_test_output "$TESTS_DIR/dsa/05_two_sum.precious" "$(printf '0\n1')" 1
run_test_output "$TESTS_DIR/dsa/06_merge_sorted.precious" "$(printf '1\n2\n2\n3\n5\n6')" 1
run_test_output "$TESTS_DIR/dsa/07_trapping_rain_water.precious" "6" 6
run_test_output "$TESTS_DIR/dsa/08_binary_search.precious" "6" 6
run_test_output "$TESTS_DIR/dsa/09_three_sum.precious" "3" 3
run_test_output "$TESTS_DIR/dsa/10_plus_one.precious" "$(printf '1\n2\n4')" 1

echo ""
echo "=== Results: $PASS passed, $FAIL failed, $SKIP skipped, $TOTAL total ==="

if [ "$FAIL" -eq 0 ]; then
    echo "All tests passed!"
else
    echo "Some tests failed."
    exit 1
fi
