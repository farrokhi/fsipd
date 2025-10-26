#!/bin/sh
#
# Test suite for fsipd
# Can be run locally or in CI environments
#

set -e

# Configuration
TEST_LOG="/tmp/fsipd_test_$$.log"
TEST_PID="/tmp/fsipd_test_$$.pid"
DAEMON_TIMEOUT=5
MESSAGE_TIMEOUT=2

# Test results
TESTS_RUN=0
TESTS_PASSED=0
TESTS_FAILED=0

# Colors for output (disabled in CI)
if [ -t 1 ] && [ -z "$CI" ]; then
    RED='\033[0;31m'
    GREEN='\033[0;32m'
    YELLOW='\033[0;33m'
    NC='\033[0m'
else
    RED=''
    GREEN=''
    YELLOW=''
    NC=''
fi

# Cleanup function
cleanup() {
    if [ -f "$TEST_PID" ]; then
        PID=$(cat "$TEST_PID")
        kill "$PID" 2>/dev/null || true
        sleep 1
        kill -9 "$PID" 2>/dev/null || true
    fi
    rm -f "$TEST_LOG" "$TEST_PID"
}

trap cleanup EXIT INT TERM

# Logging functions
log_info() {
    printf "${YELLOW}[INFO]${NC} %s\n" "$1"
}

log_pass() {
    printf "${GREEN}[PASS]${NC} %s\n" "$1"
    TESTS_PASSED=$((TESTS_PASSED + 1))
}

log_fail() {
    printf "${RED}[FAIL]${NC} %s\n" "$1"
    TESTS_FAILED=$((TESTS_FAILED + 1))
}

# Test assertion functions
assert_file_exists() {
    TESTS_RUN=$((TESTS_RUN + 1))
    if [ -f "$1" ]; then
        log_pass "File exists: $1"
        return 0
    else
        log_fail "File does not exist: $1"
        return 1
    fi
}

assert_process_running() {
    TESTS_RUN=$((TESTS_RUN + 1))
    if ps -p "$1" > /dev/null 2>&1; then
        log_pass "Process $1 is running"
        return 0
    else
        log_fail "Process $1 is not running"
        return 1
    fi
}

assert_log_contains() {
    TESTS_RUN=$((TESTS_RUN + 1))
    if grep -q "$1" "$TEST_LOG" 2>/dev/null; then
        log_pass "Log contains: $1"
        return 0
    else
        log_fail "Log does not contain: $1"
        return 1
    fi
}

assert_log_line_count() {
    TESTS_RUN=$((TESTS_RUN + 1))
    COUNT=$(wc -l < "$TEST_LOG" 2>/dev/null | tr -d ' ')
    if [ "$COUNT" -ge "$1" ]; then
        log_pass "Log has at least $1 lines (found $COUNT)"
        return 0
    else
        log_fail "Log has fewer than $1 lines (found $COUNT)"
        return 1
    fi
}

assert_csv_format() {
    TESTS_RUN=$((TESTS_RUN + 1))
    if [ ! -f "$TEST_LOG" ] || [ ! -s "$TEST_LOG" ]; then
        log_fail "Log file is empty or does not exist"
        return 1
    fi
    if awk -F, 'NF != 5 {print "Line " NR " has " NF " fields instead of 5: " $0; exit 1}' "$TEST_LOG" 2>/dev/null; then
        log_pass "Log entries are valid CSV with 5 fields"
        return 0
    else
        log_fail "Log entries are not valid CSV format"
        cat "$TEST_LOG"
        return 1
    fi
}

# Check if netcat is available
check_netcat() {
    if command -v nc >/dev/null 2>&1; then
        return 0
    else
        log_info "netcat (nc) not found, skipping network tests"
        return 1
    fi
}

# Send SIP message via UDP
send_udp_message() {
    local MESSAGE="$1"
    local HOST="${2:-127.0.0.1}"
    local PORT="${3:-5060}"

    echo "$MESSAGE" | nc -u -w1 "$HOST" "$PORT" 2>/dev/null || true
}

# Send SIP message via TCP
send_tcp_message() {
    local MESSAGE="$1"
    local HOST="${2:-127.0.0.1}"
    local PORT="${3:-5060}"

    echo "$MESSAGE" | nc -w1 "$HOST" "$PORT" 2>/dev/null || true
}

