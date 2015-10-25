# fsipd

fsipd - Fake SIP Daemon

fsipd is a minimal SIP honeypot. It listens on TCP/UDP 5060 and logs all incoming SIP requests along with SRC/DST Source and Port in CSV format.

## LOG Format

Incoming packets are logged in CSV format in "fsipd.log". Log format is described below:

`epoch, protocol, src ip, src port, "message"`

example:

`1445775973,UDP,127.0.0.1,50751,"INVITE"`

## Dependencies

This program depends on [libpidutil](https://github.com/farrokhi/libpidutil)
