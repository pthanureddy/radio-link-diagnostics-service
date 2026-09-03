# Requirements traceability

This matrix links every formal system requirement to one or more named automated
tests. The machine-readable source is
[`verification/traceability.json`](../verification/traceability.json), and
`tools/verify_traceability.py` checks that the requirement document, this matrix,
the manifest, and the C++ test registry remain consistent.

| Requirement | Verification method | Named test cases |
|---|---|---|
| `RLDS-SYS-001` | Automated unit test | `frame_round_trip`, `frame_size_is_fixed` |
| `RLDS-SYS-002` | Automated unit test | `crc32_known_vector`, `reject_corrupted_payload` |
| `RLDS-SYS-003` | Automated unit test | `reject_truncated_frame`, `reject_invalid_magic`, `reject_unsupported_version` |
| `RLDS-SYS-004` | Automated unit test | `reject_invalid_encode_metrics`, `reject_invalid_decoded_metrics` |
| `RLDS-SYS-005` | Automated unit test | `nominal_quality`, `degraded_quality`, `critical_quality` |
| `RLDS-SYS-006` | Automated unit test | `reject_frequency_outside_policy`, `reject_frequency_above_policy` |
| `RLDS-SYS-007` | Automated unit test | `reject_bandwidth_above_policy` |
| `RLDS-SYS-008` | Automated unit test | `reject_stale_frame`, `reject_future_frame` |
| `RLDS-SYS-009` | Automated unit test | `detect_sequence_gap` |
| `RLDS-SYS-010` | Automated unit test | `detect_duplicate`, `detect_out_of_order` |
| `RLDS-SYS-011` | Automated unit test | `track_sensors_independently`, `reset_sequence_state` |
| `RLDS-SYS-012` | Automated unit test | `json_contains_diagnostics` |
| `RLDS-SYS-013` | Automated loopback integration test | `udp_loopback_round_trip` |
| `RLDS-SYS-014` | Automated socket integration test | `udp_timeout` |
| `RLDS-SYS-015` | Automated component test | `reject_invalid_ipv4` |

The C++ runner prints each named result and a final `26/26 passed` summary. CTest
registers the C++ runner and the independent traceability check as two tests.

## Coverage boundaries

The matrix demonstrates requirements-based verification of the synthetic software
path. It does not demonstrate RF performance, interoperability with telecom
standards, security accreditation, live-network behavior, throughput, long-duration
reliability, or hardware integration. Those items are outside this repository's
declared scope.
