[![Test](https://github.com/farrokhi/fsipd/workflows/Test/badge.svg)](https://github.com/farrokhi/fsipd/actions)

# fsipd

Fake SIP Daemon - A minimal SIP honeypot

Version 1.1.0

## Overview

fsipd is a lightweight SIP honeypot that listens on TCP/UDP port 5060 for both IPv4 and IPv6 (when available). It logs all incoming SIP requests with source/destination IP addresses and port numbers in CSV format.

## Features

- Dual protocol support (TCP and UDP)
- IPv4 and IPv6 support
- Multi-threaded architecture
- CSV logging for easy analysis
- Syslog integration option
- Signal-safe log rotation (SIGHUP)
- Sanitizes multiline messages to preserve CSV integrity
- Configurable log and PID file paths

## Log Format

Logs are written in CSV format with the following fields:

```
epoch timestamp, protocol, src ip, src port, "message"
```

Example:

```
1445775973,UDP4,127.0.0.1,50751,"INVITE sip:user@example.com SIP/2.0"
1445775974,TCP6,::1,54321,"REGISTER sip:example.com SIP/2.0"
```

Note: Multiline SIP messages are sanitized by replacing newlines and control characters with spaces to maintain CSV format integrity.

## Usage

```
fsipd [-hv] [-l logfile] [-s] [-p priority] [-f pidfile]
    -h: Show help message
    -v: Show version
    -s: Use syslog instead of local log file
    -p: Syslog priority (default: user.notice)
    -l: Specify output log filename (default: fsipd.log)
    -f: Specify PID file path (default: /var/run/fsipd.pid)
```

## Building

```bash
make
```

## Testing

Run the test suite:

```bash
make test
```

This runs both unit tests and integration tests.

## Dependencies

This program depends on:
- [libpidutil](https://github.com/farrokhi/libpidutil)
- pthread library

## License

SPDX-License-Identifier: BSD-2-Clause

This project is licensed under the BSD 2-Clause License. See the [LICENSE](LICENSE) file for details.
