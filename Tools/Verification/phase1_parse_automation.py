#!/usr/bin/env python3
"""Fail-closed parser for Unreal Automation Framework logs."""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path


FOUND_RE = re.compile(r"Found\s+(\d+)\s+automation tests based on '([^']+)'")
COMPLETED_RE = re.compile(
    r"Test Completed\. Result=\{([^}]+)\}.*Path=\{([^}]+)\}"
)
MARKER_RE = re.compile(r"\*{4} TEST COMPLETE\. EXIT CODE: (\d+) \*{4}")


def parse_suite(spec: str) -> dict:
    try:
        prefix, expected_text, raw_path = spec.split("|", 2)
        expected = int(expected_text)
    except (ValueError, TypeError) as exc:
        raise ValueError(
            f"Invalid --suite value {spec!r}; expected PREFIX|COUNT|RAW_LOG"
        ) from exc

    path = Path(raw_path)
    failures: list[str] = []
    if not path.is_file():
        return {
            "prefix": prefix,
            "expected_count": expected,
            "raw_log": str(path),
            "pass": False,
            "failures": ["Raw Unreal automation log is missing."],
        }

    text = path.read_text(encoding="utf-8", errors="replace")
    found_matches = [(int(n), p) for n, p in FOUND_RE.findall(text) if p == prefix]
    completions = [
        {"result": result, "path": test_path}
        for result, test_path in COMPLETED_RE.findall(text)
        if test_path.startswith(prefix + ".")
    ]
    markers = [int(code) for code in MARKER_RE.findall(text)]

    if len(found_matches) != 1:
        failures.append(
            f"Expected one discovery marker for {prefix}; found {len(found_matches)}."
        )
        discovered = None
    else:
        discovered = found_matches[0][0]
        if discovered != expected:
            failures.append(
                f"Discovered {discovered} tests for {prefix}; expected {expected}."
            )

    completed_paths = [entry["path"] for entry in completions]
    if len(completions) != expected:
        failures.append(
            f"Completed {len(completions)} tests for {prefix}; expected {expected}."
        )
    if len(set(completed_paths)) != len(completed_paths):
        failures.append("Duplicate completed test paths were reported.")

    failed_tests = [
        entry for entry in completions if entry["result"].lower() != "success"
    ]
    if failed_tests:
        failures.append(f"Non-success test results: {failed_tests}")

    if len(markers) != 1 or markers[0] != 0:
        failures.append(
            f"Expected one explicit TEST COMPLETE EXIT CODE 0 marker; found {markers}."
        )

    return {
        "prefix": prefix,
        "expected_count": expected,
        "discovered_count": discovered,
        "completed_count": len(completions),
        "completed_tests": completions,
        "failed_count": len(failed_tests),
        "completion_markers": markers,
        "raw_log": str(path.resolve()),
        "pass": not failures,
        "failures": failures,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--suite",
        action="append",
        required=True,
        help="PREFIX|EXPECTED_COUNT|RAW_LOG",
    )
    parser.add_argument("--output", required=True)
    args = parser.parse_args()

    suites = []
    for spec in args.suite:
        try:
            suites.append(parse_suite(spec))
        except ValueError as exc:
            parser.error(str(exc))

    summary = {
        "schema_version": 1,
        "overall_pass": all(suite["pass"] for suite in suites),
        "expected_total": sum(suite["expected_count"] for suite in suites),
        "completed_total": sum(suite.get("completed_count", 0) for suite in suites),
        "failed_total": sum(suite.get("failed_count", 0) for suite in suites),
        "suites": suites,
    }
    output = Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(summary, indent=2))
    return 0 if summary["overall_pass"] else 2


if __name__ == "__main__":
    sys.exit(main())
