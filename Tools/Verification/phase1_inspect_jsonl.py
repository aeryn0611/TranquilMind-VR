#!/usr/bin/env python3
"""Strict fail-closed validator for a TranquilMind Phase 1 Research JSONL."""

from __future__ import annotations

import argparse
import json
import math
import os
import sys
from collections import Counter
from pathlib import Path

DEFAULT_SEED = 20260708
DEFAULT_HASH = "4922870221080512783"
DEFAULT_SEQUENCE = ".....N......N.N...N.N......N......N...N..N.N......"
DEFAULT_SEGMENT_LENGTHS = [17, 17, 16]
DEFAULT_SEGMENT_NOGO = [3, 3, 4]
VISUAL_TARGET_S = 0.400
VISUAL_HARD_TOL_S = 0.050
RESPONSE_TARGET_S = 1.200
RESPONSE_WINDOW_TOL_S = 0.010
ACCEPTED_DEADLINE_TOL_S = 0.005
ORDERING_TOL_S = 0.002
ITI_MIN_MS = 840.0
ITI_MAX_MS = 1560.0
ITI_TOL_MS = 75.0
OUTCOMES = ("Hit", "Omission", "CorrectRejection", "Commission", "Void")
GO_OUTCOMES = {"Hit", "Omission", "Void"}
NOGO_OUTCOMES = {"Commission", "CorrectRejection", "Void"}
STRUCTURAL_TYPES = {"session_header", "trial", "block_summary", "session_footer"}
PII_KEYS = {
    "name", "email", "phone", "address", "participantname", "dateofbirth",
    "dob", "firstname", "lastname", "ssn",
}


class Report:
    def __init__(self, path: str):
        self.failures: list[str] = []
        self.warnings: list[str] = []
        self.info: dict = {
            "file": str(Path(path).resolve()),
            "human_checks": {
                "fixed_central_location": "NOT VERIFIED",
                "no_depth_travel": "NOT VERIFIED",
                "approximately_400ms_by_headset_observation": "NOT VERIFIED",
                "post_offset_response_by_headset_observation": "NOT VERIFIED",
                "no_visible_double_scoring": "NOT VERIFIED",
                "no_crash_by_user_observation": "NOT VERIFIED",
            },
        }

    def fail(self, message: str) -> None:
        self.failures.append(message)

    def warn(self, message: str) -> None:
        self.warnings.append(message)

    @property
    def passed(self) -> bool:
        return not self.failures


def finite_number(value) -> bool:
    return isinstance(value, (int, float)) and not isinstance(value, bool) and math.isfinite(value)


def load_records(path: str, rep: Report):
    records = []
    try:
        with open(path, "r", encoding="utf-8", errors="strict") as handle:
            for lineno, raw in enumerate(handle, 1):
                if not raw.strip():
                    continue
                try:
                    obj = json.loads(raw)
                except json.JSONDecodeError as exc:
                    rep.fail(f"Line {lineno}: invalid JSON ({exc}).")
                    continue
                if not isinstance(obj, dict):
                    rep.fail(f"Line {lineno}: record is not a JSON object.")
                    continue
                records.append((lineno, obj))
    except UnicodeDecodeError as exc:
        rep.fail(f"File is not valid UTF-8: {exc}.")
    return records


def check_unique(values, label: str, rep: Report):
    unique = sorted({str(value) for value in values})
    rep.info[label] = unique
    if len(unique) != 1:
        rep.fail(f"Expected one consistent {label}; found {unique}.")
    return unique[0] if len(unique) == 1 else None


def max_runs(sequence: str):
    max_go = max_nogo = run_go = run_nogo = 0
    for symbol in sequence:
        if symbol == ".":
            run_go += 1
            run_nogo = 0
        else:
            run_nogo += 1
            run_go = 0
        max_go = max(max_go, run_go)
        max_nogo = max(max_nogo, run_nogo)
    return max_go, max_nogo