# Test: Build the project
test_build() {
    log_info "Test: Building fsipd..."
    if make clean && make; then
        log_pass "Build successful"
        return 0
    else
        log_fail "Build failed"
        return 1
    fi
}

# Test: Start daemon
test_daemon_start() {
    log_info "Test: Starting daemon..."

    ./fsipd -l "$TEST_LOG" -f "$TEST_PID" &
    sleep "$DAEMON_TIMEOUT"

    assert_file_exists "$TEST_PID" || return 1

    PID=$(cat "$TEST_PID")
    assert_process_running "$PID" || return 1

    return 0
}

# Test: SIP INVITE via UDP
test_sip_invite_udp() {
    log_info "Test: SIP INVITE via UDP..."

    if ! check_netcat; then
        return 0
    fi

    send_udp_message "INVITE sip:user@example.com SIP/2.0"

    sleep "$MESSAGE_TIMEOUT"

    assert_log_contains "INVITE" || return 1
    assert_log_contains "UDP4" || return 1

    return 0
}

# Test: SIP REGISTER via TCP
test_sip_register_tcp() {
    log_info "Test: SIP REGISTER via TCP..."

    if ! check_netcat; then
        return 0
    fi

    send_tcp_message "REGISTER sip:example.com SIP/2.0"

    sleep "$MESSAGE_TIMEOUT"

    assert_log_contains "REGISTER" || return 1
    assert_log_contains "TCP4" || return 1

    return 0
}

# Test: SIP OPTIONS via UDP
test_sip_options_udp() {
    log_info "Test: SIP OPTIONS via UDP..."

    if ! check_netcat; then
        return 0
    fi

    send_udp_message "OPTIONS sip:example.com SIP/2.0"

    sleep "$MESSAGE_TIMEOUT"

    assert_log_contains "OPTIONS" || return 1

    return 0
}

# Test: IPv6 support (if available)
test_ipv6_support() {
    log_info "Test: IPv6 support..."

    if ! check_netcat; then
        return 0
    fi

    if nc -6 -u -w1 ::1 5060 < /dev/null 2>/dev/null; then
        send_udp_message "INVITE sip:user@[::1] SIP/2.0" "::1"
        sleep "$MESSAGE_TIMEOUT"

        if assert_log_contains "UDP6"; then
            assert_log_contains "::1" || return 1
        fi
    else
        log_info "IPv6 not available, skipping"
    fi

    return 0
}

# Test: Log format validation
test_log_format() {
    log_info "Test: Log format validation..."

    if [ ! -f "$TEST_LOG" ]; then
        log_info "No log file, skipping format validation"
        return 0
    fi

    assert_csv_format || return 1

    # Validate timestamp format (epoch)
    TESTS_RUN=$((TESTS_RUN + 1))
    if awk -F, '$1 ~ /^[0-9]+$/ {count++} END {exit(count>0?0:1)}' "$TEST_LOG"; then
        log_pass "Timestamps are valid epoch values"
    else
        log_fail "Invalid timestamp format"
        return 1
    fi

    # Validate protocol field
    TESTS_RUN=$((TESTS_RUN + 1))
    if awk -F, '$2 ~ /^(UDP4|TCP4|UDP6|TCP6)$/ {count++} END {exit(count>0?0:1)}' "$TEST_LOG"; then
        log_pass "Protocol fields are valid"
    else
        log_fail "Invalid protocol format"
        return 1
    fi

    return 0
}

# Test: Multiple concurrent connections
test_concurrent_connections() {
    log_info "Test: Multiple concurrent connections..."

    if ! check_netcat; then
        return 0
    fi

    # Send multiple messages in parallel
    send_udp_message "INVITE sip:user1@example.com SIP/2.0" &
    send_tcp_message "REGISTER sip:user2@example.com SIP/2.0" &
    send_udp_message "OPTIONS sip:user3@example.com SIP/2.0" &

    wait
    sleep "$MESSAGE_TIMEOUT"

    assert_log_line_count 3 || return 1

    return 0
}

