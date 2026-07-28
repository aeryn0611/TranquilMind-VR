# TranquilMind Phase 1 — Local Verification Toolkit

A small, safe toolkit to build, test, package, and verify **Phase 1 Research Mode**
on macOS + Meta Quest, and to run a Demo regression. It writes all output under a
timestamped directory and **never** commits, stages, pushes, or touches Git history.

> Scripts cannot verify anything you must see or do inside the headset (flashed
> bubbles, early/late/duplicate trigger presses, the Demo Summary). Those are
> `NOT VERIFIED` unless the user explicitly confirms them.

## Fixed paths (baked into `phase1_verify_common.sh`)

```
PROJECT_ROOT = /Users/aeryn/UnrealProjects/TranquilMind
UPROJECT     = $PROJECT_ROOT/TranquilMind.uproject
UE_ROOT      = /Users/Shared/Epic Games/UE_5.7
ADB          = /Users/aeryn/Library/Android/sdk/platform-tools/adb
PACKAGE      = com.tranquilmind.vr
ACTIVITY     = com.epicgames.unreal.GameActivity
```

## Execution order

Every operation is explicit:

- `--new-run` atomically creates a unique `Verification/Phase1/<timestamp>/`
  directory and updates the validated `.current_run` pointer. It never deletes or
  overwrites an old run.
- `--resume` intentionally resumes only the validated active run. Missing,
  malformed, symlinked, escaped, or external pointers are rejected.
- `--resume --close` restores owned state, writes the report, and closes the active
  pointer without deleting run evidence.

Do not edit or delete `.current_run` manually. The scripts are intentionally invoked
through `bash`, so executable permission bits are not required.

```bash
# 1. Create a fresh run, build the editor target, and run both Research suites.
bash Tools/Verification/phase1_build_and_test.sh --new-run

# 2. Explicitly resume and package Research Mode.
bash Tools/Verification/phase1_package_research.sh --resume

# 3. Resume, install, launch, capture raw logs, collect explicit human checks,
#    pull only the current-session JSONL, and run the strict inspector.
bash Tools/Verification/phase1_capture_research.sh --resume

# 4. Restore exact config/device state, aggregate evidence, and close the run.
bash Tools/Verification/phase1_restore_and_report.sh --resume --close
```

Demo is a separate later gate and must not run until Research verification passes:

```bash
bash Tools/Verification/phase1_package_demo.sh --resume
bash Tools/Verification/phase1_capture_demo.sh --resume
```

For an intentional non-default or interrupted JSONL inspection, use the Inspector's
explicit options such as `--expected-seed`, `--disable-default-contract`,
`--expected-termination interrupted`, or `--allow-void`.

## Which steps need what

| Step | Quest connected? | Wearing the headset? |
|---|---|---|
| 1 build + tests | no | no |
| 2 package Research | no | no |
| 3 capture Research | **yes** | **yes** (checklist) |
| 4 restore + report + close | yes if device state was captured | no |
| later Demo package | no | no |
| later Demo capture | **yes** | **yes** (checklist) |

## Where outputs go

```
Verification/Phase1/<timestamp>/
  git/            read-only git snapshots
  build/          build.log, first_error_context.txt
  tests/          raw Sequence/Lifecycle UE logs and automation_summary.json
  research/       package/APK metadata, install/launch evidence, raw + filtered logcat
  jsonl/          exact session path, pull metadata/hash, JSONL, strict JSON/Markdown summaries
  demo/           package log, apk_path.txt, human_checklist_status.txt
  demo_logs/      raw + filtered Demo logcat
  device/         adb inventory/serial, logcat process ownership, Quest power state
  config/         exact DefaultGame.ini snapshot, SHA-256 manifest, restore status
  FINAL_REPORT.md          evidence-driven aggregate report
```

Packaged APKs are archived under
`ArchivedBuilds/Phase1Verification/<timestamp>/{Research,Demo}/`.
The toolkit never deletes `Packaged/`, `ArchivedBuilds/`, Content assets, or user files.

## Safety guarantees

- Every shell script uses `set -euo pipefail`, explicit new/resume semantics,
  fail-closed path validation, and timestamped evidence.
- No script runs `git clean/reset/checkout/restore/commit/add/push`.
- Each run captures one fresh exact-byte `DefaultGame.ini` backup plus original and
  backup SHA-256 values. Duplicate mode lines fail before mutation; restore is
  hash-verified on normal exit, failure, SIGINT, and SIGTERM.
- Device steps require exactly one authorized Quest, or an explicit verified
  `ADB_SERIAL`. Every device command is scoped with `adb -s`.
- The original Quest stay-awake bitmask is persisted, USB stay-awake is enabled
  temporarily, and the exact value is restored and verified even from a later
  script process.
- Raw logcat uses one tracked adb PID rather than a pipeline. PID, command, process
  start identity, UTC start/stop times, and stop result are recorded. Only an
  ownership-verified process is stopped.
- APK selection is restricted to new arm64 artifacts in the current run archive.
- JSONL selection uses the session ID proven by the current launch logs; there is
  no newest-file fallback.

## Aborting safely

Press **Ctrl+C** at any point. The active script's traps stop its ownership-verified
logcat, force-stop only this app where applicable, and restore any state it mutated.
If interrupted between scripts, run:

```bash
bash Tools/Verification/phase1_restore_and_report.sh --resume --close
```

The restore command uses only backups and device identity recorded by that run.

## If the build fails

Send back the first compiler-error context:

```
Verification/Phase1/<timestamp>/build/first_error_context.txt
```

(and, if useful, the tail of `build/build.log`). The most version-sensitive line is
the `EAutomationTestFlags` expression in `Source/TranquilMind/Private/Tests/TMSequenceGeneratorTest.cpp`.

## Notes

- This toolkit does **not** commit or push anything.
- Human visual/timing actions inside the headset cannot be independently verified by
  scripts. Each remains `NOT VERIFIED` unless the user explicitly confirms it.
- `tests/fixtures/` contains **synthetic** JSONL files used only to self-check the
  inspector — they are not real verification data.

## Toolkit self-tests

These tests use only temporary project/config/device-free fixtures:

```bash
bash -n Tools/Verification/*.sh
python3 -m py_compile Tools/Verification/*.py
python3 Tools/Verification/tests/test_phase1_toolkit.py
```
