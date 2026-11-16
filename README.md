# Radio Link Diagnostics Service

A portable C++20 reference service that receives synthetic radio-link telemetry over UDP/IP, validates a fixed binary protocol, tracks packet sequences, and emits deterministic JSON diagnostics for Linux operations.

The repository is intentionally unclassified and hardware-independent. It demonstrates systems-development techniques; it does not implement signal interception, RF control, encryption, or a production telecom standard.

## What it demonstrates

- Object-oriented C++20 components for UDP transport and stateful link analysis
- A versioned 48-byte network-order frame with CRC-32 integrity validation
- Radio-adjacent metrics: center frequency, bandwidth, RSSI, SNR, and EVM
- Detection of stale, duplicate, out-of-order, missing, malformed, and policy-violating frames
- Cross-platform socket code for Linux and Windows
- Linux packaging through Docker and a hardened `systemd` unit
- CMake, CTest, GCC, Clang, MSVC, AddressSanitizer, UndefinedBehaviorSanitizer, and GitHub Actions
- Testable `RLDS-SYS-*` requirements with an automated requirements-to-test traceability check
- Java 17/JUnit 5 black-box acceptance tests of the compiled sender and monitor

## Architecture

```text
synthetic sender -> UDP/IPv4 -> frame decoder -> policy + sequence analyzer -> JSON diagnostics
                                      |                    |
                                   CRC-32           per-sensor state
```

The sender creates deterministic test telemetry. The monitor binds a UDP port, decodes each datagram, rejects invalid frames, analyzes valid metrics, and writes one JSON object per frame to standard output.

## Build and verify

Requirements: CMake 3.20+, a C++20 compiler, Python 3.8+ for the
traceability check, and Ninja or another supported CMake generator.

```bash
cmake -S . -B build -G Ninja -DRADIO_WARNINGS_AS_ERRORS=ON
cmake --build build
ctest --test-dir build --output-on-failure
./build/radio_link_tests
```

The named C++ test runner covers 26 unit and loopback integration scenarios.
CTest also runs an automated traceability check that fails if a formal
requirement or named test is missing from the verification matrix.

To run that check directly:

```bash
python3 tools/verify_traceability.py --root .
```

## Java acceptance framework

After building the native binaries, use JDK 17+ and Maven 3.9+:

```bash
mvn -B -ntp -f java-acceptance/pom.xml verify
```

The default binary directory is `build/`. For a multi-configuration Windows
build or a custom build directory, pass its absolute path:

```powershell
mvn -B -ntp -f java-acceptance/pom.xml "-Dradio.binary.dir=$PWD/build/Release" verify
```

JUnit launches the real C++ processes and verifies JSON diagnostics after UDP
fault injection. It uses loopback binding, OS-assigned ports, an explicit
readiness event, and bounded process cleanup. There are no startup sleeps or
mocked decoder calls. Maven writes XML and text results under
`java-acceptance/target/surefire-reports/`. CI runs this suite on Linux and Windows
and retains the reports. See [Java acceptance design](docs/java-acceptance.md)
for test mapping, clock handling, failure diagnosis, and limitations.

## Run locally

Terminal 1:

```bash
./build/radio_link_monitor --port 9400 --count 3
```

Terminal 2:

```bash
./build/radio_link_sender --host 127.0.0.1 --port 9400 --count 3 --scenario nominal
```

Available synthetic sender scenarios are `nominal`, `degraded`, and `critical`. Use `--continuous` on the monitor for a long-running service.

The monitor also accepts `--bind IPv4` (default `0.0.0.0`), `--port 0` to request
an OS-assigned port, and `--announce-ready`. The last option writes
`radio_link_monitor: ready port=<actual port>` to stderr only after binding.
Diagnostics remain one flushed JSON object per datagram on stdout. Both CLIs
reject malformed, overflowing, and out-of-range numeric arguments with exit
code 2; sender ports must be 1-65535, monitor ports 0-65535, and counts positive.

Example output:

```json
{"sensor_id":1001,"sequence":1,"center_frequency_hz":3500000000,"bandwidth_hz":20000000,"rssi_dbm":-70,"snr_db":18,"evm":0.07,"state":"nominal","missing_frames":0,"duplicate":false,"out_of_order":false,"reasons":["within_policy"]}
```

## Container and Linux service

```bash
docker build -t radio-link-diagnostics-service .
docker run --read-only --cap-drop=ALL -p 9400:9400/udp radio-link-diagnostics-service
```

For a host installation, copy both binaries to `/usr/local/bin/` and install `deploy/radio-link-monitor.service` under `/etc/systemd/system/`.

## Evidence boundaries

- All telemetry and failure scenarios are synthetic.
- UDP is deliberately unauthenticated and unencrypted; bind to a trusted interface or place the service behind an authenticated transport before production use.
- CRC-32 detects accidental corruption but is not a cryptographic integrity mechanism.
- The project does not connect to radios, antennas, switches, routers, classified systems, or live operational networks.
- The protocol is project-specific and is not presented as 3GPP, ETSI, or defence-system compliance.

See [system-requirements.md](docs/system-requirements.md),
[verification-plan.md](docs/verification-plan.md),
[requirements-traceability.md](docs/requirements-traceability.md),
[protocol.md](docs/protocol.md), [architecture.md](docs/architecture.md), and
[security.md](docs/security.md) for review evidence.

## License

MIT. See [LICENSE](LICENSE).