# Test: Special characters handling
test_special_characters() {
    log_info "Test: Special characters in messages..."

    if ! check_netcat; then
        return 0
    fi

    send_udp_message "INVITE sip:user@example.com;param=value?header=data SIP/2.0"
    sleep "$MESSAGE_TIMEOUT"

    assert_log_contains "INVITE" || return 1

    return 0
}

# Test: Multiline message handling
test_multiline_messages() {
    log_info "Test: Multiline message handling..."

    if ! check_netcat; then
        return 0
    fi

    # Send multiline SIP message via UDP
    printf "INVITE sip:user@example.com SIP/2.0\r\nVia: SIP/2.0/UDP test\r\n\r\n" | nc -u -w1 127.0.0.1 5060 2>/dev/null || true
    sleep "$MESSAGE_TIMEOUT"

    # Log should contain the message on a single line
    TESTS_RUN=$((TESTS_RUN + 1))
    LINE_COUNT=$(wc -l < "$TEST_LOG" 2>/dev/null | tr -d ' ')
    EXPECTED_LINES=1

    # Account for previous test messages
    if [ -f "$TEST_LOG" ]; then
        # Check that the last line contains both parts of the message
        LAST_LINE=$(tail -1 "$TEST_LOG")
        if echo "$LAST_LINE" | grep -q "INVITE" && echo "$LAST_LINE" | grep -q "Via"; then
            log_pass "Multiline message on single CSV line"
        else
            log_fail "Multiline message not properly sanitized"
            return 1
        fi

        # Verify no embedded newlines broke CSV
        if awk -F, 'NF != 5 {exit 1}' "$TEST_LOG" 2>/dev/null; then
            log_pass "CSV format preserved with multiline input"
        else
            log_fail "CSV format broken by multiline message"
            return 1
        fi
    else
        log_fail "Log file not found"
        return 1
    fi

    return 0
}

# Test: Control character sanitization
test_control_characters() {
    log_info "Test: Control character sanitization..."

    if ! check_netcat; then
        return 0
    fi

    # Send message with control characters
    printf "OPTIONS\x01\x02\x1fsip:test@example.com SIP/2.0\n" | nc -u -w1 127.0.0.1 5060 2>/dev/null || true
    sleep "$MESSAGE_TIMEOUT"

    assert_log_contains "OPTIONS" || return 1

    # Verify CSV format is still valid
    TESTS_RUN=$((TESTS_RUN + 1))
    if awk -F, 'NF != 5 {exit 1}' "$TEST_LOG" 2>/dev/null; then
        log_pass "CSV format preserved with control characters"
        return 0
    else
        log_fail "CSV format broken by control characters"
        return 1
    fi
}

# Test: Daemon shutdown
test_daemon_shutdown() {
    log_info "Test: Daemon shutdown..."

    if [ ! -f "$TEST_PID" ]; then
        log_info "No PID file, skipping shutdown test"
        return 0
    fi

    PID=$(cat "$TEST_PID")
    kill "$PID" 2>/dev/null
    sleep 2

    TESTS_RUN=$((TESTS_RUN + 1))
    if ! ps -p "$PID" > /dev/null 2>&1; then
        log_pass "Daemon stopped cleanly"
        return 0
    else
        log_fail "Daemon still running after SIGTERM"
        return 1
    fi
}

# Main test execution
main() {
    log_info "Starting fsipd test suite..."
    echo ""

    # Run tests in order
    test_build
    test_daemon_start
    test_sip_invite_udp
    test_sip_register_tcp
    test_sip_options_udp
    test_ipv6_support
    test_log_format
    test_concurrent_connections
    test_special_characters
    test_multiline_messages
    test_control_characters
    test_daemon_shutdown

    # Print summary
    echo ""
    echo "======================================"
    echo "Test Summary"
    echo "======================================"
    printf "Total tests:  %d\n" "$TESTS_RUN"
    printf "${GREEN}Passed:       %d${NC}\n" "$TESTS_PASSED"
    printf "${RED}Failed:       %d${NC}\n" "$TESTS_FAILED"
    echo "======================================"

    if [ "$TESTS_FAILED" -eq 0 ]; then
        echo ""
        log_pass "All tests passed!"
        return 0
    else
        echo ""
        log_fail "$TESTS_FAILED test(s) failed"
        return 1
    fi
}

main
