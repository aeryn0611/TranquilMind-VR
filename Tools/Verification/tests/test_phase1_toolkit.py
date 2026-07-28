#!/usr/bin/env python3
"""Isolated self-tests for the Phase 1 verification toolkit."""

from __future__ import annotations

import copy
import json
import os
import subprocess
import tempfile
import time
import unittest
from pathlib import Path


TOOLS = Path(__file__).resolve().parents[1]
COMMON = TOOLS / "phase1_verify_common.sh"
INSPECTOR = TOOLS / "phase1_inspect_jsonl.py"
PARSER = TOOLS / "phase1_parse_automation.py"
REPORT_SCRIPT = TOOLS / "phase1_restore_and_report.sh"
SEQUENCE = ".....N......N.N...N.N......N......N...N..N.N......"
SESSION = "Synthetic-Phase1-Session"
HASH = "4922870221080512783"


def shell(script: str, env: dict, check=False):
    return subprocess.run(
        ["bash", "-c", script],
        env=env,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=check,
    )


def make_records():
    records = [{
        "type": "session_header",
        "sessionId": SESSION,
        "startedUtc": "2026-07-20T00:00:00Z",
        "schemaVersion": 2,
    }]
    outcome_counts = {
        "Hit": 0, "Omission": 0, "CorrectRejection": 0, "Commission": 0, "Void": 0
    }
    go_seen = nogo_seen = 0
    for index, symbol in enumerate(SEQUENCE):
        onset = 1000.0 + index * 2.0
        offset = onset + 0.4
        deadline = onset + 1.2
        stimulus = "NOGO" if symbol == "N" else "GO"
        if stimulus == "GO":
            outcome = "Hit" if go_seen % 2 == 0 else "Omission"
            go_seen += 1
        else:
            outcome = "Commission" if nogo_seen % 2 == 0 else "CorrectRejection"
            nogo_seen += 1
        had_response = outcome in {"Hit", "Commission"}
        response = onset + (0.6 if index == 0 else 0.2) if had_response else -1.0
        resolution = response if had_response else deadline
        rt_ms = (response - onset) * 1000.0 if had_response else -1.0
        outcome_counts[outcome] += 1
        records.append({
            "type": "trial",
            "sessionId": SESSION,
            "mode": "Research",
            "block": 0,
            "trial": index,
            "stimulus": stimulus,
            "seed": 20260708,
            "seqHash": HASH,
            "scheduledStimulusOnsetSec": onset,
            "realizedStimulusOnsetSec": onset,
            "scheduledStimulusOffsetSec": offset,
            "realizedStimulusOffsetSec": offset,
            "responseWindowDeadlineSec": deadline,
            "responseTimestampSec": response,
            "behavioralResolutionTimestampSec": resolution,
            "trialCompleteTimestampSec": max(offset, resolution),
            "hadResponse": had_response,
            "rtMs": rt_ms,
            "outcome": outcome,
            "visualTerminatedByResponse": False,
            "responseAcceptedAfterVisualOffset": had_response and response >= offset,
            "responseWindowMs": 1200.0,
            "scheduledItiMs": 0.0 if index == 49 else 1200.0,
            "realizedItiMs": 0.0 if index == 49 else 1200.0,
            "valid": True,
            "voidReason": "",
            "gsr": None,
        })
    records.append({
        "type": "block_summary",
        "sessionId": SESSION,
        "block": 0,
        "goCount": 40,
        "noGoCount": 10,
        "hits": outcome_counts["Hit"],
        "omissions": outcome_counts["Omission"],
        "correctRejections": outcome_counts["CorrectRejection"],
        "commissions": outcome_counts["Commission"],
        "voids": 0,
        "seed": 20260708,
        "seqHash": HASH,
        "completion": "normal",
        "terminationReason": "",
    })
    records.append({
        "type": "session_footer",
        "sessionId": SESSION,
        "recordsWritten": 51,
        "writeFailures": 0,
        "healthy": True,
        "completion": "normal",
        "terminationReason": "",
        "endedUtc": "2026-07-20T00:03:00Z",
    })
    return records