def inspect(path: str, args) -> Report:
    rep = Report(path)
    if not os.path.isfile(path):
        rep.fail("Input file does not exist.")
        return rep
    if os.path.getsize(path) == 0:
        rep.fail("Input file is empty.")
        return rep

    records = load_records(path, rep)
    rep.info["total_nonempty_records"] = len(records)
    types = Counter(obj.get("type") for _, obj in records)
    rep.info["record_types"] = dict(types)
    unexpected = sorted(str(value) for value in set(types) - STRUCTURAL_TYPES)
    if unexpected:
        rep.fail(f"Unexpected record type(s): {unexpected}.")
    for expected_type in ("session_header", "block_summary", "session_footer"):
        if types[expected_type] != 1:
            rep.fail(f"Expected exactly one {expected_type}; found {types[expected_type]}.")

    headers = [obj for _, obj in records if obj.get("type") == "session_header"]
    trials = [obj for _, obj in records if obj.get("type") == "trial"]
    summaries = [obj for _, obj in records if obj.get("type") == "block_summary"]
    footers = [obj for _, obj in records if obj.get("type") == "session_footer"]
    header = headers[0] if len(headers) == 1 else {}
    summary = summaries[0] if len(summaries) == 1 else {}
    footer = footers[0] if len(footers) == 1 else {}
    rep.info["trial_count"] = len(trials)

    pii_hits = []
    for lineno, obj in records:
        for key in obj:
            if key.lower() in PII_KEYS:
                pii_hits.append(f"line {lineno}: {key}")
    rep.info["pii_suspect_hits"] = pii_hits
    if pii_hits:
        rep.fail(f"Possible personal-identifier keys found: {pii_hits}.")

    expected_trial_count = 50 if args.expected_termination == "normal" else len(trials)
    if args.expected_termination == "normal" and len(trials) != 50:
        rep.fail(f"Normal default block requires exactly 50 trial records; found {len(trials)}.")
    if args.expected_termination == "interrupted" and len(trials) > 50:
        rep.fail(f"Interrupted block cannot contain more than 50 trials; found {len(trials)}.")

    indices = [trial.get("trial") for trial in trials]
    expected_indices = list(range(expected_trial_count))
    rep.info["trial_indices"] = indices
    if indices != expected_indices:
        rep.fail(f"Trial indices must be exactly {expected_indices}; found {indices}.")
    duplicate_indices = sorted(index for index, count in Counter(indices).items() if count > 1)
    if duplicate_indices:
        rep.fail(f"Duplicate trial indices: {duplicate_indices}.")

    all_structural = headers + trials + summaries + footers
    session = check_unique([obj.get("sessionId") for obj in all_structural], "session_ids", rep)
    if not session or session == "None":
        rep.fail("Every structural record must contain a non-empty sessionId.")
    if args.expected_session and session != args.expected_session:
        rep.fail(f"Session ID {session!r} does not match expected current session {args.expected_session!r}.")

    mode = check_unique([trial.get("mode") for trial in trials], "modes", rep) if trials else None
    if mode != "Research":
        rep.fail(f"Mode must be Research; found {mode!r}.")
    seed_text = check_unique([trial.get("seed") for trial in trials], "seeds", rep) if trials else None
    try:
        seed = int(seed_text) if seed_text is not None else None
    except ValueError:
        seed = None
    if seed != args.expected_seed:
        rep.fail(f"Seed must be {args.expected_seed}; found {seed_text!r}.")
    sequence_hash = check_unique([trial.get("seqHash") for trial in trials], "sequence_hashes", rep) if trials else None

    stimuli = [trial.get("stimulus") for trial in trials]
    if any(stimulus not in {"GO", "NOGO"} for stimulus in stimuli):
        rep.fail("Every trial stimulus must be GO or NOGO.")
    sequence = "".join("N" if stimulus == "NOGO" else "." for stimulus in stimuli)
    rep.info["sequence"] = sequence
    stimulus_counts = Counter(stimuli)
    rep.info["stimulus_counts"] = dict(stimulus_counts)
    if args.expected_termination == "normal":
        if stimulus_counts != Counter({"GO": 40, "NOGO": 10}):
            rep.fail(f"Expected GO/NOGO=40/10; found {dict(stimulus_counts)}.")
        if sequence and sequence[0] != ".":
            rep.fail("The first trial must be GO.")
        max_go, max_nogo = max_runs(sequence)
        rep.info["max_runs"] = {"GO": max_go, "NOGO": max_nogo}
        if max_go > 6:
            rep.fail(f"Maximum GO run is {max_go}, exceeding 6.")
        if max_nogo > 1:
            rep.fail(f"Maximum NOGO run is {max_nogo}, exceeding 1.")
        starts = [0, 17, 34]
        segment_nogo = [
            sequence[start:start + length].count("N")
            for start, length in zip(starts, DEFAULT_SEGMENT_LENGTHS)
        ]
        rep.info["segment_lengths"] = DEFAULT_SEGMENT_LENGTHS
        rep.info["segment_nogo_counts"] = segment_nogo
        if segment_nogo != DEFAULT_SEGMENT_NOGO:
            rep.fail(f"Segment NOGO counts must be {DEFAULT_SEGMENT_NOGO}; found {segment_nogo}.")
        if args.expected_seed == DEFAULT_SEED and not args.disable_default_contract:
            if sequence != DEFAULT_SEQUENCE:
                rep.fail("Default-seed sequence does not match the compiled replay contract.")
            if sequence_hash != DEFAULT_HASH:
                rep.fail(f"Default sequence hash must be {DEFAULT_HASH}; found {sequence_hash!r}.")

    outcome_counts = Counter(trial.get("outcome") for trial in trials)
    rep.info["outcome_counts"] = {name: outcome_counts.get(name, 0) for name in OUTCOMES}
    for trial in trials:
        index = trial.get("trial")
        stimulus = trial.get("stimulus")
        outcome = trial.get("outcome")
        allowed = GO_OUTCOMES if stimulus == "GO" else NOGO_OUTCOMES
        if outcome not in allowed:
            rep.fail(f"Trial {index}: outcome {outcome!r} is invalid for {stimulus}.")
        if outcome == "Void" and not args.allow_void:
            rep.fail(f"Trial {index}: Void is not allowed for this standard verification run.")
        if outcome == "Void":
            if trial.get("valid") is not False or not trial.get("voidReason"):
                rep.fail(f"Trial {index}: Void requires valid=false and a non-empty voidReason.")
        elif trial.get("valid") is not True:
            rep.fail(f"Trial {index}: non-Void outcome requires valid=true.")

    timing_fields = (
        "scheduledStimulusOnsetSec", "realizedStimulusOnsetSec",
        "scheduledStimulusOffsetSec", "realizedStimulusOffsetSec",
        "responseWindowDeadlineSec", "responseTimestampSec",
        "behavioralResolutionTimestampSec", "trialCompleteTimestampSec",
        "rtMs", "responseWindowMs", "scheduledItiMs", "realizedItiMs",
    )
    visual_durations = []
    post_offset_count = 0
    for trial in trials:
        index = trial.get("trial")
        for field in timing_fields:
            if not finite_number(trial.get(field)):
                rep.fail(f"Trial {index}: {field} must be finite.")
        if any(not finite_number(trial.get(field)) for field in timing_fields):
            continue
        onset = float(trial["realizedStimulusOnsetSec"])
        offset = float(trial["realizedStimulusOffsetSec"])
        deadline = float(trial["responseWindowDeadlineSec"])
        response = float(trial["responseTimestampSec"])
        resolution = float(trial["behavioralResolutionTimestampSec"])
        complete = float(trial["trialCompleteTimestampSec"])
        rt_ms = float(trial["rtMs"])
        visual = offset - onset
        visual_durations.append(visual)
        if abs(visual - VISUAL_TARGET_S) > VISUAL_HARD_TOL_S:
            rep.fail(
                f"Trial {index}: visual exposure {visual:.6f}s is outside "
                f"{VISUAL_TARGET_S:.3f}+/-{VISUAL_HARD_TOL_S:.3f}s."
            )
        if abs((deadline - onset) - RESPONSE_TARGET_S) > RESPONSE_WINDOW_TOL_S:
            rep.fail(f"Trial {index}: response deadline duration is inconsistent with 1200 ms.")
        if abs(float(trial["responseWindowMs"]) / 1000.0 - (deadline - onset)) > RESPONSE_WINDOW_TOL_S:
            rep.fail(f"Trial {index}: responseWindowMs disagrees with deadline timestamp.")
        if trial.get("visualTerminatedByResponse") is not False:
            rep.fail(f"Trial {index}: visualTerminatedByResponse must be false.")

        had_response = trial.get("hadResponse")
        if not isinstance(had_response, bool):
            rep.fail(f"Trial {index}: hadResponse must be boolean.")
        elif had_response:
            if response < onset - ORDERING_TOL_S:
                rep.fail(f"Trial {index}: response precedes realized onset.")
            if response > deadline + ACCEPTED_DEADLINE_TOL_S:
                rep.fail(f"Trial {index}: accepted response occurs after the deadline.")
            if rt_ms < 0:
                rep.fail(f"Trial {index}: accepted response has negative RT.")
            if abs(rt_ms - (response - onset) * 1000.0) > 5.0:
                rep.fail(f"Trial {index}: rtMs disagrees with response-onset delta.")
            actual_after_offset = response >= offset - ORDERING_TOL_S
            if trial.get("responseAcceptedAfterVisualOffset") is not actual_after_offset:
                rep.fail(f"Trial {index}: responseAcceptedAfterVisualOffset is inconsistent.")
            if actual_after_offset:
                post_offset_count += 1
            if abs(resolution - response) > ORDERING_TOL_S and trial.get("outcome") != "Void":
                rep.fail(f"Trial {index}: behavioral resolution does not equal accepted response time.")
        else:
            if response >= 0 or rt_ms >= 0:
                rep.fail(f"Trial {index}: no-response trial must use negative response timestamp and RT.")
            if trial.get("responseAcceptedAfterVisualOffset") is not False:
                rep.fail(f"Trial {index}: no-response trial cannot be marked post-offset accepted.")
            if trial.get("outcome") != "Void" and abs(resolution - deadline) > ORDERING_TOL_S:
                rep.fail(f"Trial {index}: timeout resolution must occur at the response deadline.")

        if complete + ORDERING_TOL_S < max(offset, resolution):
            rep.fail(f"Trial {index}: trial completed before visual and behavioral resolution.")

        scheduled_iti = float(trial["scheduledItiMs"])
        realized_iti = float(trial["realizedItiMs"])
        if index == len(trials) - 1 and args.expected_termination == "normal":
            if abs(scheduled_iti) > 1.0 or abs(realized_iti) > 1.0:
                rep.fail("Final trial ITI must be zero.")
        elif args.expected_termination == "normal":
            for label, value in (("scheduledItiMs", scheduled_iti), ("realizedItiMs", realized_iti)):
                if value < ITI_MIN_MS - ITI_TOL_MS or value > ITI_MAX_MS + ITI_TOL_MS:
                    rep.fail(f"Trial {index}: {label}={value} is outside configured bounds plus tolerance.")

    rep.info["post_offset_accepted_count"] = post_offset_count
    if visual_durations:
        rep.info["visual_exposure_s"] = {
            "min": min(visual_durations),
            "max": max(visual_durations),
            "mean": sum(visual_durations) / len(visual_durations),
            "target": VISUAL_TARGET_S,
            "hard_tolerance": VISUAL_HARD_TOL_S,
        }

    if summary:
        if summary.get("seed") != args.expected_seed:
            rep.fail("Block summary seed does not match expected seed.")
        if str(summary.get("seqHash")) != str(sequence_hash):
            rep.fail("Block summary sequence hash does not match trial records.")
        if summary.get("goCount") != 40 or summary.get("noGoCount") != 10:
            rep.fail("Block summary GO/NOGO schedule counts must be 40/10.")
        summary_outcomes = {
            "Hit": summary.get("hits"), "Omission": summary.get("omissions"),
            "CorrectRejection": summary.get("correctRejections"),
            "Commission": summary.get("commissions"), "Void": summary.get("voids"),
        }
        for outcome, value in summary_outcomes.items():
            if value != outcome_counts.get(outcome, 0):
                rep.fail(f"Block summary {outcome} count {value} disagrees with trials.")
        if summary.get("completion") != args.expected_termination:
            rep.fail("Block summary completion does not match expected termination.")
        reason = summary.get("terminationReason")
        if args.expected_termination == "normal" and reason not in ("", None):
            rep.fail("Normal block summary must have an empty terminationReason.")
        if args.expected_termination == "interrupted" and not reason:
            rep.fail("Interrupted block summary must have a terminationReason.")

    if footer:
        if footer.get("healthy") is not True:
            rep.fail("Session footer healthy must be true for a successful verification run.")
        if footer.get("writeFailures") != 0:
            rep.fail("Session footer writeFailures must equal zero.")
        expected_records_written = len(trials) + len(summaries)
        if footer.get("recordsWritten") != expected_records_written:
            rep.fail(
                f"Footer recordsWritten={footer.get('recordsWritten')} does not match "
                f"logger semantics trial+summary={expected_records_written}."
            )
        if footer.get("completion") != args.expected_termination:
            rep.fail("Session footer completion does not match expected termination.")
        reason = footer.get("terminationReason")
        if args.expected_termination == "normal" and reason not in ("", None):
            rep.fail("Normal session footer must have an empty terminationReason.")
        if args.expected_termination == "interrupted" and not reason:
            rep.fail("Interrupted session footer must have a terminationReason.")

    coverage = {outcome: outcome_counts.get(outcome, 0) > 0 for outcome in OUTCOMES if outcome != "Void"}
    rep.info["behavioral_outcome_coverage"] = coverage
    missing_coverage = [outcome for outcome, observed in coverage.items() if not observed]
    if missing_coverage:
        rep.warn(
            "Behavioral outcome coverage not observed for "
            + ", ".join(missing_coverage)
            + "; this does not invalidate protocol integrity."
        )
    rep.info["protocol_integrity"] = "PASS" if rep.passed else "FAIL"
    return rep


