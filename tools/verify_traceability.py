#!/usr/bin/env python3
"""Validate formal requirement and named-test traceability using only stdlib."""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path


REQUIREMENT_ID = re.compile(r"RLDS-SYS-\d{3}")
REQUIREMENT_ROW = re.compile(r"^\|\s*`?(RLDS-SYS-\d{3})`?\s*\|", re.MULTILINE)
TEST_REGISTRATION = re.compile(r'\{\s*"([a-z][a-z0-9_]*)"\s*,\s*\[\s*\]')
ALLOWED_METHODS = {
    "automated_unit_test",
    "automated_component_test",
    "automated_socket_integration_test",
    "automated_loopback_integration_test",
}


class TraceabilityError(Exception):
    """Raised when traceability artifacts do not agree."""


def duplicates(values: list[str]) -> list[str]:
    seen: set[str] = set()
    repeated: set[str] = set()
    for value in values:
        if value in seen:
            repeated.add(value)
        seen.add(value)
    return sorted(repeated)


def require_equal(label: str, actual: set[str], expected: set[str]) -> None:
    missing = sorted(expected - actual)
    unexpected = sorted(actual - expected)
    if missing or unexpected:
        details = []
        if missing:
            details.append(f"missing {missing}")
        if unexpected:
            details.append(f"unexpected {unexpected}")
        raise TraceabilityError(f"{label}: " + "; ".join(details))


def validate(root: Path) -> tuple[int, int]:
    requirements_path = root / "docs" / "system-requirements.md"
    traceability_path = root / "docs" / "requirements-traceability.md"
    plan_path = root / "docs" / "verification-plan.md"
    tests_path = root / "tests" / "radio_link_tests.cpp"
    manifest_path = root / "verification" / "traceability.json"

    requirements_text = requirements_path.read_text(encoding="utf-8")
    traceability_text = traceability_path.read_text(encoding="utf-8")
    plan_text = plan_path.read_text(encoding="utf-8")
    tests_text = tests_path.read_text(encoding="utf-8")
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))

    if manifest.get("schema_version") != 1:
        raise TraceabilityError("manifest schema_version must be 1")
    entries = manifest.get("requirements")
    if not isinstance(entries, list) or not entries:
        raise TraceabilityError("manifest requirements must be a non-empty list")

    document_ids = REQUIREMENT_ROW.findall(requirements_text)
    matrix_ids = REQUIREMENT_ROW.findall(traceability_text)
    manifest_ids: list[str] = []
    mapped_tests: list[str] = []
    for entry in entries:
        if not isinstance(entry, dict):
            raise TraceabilityError("each manifest requirement must be an object")
        requirement_id = entry.get("id")
        if not isinstance(requirement_id, str) or REQUIREMENT_ID.fullmatch(requirement_id) is None:
            raise TraceabilityError(f"invalid requirement id: {requirement_id!r}")
        method = entry.get("method")
        if method not in ALLOWED_METHODS:
            raise TraceabilityError(f"{requirement_id} has unsupported method: {method!r}")
        test_cases = entry.get("test_cases")
        if not isinstance(test_cases, list) or not test_cases:
            raise TraceabilityError(f"{requirement_id} must map to at least one test")
        if not all(isinstance(case, str) and case for case in test_cases):
            raise TraceabilityError(f"{requirement_id} has an invalid test case name")
        manifest_ids.append(requirement_id)
        mapped_tests.extend(test_cases)

    for label, values in (
        ("formal requirements", document_ids),
        ("traceability matrix requirements", matrix_ids),
        ("manifest requirements", manifest_ids),
    ):
        repeated = duplicates(values)
        if repeated:
            raise TraceabilityError(f"{label} contain duplicates: {repeated}")

    if manifest_ids != sorted(manifest_ids):
        raise TraceabilityError("manifest requirements must be ordered by id")

    expected_ids = set(document_ids)
    require_equal("traceability matrix requirement ids", set(matrix_ids), expected_ids)
    require_equal("manifest requirement ids", set(manifest_ids), expected_ids)

    registered_tests = TEST_REGISTRATION.findall(tests_text)
    repeated_tests = duplicates(registered_tests)
    if repeated_tests:
        raise TraceabilityError(f"C++ test registry contains duplicates: {repeated_tests}")
    require_equal("mapped test cases", set(mapped_tests), set(registered_tests))

    for requirement_id in expected_ids:
        if f"`{requirement_id}`" not in traceability_text:
            raise TraceabilityError(f"traceability matrix omits `{requirement_id}`")
    for test_case in registered_tests:
        if f"`{test_case}`" not in traceability_text:
            raise TraceabilityError(f"traceability matrix omits `{test_case}`")

    required_plan_references = {
        "system-requirements.md",
        "requirements-traceability.md",
        "verification/traceability.json",
        "verify_traceability.py",
    }
    missing_plan_references = sorted(
        reference for reference in required_plan_references if reference not in plan_text
    )
    if missing_plan_references:
        raise TraceabilityError(
            f"verification plan is missing controlled-artifact references: {missing_plan_references}"
        )

    return len(expected_ids), len(registered_tests)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--root",
        type=Path,
        default=Path(__file__).resolve().parents[1],
        help="repository root (defaults to the script's parent repository)",
    )
    args = parser.parse_args()

    try:
        requirement_count, test_count = validate(args.root.resolve())
    except (OSError, json.JSONDecodeError, TraceabilityError) as error:
        print(f"TRACEABILITY FAIL: {error}", file=sys.stderr)
        return 1

    print(
        "TRACEABILITY PASS: "
        f"{requirement_count} requirements, {test_count} named tests, "
        f"{test_count}/{test_count} tests mapped"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
