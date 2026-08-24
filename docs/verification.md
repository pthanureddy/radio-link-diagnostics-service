# Verification record

Verified on 2026-08-24 against commit `48fbfd2` on `main`.

## Local Windows verification

- Toolchain: CMake 4.2.1, Ninja, and GCC 16.1.0 (w64devkit)
- Configuration: C++20 with `RADIO_WARNINGS_AS_ERRORS=ON`
- Clean build: 11/11 targets compiled and linked
- CTest: 1/1 suite passed
- Named test executable: 22/22 checks passed
- UDP CLI smoke test: three degraded frames sent through real IPv4 loopback and emitted as three valid JSON diagnostic lines

## GitHub Actions verification

Main-branch run: [CI run 32746366818](https://github.com/pthanureddy/radio-link-diagnostics-service/actions/runs/32746366818)

All five independent jobs completed successfully:

- Linux GCC build and CTest
- Linux Clang build and CTest
- Windows MSVC build and CTest
- Linux Clang AddressSanitizer and UndefinedBehaviorSanitizer build and CTest
- Ubuntu multi-stage container build, including the Dockerfile's internal CTest gate

## Scope

This record verifies the source, test suite, cross-platform build, synthetic UDP loopback path, and container recipe. It does not claim RF-hardware, live-network, telecom-standard, security-accreditation, or classified-environment validation.
