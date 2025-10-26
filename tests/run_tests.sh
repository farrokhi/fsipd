#!/bin/sh
#
# Main test runner for fsipd
# Runs both unit and integration tests
#

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Colors
if [ -t 1 ] && [ -z "$CI" ]; then
    GREEN='\033[0;32m'
    RED='\033[0;31m'
    YELLOW='\033[0;33m'
    NC='\033[0m'
else
    GREEN=''
    RED=''
    YELLOW=''
    NC=''
fi

TOTAL_SUITES=0
PASSED_SUITES=0
FAILED_SUITES=0

run_test_suite() {
    local TEST_NAME="$1"
    local TEST_CMD="$2"

    TOTAL_SUITES=$((TOTAL_SUITES + 1))
    printf "${YELLOW}[RUN]${NC} %s\n" "$TEST_NAME"

    if eval "$TEST_CMD"; then
        printf "${GREEN}[OK]${NC} %s\n\n" "$TEST_NAME"
        PASSED_SUITES=$((PASSED_SUITES + 1))
        return 0
    else
        printf "${RED}[FAIL]${NC} %s\n\n" "$TEST_NAME"
        FAILED_SUITES=$((FAILED_SUITES + 1))
        return 1
    fi
}

main() {
    cd "$PROJECT_ROOT"

    echo "======================================"
    echo "fsipd Test Suite"
    echo "======================================"
    echo ""

    # Run unit tests
    echo "Running unit tests..."
    echo "--------------------------------------"
    run_test_suite "Unit: chomp()" "./tests/unit/test_chomp"
    run_test_suite "Unit: logfile" "./logfile_test"

    # Run integration tests
    echo "Running integration tests..."
    echo "--------------------------------------"
    run_test_suite "Integration: daemon" "./tests/integration/test_daemon.sh"

    # Print summary
    echo ""
    echo "======================================"
    echo "Test Summary"
    echo "======================================"
    printf "Total suites: %d\n" "$TOTAL_SUITES"
    printf "${GREEN}Passed:       %d${NC}\n" "$PASSED_SUITES"
    printf "${RED}Failed:       %d${NC}\n" "$FAILED_SUITES"
    echo "======================================"

    if [ "$FAILED_SUITES" -eq 0 ]; then
        echo ""
        printf "${GREEN}All test suites passed!${NC}\n"
        return 0
    else
        echo ""
        printf "${RED}%d test suite(s) failed${NC}\n" "$FAILED_SUITES"
        return 1
    fi
}

main
