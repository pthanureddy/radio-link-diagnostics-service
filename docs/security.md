# Security review

## Trust boundary

The UDP socket is the primary untrusted-input boundary. The decoder performs exact-size, magic, version, checksum, and metric checks before the analyzer receives a frame. Fixed-width parsing prevents delimiter ambiguity, and no received value controls memory allocation size.

## Controls implemented

- Exact 48-byte framing and explicit big-endian parsing
- CRC-32 corruption detection
- Frequency, bandwidth, EVM, and freshness policies
- Duplicate, missing, and out-of-order sequence detection
- RAII socket cleanup and bounded receive buffer
- Compiler warnings treated as errors
- AddressSanitizer and UndefinedBehaviorSanitizer CI job
- Non-root container user and hardened `systemd` sandbox directives

## Deliberate limitations

CRC-32 is not authentication. Anyone able to reach the UDP port can forge a syntactically valid frame, observe traffic, or replay a captured datagram. UDP can also be spoofed and offers no delivery guarantee.

Before production use, add mutually authenticated transport or frame-level signatures, key rotation, anti-replay windows, source authorization, rate limiting, metrics, and an operational incident response path. Bind only to intended interfaces and enforce network policy outside the process.

This repository handles synthetic values only and contains no credentials, personal data, classified material, or production endpoints.