def write_jsonl(path: Path, records):
    path.write_text(
        "".join(json.dumps(record, separators=(",", ":")) + "\n" for record in records),
        encoding="utf-8",
    )


class ToolkitTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        (self.root / "Config").mkdir()
        self.config = self.root / "Config" / "DefaultGame.ini"
        self.original_bytes = (
            b"; keep exact bytes\n"
            b"[/Script/TranquilMind.TMResearchSettings]\n"
            b"OperatingMode=Demo\n"
            b"Unrelated=PreserveMe\n"
        )
        self.config.write_bytes(self.original_bytes)
        self.base = self.root / "Verification" / "Phase1"
        self.env = os.environ.copy()
        self.env.update({
            "PHASE1_PROJECT_ROOT": str(self.root),
            "PHASE1_VERIFICATION_BASE": str(self.base),
        })

    def tearDown(self):
        self.temp.cleanup()

    def common(self, body: str):
        return shell(f"source {COMMON!s}; {body}", self.env)

    def new_run(self):
        result = self.common("init_run_context new")
        self.assertEqual(result.returncode, 0, result.stderr)
        return (self.base / ".current_run").read_text().strip()

    def test_new_run_does_not_overwrite_old_run(self):
        first = self.new_run()
        sentinel = self.base / first / "sentinel"
        sentinel.write_text("preserve", encoding="utf-8")
        time.sleep(1)
        second = self.new_run()
        self.assertNotEqual(first, second)
        self.assertEqual(sentinel.read_text(), "preserve")

    def test_malformed_current_run_is_rejected(self):
        self.base.mkdir(parents=True)
        (self.base / ".current_run").write_text("../../outside\n", encoding="utf-8")
        result = self.common("init_run_context resume")
        self.assertNotEqual(result.returncode, 0)

    def test_stale_config_backup_is_rejected(self):
        self.new_run()
        first = self.common("init_run_context resume; backup_default_game_ini")
        self.assertEqual(first.returncode, 0, first.stderr)
        self.config.write_text("unexpected external mutation\n", encoding="utf-8")
        second = self.common("init_run_context resume; backup_default_game_ini")
        self.assertNotEqual(second.returncode, 0)

    def test_duplicate_operating_mode_is_rejected_without_mutation(self):
        duplicate = self.original_bytes.replace(
            b"OperatingMode=Demo\n", b"OperatingMode=Demo\nOperatingMode=Research\n"
        )
        self.config.write_bytes(duplicate)
        self.new_run()
        result = self.common(
            "init_run_context resume; backup_default_game_ini; set_operating_mode Research"
        )
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(self.config.read_bytes(), duplicate)

    def test_config_exact_byte_restore(self):
        self.new_run()
        result = self.common(
            "init_run_context resume; backup_default_game_ini; "
            "set_operating_mode Research; restore_default_game_ini"
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(self.config.read_bytes(), self.original_bytes)

    def test_apk_selector_rejects_stale_and_multiple(self):
        archive = self.root / "archive"
        archive.mkdir()
        one = archive / "TranquilMind-arm64.apk"
        one.write_bytes(b"one")
        os.utime(one, (100, 100))
        stale = self.common(f"find_run_arm64_apk {archive!s} 200")
        self.assertNotEqual(stale.returncode, 0)
        os.utime(one, (300, 300))
        two = archive / "Other-arm64.apk"
        two.write_bytes(b"two")
        os.utime(two, (300, 300))
        multiple = self.common(f"find_run_arm64_apk {archive!s} 200")
        self.assertNotEqual(multiple.returncode, 0)

    def test_jsonl_path_selector_rejects_wrong_session(self):
        result = self.common(
            "validate_session_jsonl_path Current-Session /tmp/Old-Session.jsonl"
        )
        self.assertNotEqual(result.returncode, 0)

    def run_inspector(self, records, expected_session=SESSION):
        path = self.root / "fixture.jsonl"
        write_jsonl(path, records)
        result = subprocess.run(
            [
                "python3", str(INSPECTOR), str(path),
                "--expected-session", expected_session,
                "--json-out", str(self.root / "summary.json"),
                "--markdown-out", str(self.root / "summary.md"),
            ],
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        summary = json.loads((self.root / "summary.json").read_text())
        return result, summary

    def test_inspector_passes_valid_50_trial_fixture(self):
        result, summary = self.run_inspector(make_records())
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertTrue(summary["protocol_integrity_pass"])
        self.assertTrue(all(
            value == "NOT VERIFIED"
            for value in summary["info"]["human_checks"].values()
        ))

    def test_inspector_required_failures(self):
        base = make_records()
        cases = {}

        cases["49_trials"] = base[:50] + base[51:]
        extra = copy.deepcopy(base[1])
        extra["trial"] = 50
        cases["51_trials"] = base[:-2] + [extra] + base[-2:]
        duplicate = copy.deepcopy(base)
        duplicate[2]["trial"] = 0
        cases["duplicate_index"] = duplicate
        ratio = copy.deepcopy(base)
        ratio[6]["stimulus"] = "GO"
        ratio[6]["outcome"] = "Hit"
        cases["wrong_ratio"] = ratio
        wrong_session = copy.deepcopy(base)
        wrong_session[1]["sessionId"] = "Other-Session"
        cases["wrong_session"] = wrong_session
        late = copy.deepcopy(base)
        trial = next(record for record in late if record.get("hadResponse"))
        trial["responseTimestampSec"] = trial["responseWindowDeadlineSec"] + 0.1
        trial["rtMs"] = (
            trial["responseTimestampSec"] - trial["realizedStimulusOnsetSec"]
        ) * 1000.0
        trial["behavioralResolutionTimestampSec"] = trial["responseTimestampSec"]
        trial["trialCompleteTimestampSec"] = trial["responseTimestampSec"]
        cases["response_after_deadline"] = late
        short = copy.deepcopy(base)
        short[1]["realizedStimulusOffsetSec"] = (
            short[1]["realizedStimulusOnsetSec"] + 0.2
        )
        cases["short_visual_exposure"] = short
        terminated = copy.deepcopy(base)
        terminated[1]["visualTerminatedByResponse"] = True
        cases["visual_terminated_by_response"] = terminated
        cases["missing_footer"] = base[:-1]
        footer_count = copy.deepcopy(base)
        footer_count[-1]["recordsWritten"] = 50
        cases["footer_count_mismatch"] = footer_count

        for name, records in cases.items():
            with self.subTest(name=name):
                result, summary = self.run_inspector(records)
                self.assertNotEqual(result.returncode, 0)
                self.assertFalse(summary["protocol_integrity_pass"])

    def test_automation_parser_fails_closed_and_passes_exact_evidence(self):
        raw = self.root / "automation.log"
        raw.write_text(
            "Found 1 automation tests based on 'Suite.Exact'\n"
            "Test Completed. Result={Success} Name={One} Path={Suite.Exact.One}\n"
            "**** TEST COMPLETE. EXIT CODE: 0 ****\n",
            encoding="utf-8",
        )
        output = self.root / "automation_summary.json"
        passed = subprocess.run([
            "python3", str(PARSER), "--suite", f"Suite.Exact|1|{raw}",
            "--output", str(output),
        ], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        self.assertEqual(passed.returncode, 0, passed.stderr)
        failed = subprocess.run([
            "python3", str(PARSER), "--suite", f"Suite.Exact|2|{raw}",
            "--output", str(output),
        ], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        self.assertNotEqual(failed.returncode, 0)

    def test_report_keeps_unconfirmed_human_checks_not_verified(self):
        run_id = self.new_run()
        result = subprocess.run(
            ["bash", str(REPORT_SCRIPT), "--resume", "--close"],
            env=self.env,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        report = (self.base / run_id / "FINAL_REPORT.md").read_text(encoding="utf-8")
        self.assertIn("| Quest headset checks | NOT VERIFIED |", report)
        self.assertIn("- Overall: **NOT VERIFIED**", report)


if __name__ == "__main__":
    unittest.main(verbosity=2)
