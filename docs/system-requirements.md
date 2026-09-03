# System requirements

## Purpose and scope

These requirements define the verifiable behavior of the hardware-independent
radio-link diagnostics reference service. The system under test consists of the
synthetic sender, version-1 binary interface, UDP transport wrapper, decoder,
stateful analyzer, and JSON diagnostic serializer.

The keywords **shall** and **shall not** identify mandatory behavior. All numeric
values refer to the default configuration in the repository. The binary interface
is controlled by [`protocol.md`](protocol.md).

| ID | Verifiable requirement | Acceptance criterion |
|---|---|---|
| RLDS-SYS-001 | The codec shall serialize a version-1 telemetry frame to exactly 48 bytes and recover every domain field on decode. | A nominal frame round-trips without field changes and the encoded size is 48 bytes. |
| RLDS-SYS-002 | The codec shall implement the repository's documented CRC-32 integrity check and shall reject a payload changed after checksum generation. | The standard `123456789` vector produces `0xCBF43926`; a one-byte payload change returns `checksum_mismatch`. |
| RLDS-SYS-003 | The decoder shall reject a datagram with the wrong length, magic, or protocol version before analysis. | Each malformed input returns its corresponding `invalid_size`, `invalid_magic`, or `unsupported_version` result. |
| RLDS-SYS-004 | The codec shall reject zero frequency, zero bandwidth, and EVM greater than 1.000 at both serialization and deserialization boundaries. | All three invalid metric classes raise an encode error and return `invalid_metric` after decode with a valid recomputed checksum. |
| RLDS-SYS-005 | The analyzer shall classify the defined nominal, degraded-RSSI, and critical-SNR fixtures as `nominal`, `degraded`, and `critical`, respectively. | Each fixture produces its specified state using the default link policy. |
| RLDS-SYS-006 | The analyzer shall reject center frequency below 100 MHz or above 6 GHz. | Representative values below and above the inclusive policy band both produce `rejected`. |
| RLDS-SYS-007 | The analyzer shall reject bandwidth above 200 MHz. | A value of 200,000,001 Hz produces `rejected`. |
| RLDS-SYS-008 | The analyzer shall reject telemetry older than 2,000 ms and telemetry timestamped later than its arrival time. | A stale fixture and a future-dated fixture both produce `rejected`. |
| RLDS-SYS-009 | The analyzer shall calculate the number of missing sequence values when a sensor's sequence advances by more than one. | A transition from sequence 4 to 7 reports two missing frames and a degraded state. |
| RLDS-SYS-010 | The analyzer shall identify duplicate and decreasing sequence values without advancing its highest accepted sequence for either condition. | Repeated and lower sequence fixtures set `duplicate` and `out_of_order`, respectively, and produce a degraded state. |
| RLDS-SYS-011 | The analyzer shall maintain sequence state independently by sensor and shall provide an operation that clears all sequence state. | A new sensor starts nominally after another sensor is observed; after reset, sequence 1 is nominal. |
| RLDS-SYS-012 | The serializer shall emit one JSON object containing the frame metrics, resulting state, sequence diagnostics, and reason list. | The nominal fixture's JSON contains every documented diagnostic field and expected value. |
| RLDS-SYS-013 | The UDP wrappers shall preserve an encoded datagram across IPv4 loopback transport. | Bytes received on loopback exactly equal the transmitted frame bytes. |
| RLDS-SYS-014 | The UDP receiver shall report an error when no datagram arrives within the configured receive duration. | An idle receiver with a 10 ms duration raises a timeout error. |
| RLDS-SYS-015 | The UDP sender shall reject an invalid IPv4 literal during construction. | Constructing a sender with `not-an-ip` raises an error. |

## Out-of-scope requirements

No requirement in this document claims behavior for RF hardware, antennas,
cryptographic transport, telecom-standard interoperability, classified data,
production networks, or real operational telemetry. See
[`security.md`](security.md) and [`verification-plan.md`](verification-plan.md) for
remaining risks and verification boundaries.