def write_outputs(rep: Report, json_path: str, markdown_path: str):
    result = {
        "protocol_integrity_pass": rep.passed,
        "info": rep.info,
        "warnings": rep.warnings,
        "failures": rep.failures,
    }
    Path(json_path).write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    lines = [
        f"### JSONL inspection — `{Path(rep.info['file']).name}`",
        "",
        f"- Protocol integrity: **{'PASS' if rep.passed else 'FAIL'}**",
        f"- Trials: {rep.info.get('trial_count')}",
        f"- Stimulus counts: `{rep.info.get('stimulus_counts')}`",
        f"- Outcome counts: `{rep.info.get('outcome_counts')}`",
        f"- Session IDs: `{rep.info.get('session_ids')}`",
        f"- Seeds: `{rep.info.get('seeds')}`",
        f"- Sequence hashes: `{rep.info.get('sequence_hashes')}`",
        f"- Human headset checks: **NOT VERIFIED by Inspector**",
    ]
    if rep.warnings:
        lines += ["", "**Warnings:**"] + [f"- {item}" for item in rep.warnings]
    if rep.failures:
        lines += ["", "**Blocking failures:**"] + [f"- {item}" for item in rep.failures]
    Path(markdown_path).write_text("\n".join(lines) + "\n", encoding="utf-8")


