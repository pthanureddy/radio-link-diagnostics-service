# Java acceptance tests

## Purpose and boundary

The framework checks the externally observable contract of the compiled C++
sender and monitor. It adds process and UDP integration evidence to the native
component tests. All input is synthetic. These tests do not validate an RF
channel, a physical Radio Unit, 5G/3GPP conformance, production reliability, or a
real-time deadline.

## Components and technology choices

- `MonitorSession` owns one monitor process, one UDP socket, and one reader
  executor. It binds the monitor to IPv4 loopback on port zero and waits for the
  actual bound port on stderr. No port is selected and released before binding.
- The C++ sender supplies a fresh frame to a temporary Java capture socket.
  Java uses `ByteBuffer` in network byte order to change protocol fields and
  `java.util.zip.CRC32` to recompute checksums where appropriate. Corruption
  tests deliberately leave the checksum wrong.
- The monitor receives one datagram at a time; the test parses its next JSON
  record with Jackson and asserts state, reason codes, counters, and exit code.
- JUnit 5 provides named and parameterized cases. Maven pins the test
  dependencies and plugins and produces standard Surefire XML reports. There
  is no application JVM dependency, Java server, database, container, or JNI.

## Clock and timing

Java `System.nanoTime()` and C++ `steady_clock` need not share an epoch. The
framework therefore captures timestamps from the native sender instead of
inventing a Java-to-native clock conversion. Each packet is created immediately
before exchange. A nominal packet must reach the monitor within its existing
2000 ms freshness policy; a heavily stalled host can correctly fail this check.
Stale/future tests set timestamp fields explicitly. Readiness and reads have
five-second deadlines, command processes have five-second exit limits, and each
JUnit invocation has a twenty-second limit. These limits are test timeouts,
not performance measurements. Tests run sequentially by default.

## Requirement mapping

| JUnit method | Invocations | Native requirements and observed behavior |
|---|---:|---|
| `classifiesNativeSenderScenarios` | 3 | RLDS-SYS-001/005/012/013: nominal, degraded, critical; JSON identity and metrics |
| `rejectsMalformedFrameThenRecovers` | 7 | RLDS-SYS-002/003/004/013: short/long frame, magic, version, CRC, bandwidth, EVM; decoder rejection followed by nominal recovery |
| `tracksDuplicateGapOrderingAndHighWaterMark` | 1 | RLDS-SYS-009/010: duplicate, two missing frames, older sequence, recovery without moving highest sequence backwards |
| `isolatesSequenceStateBySensor` | 1 | RLDS-SYS-011: two interleaved sensor IDs |
| `rejectsPolicyViolations` | 4 | RLDS-SYS-006/007/008: frequency, bandwidth, stale and future timestamp |
| `rejectsInvalidNumericArguments` | 20 | CLI contract: both programs reject negative, overflow, partial, empty and otherwise malformed integers |
| `rejectsInvalidBindAddressBeforeReadiness` | 1 | RLDS-SYS-015 and CLI readiness: invalid IPv4 cannot announce a successful bind |

The suite contains 37 invocations. The existing native traceability manifest
continues to cover its 15 requirements and 26 named C++ tests; this additional
mapping does not relabel native tests or claim independent requirements.

## Reproduce and diagnose

From a fresh checkout, install a C++20 toolchain, CMake 3.20+, Python 3.8+,
JDK 17+, and Maven 3.9+. From the repository root:

```bash
cmake -S . -B build -DRADIO_WARNINGS_AS_ERRORS=ON
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
mvn -B -ntp -f java-acceptance/pom.xml verify
```

For Visual Studio builds, pass
`-Dradio.binary.dir=<absolute repository path>/build/Release` to Maven. Use
`-Dtest=RadioLinkAcceptanceTest#rejectsMalformedFrameThenRecovers` to isolate a
method. A missing binary is a setup error and fails the suite; tests are not
silently skipped. Read `target/surefire-reports/` inside `java-acceptance/` for
assertion details. The Java CI matrix uploads those reports even after failure.

## Trade-offs and remaining limits

Loopback UDP does not measure a lossy external network. Every exchange waits for
one diagnostic before sending the next packet; throughput is not measured. The
sender fixture shares the C++ encoder, while Java mutates fields and calculates
CRC independently; this is not a completely independent encoder implementation.
Policy-rejected frames currently participate in sequence tracking, whereas
decoder-rejected frames do not. Sequence wrap-around, unbounded sensor-map
growth, zero-length datagrams, large-datagram truncation, and authenticated
transport remain outside this suite. No claim of hardware or production
acceptance follows from passing it.
