# Verification plan

## Objective

Demonstrate, with repeatable local and CI evidence, that the synthetic radio-link
diagnostics service satisfies the formal requirements in
[`system-requirements.md`](system-requirements.md) and that every named automated
test remains linked to a requirement.

## Test levels and environment

1. **Unit verification** exercises CRC, codec validation, policy classification,
   sequence supervision, and JSON serialization in process.
2. **Component and loopback integration verification** exercises socket
   construction, receive timeout behavior, and byte-for-byte UDP/IPv4 delivery on
   the local host. The loopback test uses no external service or radio hardware.
3. **Traceability verification** compares the formal requirement IDs, the
   machine-readable matrix, the human-readable matrix, and the C++ test registry.
4. **Portable build verification** runs CMake and CTest with warnings as errors on
   the compiler/operating-system combinations declared in the GitHub Actions
   workflow. A separate Linux job runs AddressSanitizer and
   UndefinedBehaviorSanitizer; another job builds the container image.

## Entry criteria

- CMake can locate a C++20 compiler and Python 3.8 or newer.
- The controlled interface definition in [`protocol.md`](protocol.md) and the
  system requirements are available in the same revision as the implementation.
- Tests use synthetic fixtures and loopback networking only.

## Procedure

```bash
cmake -S . -B build -G Ninja -DRADIO_WARNINGS_AS_ERRORS=ON
cmake --build build
ctest --test-dir build --output-on-failure
./build/radio_link_tests
python3 tools/verify_traceability.py --root .
```

The platform-specific executable suffix and multi-configuration build flag may be
needed on Windows.

## Pass/fail and exit criteria

- All 26 named C++ scenarios pass; any failed assertion makes the runner nonzero.
- The traceability check reports 15 requirements and 26 mapped tests with no
  missing, unknown, duplicated, or malformed identifiers.
- CTest reports both registered tests passing.
- The portable build jobs complete with warnings treated as errors; sanitizer and
  container jobs complete for changes intended for `main`.

The detailed mapping is maintained in
[`requirements-traceability.md`](requirements-traceability.md) and
[`verification/traceability.json`](../verification/traceability.json).

## Limitations and deferred verification

This plan does not verify RF characteristics, modulation, antenna behavior,
hardware timing, external equipment, adverse live-network conditions, load,
soak/endurance behavior, authentication, encryption, or compliance with 3GPP,
ETSI, defence, or safety standards. Production acceptance would require separate
requirements, representative hardware and networks, security review, and an
approved operational test environment.