def main(argv=None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("jsonl")
    parser.add_argument("--expected-session")
    parser.add_argument("--expected-seed", type=int, default=DEFAULT_SEED)
    parser.add_argument("--disable-default-contract", action="store_true")
    parser.add_argument("--allow-void", action="store_true")
    parser.add_argument(
        "--expected-termination", choices=("normal", "interrupted"), default="normal"
    )
    parser.add_argument("--json-out")
    parser.add_argument("--markdown-out")
    args = parser.parse_args(argv)

    report = inspect(args.jsonl, args)
    json_out = args.json_out or args.jsonl + ".summary.json"
    markdown_out = args.markdown_out or args.jsonl + ".summary.md"
    try:
        write_outputs(report, json_out, markdown_out)
    except OSError as exc:
        print(f"Could not write required inspection outputs: {exc}", file=sys.stderr)
        return 3

    print(f"Protocol integrity: {'PASS' if report.passed else 'FAIL'}")
    print(f"Trials: {report.info.get('trial_count')}")
    print(f"Stimulus counts: {report.info.get('stimulus_counts')}")
    print(f"Outcome counts: {report.info.get('outcome_counts')}")
    for warning in report.warnings:
        print(f"WARNING: {warning}")
    for failure in report.failures:
        print(f"FAIL: {failure}")
    print(f"JSON summary: {json_out}")
    print(f"Markdown summary: {markdown_out}")
    return 0 if report.passed else 2


if __name__ == "__main__":
    sys.exit(main())
