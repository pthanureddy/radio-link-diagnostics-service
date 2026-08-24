# Binary protocol

Every UDP datagram contains exactly one 48-byte version-1 frame. Multi-byte integers use network byte order.

| Offset | Size | Field | Validation |
|---:|---:|---|---|
| 0 | 4 | Magic | ASCII `RLDS` |
| 4 | 1 | Version | Must be `1` |
| 5 | 1 | Flags | Reserved for scenario metadata |
| 6 | 2 | Reserved | Encoded as zero |
| 8 | 4 | Sensor ID | Per-sensor sequence key |
| 12 | 4 | Sequence | Missing, duplicate, and ordering checks |
| 16 | 8 | Monotonic timestamp (ms) | Freshness policy |
| 24 | 8 | Center frequency (Hz) | Configurable allowed range |
| 32 | 4 | Bandwidth (Hz) | Positive and policy bounded |
| 36 | 2 | RSSI x10 (dBm) | Nominal/degraded/critical thresholds |
| 38 | 2 | SNR x10 (dB) | Nominal/degraded/critical thresholds |
| 40 | 2 | EVM x1000 | Range 0.000-1.000 and quality thresholds |
| 42 | 2 | Reserved | Encoded as zero |
| 44 | 4 | CRC-32 | IEEE CRC over bytes 0-43 |

Decoder validation order is length, magic, version, CRC, then metric contract. Invalid frames never reach the link-quality analyzer.

## Compatibility policy

Version 1 is deliberately strict: unsupported versions and unexpected lengths are rejected. A future version should introduce a separate decoder and explicit compatibility tests instead of silently changing this layout.
