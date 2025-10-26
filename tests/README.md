# fsipd Test Suite

This directory contains the test suite for fsipd, organized following C testing best practices.

## Directory Structure

```
tests/
├── unit/                  # Unit tests for individual functions
│   └── test_chomp.c       # Tests for chomp() function
├── integration/           # Integration tests for the daemon
│   └── test_daemon.sh     # End-to-end daemon tests
└── run_tests.sh           # Main test runner script
```

## Running Tests

### Run All Tests

```bash
make test
```

or

```bash
make check
```

### Run Unit Tests Only

```bash
make test-unit
```

### Run Integration Tests Only

```bash
make test-integration
```

### Run Individual Tests

```bash
# Unit test
./tests/unit/test_chomp

# Logfile test
./logfile_test

# Integration test
./tests/integration/test_daemon.sh
```

## Test Types

### Unit Tests

Unit tests are written in C and test individual functions in isolation. They use simple assert statements and return non-zero on failure.

**Location:** `tests/unit/`

**Example:** `test_chomp.c` tests the `chomp()` string trimming function with various inputs including edge cases.

### Integration Tests

Integration tests are shell scripts that test the complete daemon functionality including:
- Daemon startup and shutdown
- Network communication (TCP/UDP, IPv4/IPv6)
- Log file format validation
- Concurrent connection handling
- SIP message processing

**Location:** `tests/integration/`

**Requirements:** These tests require `netcat` (nc) to be installed.

## Writing New Tests

### Adding a Unit Test

1. Create a new `.c` file in `tests/unit/`
2. Include necessary headers and the function(s) to test
3. Write test functions using assertions
4. Add a `main()` function that calls all tests
5. Update `Makefile` to build the new test
6. Update `run_tests.sh` to run the new test

Example:
```c
void test_myfunction() {
    assert(myfunction(input) == expected);
    printf("[PASS] test_myfunction\n");
}

int main() {
    test_myfunction();
    return 0;
}
```

### Adding an Integration Test

1. Create a new `.sh` file in `tests/integration/`
2. Follow the pattern in `test_daemon.sh`
3. Include proper cleanup in a trap handler
4. Use descriptive test names and logging
5. Return non-zero on failure

## CI Integration

The test suite runs automatically on:
- Every push to master/main branches
- Every pull request

Tests run on multiple platforms:
- Ubuntu (latest)
- macOS (latest)

With multiple compilers:
- GCC
- Clang

## Test Coverage

Current test coverage includes:
- String manipulation (chomp function)
- Log file operations
- Daemon lifecycle
- SIP message handling (INVITE, REGISTER, OPTIONS)
- IPv4 and IPv6 support
- TCP and UDP protocols
- CSV log format validation
- Concurrent connection handling

## Troubleshooting

### Tests fail with "nc: command not found"

Install netcat:
- Ubuntu/Debian: `apt-get install netcat-openbsd`
- macOS: `brew install netcat` (or use built-in nc)
- RHEL/CentOS: `yum install nc`

### Integration tests timeout

Increase the timeout values in `test_daemon.sh`:
```bash
DAEMON_TIMEOUT=10
MESSAGE_TIMEOUT=5
```

### Permission denied errors

Integration tests create files in `/tmp`. Ensure you have write access to this directory.
