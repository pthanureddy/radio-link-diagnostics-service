# Architecture and decisions

## Components

- `RadioLinkFrame` is the domain representation of a telemetry frame.
- `encode_frame` and `decode_frame` isolate wire-format handling from business rules.
- `UdpReceiver` and `UdpSender` own their sockets through RAII and hide Windows/POSIX differences.
- `LinkAnalyzer` applies thresholds and keeps independent sequence state for each sensor.
- `radio_link_monitor` is a small process boundary that emits machine-readable diagnostics.
- `radio_link_sender` provides deterministic local and CI scenarios without RF hardware.

## Key decisions

The service uses UDP because telemetry commonly favors low overhead and because packet loss and reordering are useful conditions to detect. Reliability is observed rather than hidden: sequence numbers make gaps, duplicates, and out-of-order delivery explicit.

The wire format is fixed-size and allocation-light. Network byte order makes captured frames portable across CPU architectures. CRC-32 catches accidental corruption before metrics enter the stateful analyzer.

Monotonic timestamps avoid wall-clock corrections during one host session. They are appropriate for the local synthetic sender/monitor demonstration; distributed production systems would need a defined clock-synchronization and replay policy.

## Operational behavior

Each accepted frame becomes one JSON line for collection by `journald` or another log pipeline. The monitor returns a non-zero exit code on configuration, socket, or timeout errors so a service manager can restart or alert.
