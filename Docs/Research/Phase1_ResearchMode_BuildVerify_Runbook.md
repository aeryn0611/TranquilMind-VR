# Phase 1 Research Mode — Build & Verify Runbook

**Purpose:** exact steps to build, test, package, and verify the new Research Mode on your Mac + Quest. These steps require Unreal Engine + a connected headset and must be run on your machine (they cannot run in the assistant's sandbox).

**Nothing here is committed.** Verify both modes, then decide.

> Replace `$UE` with your UE 5.7 engine path (e.g., `/Users/Shared/Epic Games/UE_5.7` or your source build). Project is at `/Users/aeryn/UnrealProjects/TranquilMind`.

---

## 0. What changed (mode switch)

Operating mode is chosen in `Config/DefaultGame.ini`:

```
[/Script/TranquilMind.TMResearchSettings]
OperatingMode=Demo      ; <- default; Demo = verified moving-target portfolio
```

- **For the packaged Quest verification, use the config file and repackage — this is the reliable path:**
  - **Research run:** set `OperatingMode=Research` in `DefaultGame.ini`, then package.
  - **Demo run:** set `OperatingMode=Demo` (default), then package.
  Each mode is a separate package for on-device testing.
- **Development-only convenience (editor / PIE):** the console variable `tranquilmind.OperatingMode` (`0`=Demo, `1`=Research, `-1`=use ini) can be typed in the editor console (`tranquilmind.OperatingMode 1`) while play-testing in the editor.
- **Do not assume a runtime console is reachable on a packaged Quest build.** There is no guaranteed accessible on-device console, so the cvar is **editor/development-only** here; for Quest, switch modes via `DefaultGame.ini` and repackage. (No new runtime settings UI was added in this task.)

---

## 1. Compile the editor target (also compiles the automation tests)

```bash
"$UE/Engine/Build/BatchFiles/Mac/Build.sh" TranquilMindEditor Mac Development \
  -project="/Users/aeryn/UnrealProjects/TranquilMind/TranquilMind.uproject"
```

Expect a clean build. New C++ files that must compile:
`TMResearchConfig.h`, `TMResearchSettings.{h,cpp}`, `TMSequenceGenerator.{h,cpp}`,
`TMResearchRunner.{h,cpp}`, `TMTrialLogger.{h,cpp}`, `Private/Tests/TMSequenceGeneratorTest.cpp`.

> If `EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter` errors on your exact 5.7 revision, that's the one likely tweak (flag names occasionally shift between engine versions); adjust the flag expression in `TMSequenceGeneratorTest.cpp` only.

## 2. Run the sequence-generator automation tests (headless)

```bash
"$UE/Engine/Binaries/Mac/UnrealEditor-Cmd" \
  "/Users/aeryn/UnrealProjects/TranquilMind/TranquilMind.uproject" \
  -ExecCmds="Automation RunTests TranquilMind.Research.Sequence; Quit" \
  -unattended -nopause -nullrhi -nosplash -log
```

Expect all to pass:
`TranquilMind.Research.Sequence.ExactCounts`, `.Constraints`, `.SegmentBalance`, `.Determinism`, `.InfeasibleFailsLoudly`, `.BoundaryStress`.
(These mirror the standalone tests already validated: 40 GO / 10 NOGO, run caps, first-trial rule, **per-segment NOGO quotas differing by ≤1**, deterministic replay, fail-loud infeasibility, and a **cross-segment-boundary run-constraint** case.)

## 3. Package for Quest

```bash
"$UE/Engine/Build/BatchFiles/RunUAT.sh" BuildCookRun \
  -project="/Users/aeryn/UnrealProjects/TranquilMind/TranquilMind.uproject" \
  -platform=Android -cookflavor=ASTC -clientconfig=Development \
  -cook -build -stage -pak -package -archive \
  -archivedirectory="/Users/aeryn/UnrealProjects/TranquilMind/ArchivedBuilds"
```

## 4. Install + launch on Quest

```bash
adb install -r /path/to/TranquilMind-arm64.apk
adb shell am start -n com.tranquilmind.vr/com.epicgames.unreal.GameActivity
```

## 5. Capture logs

```bash
adb logcat -c
adb logcat | grep -E "TMResearch|Research|Mode|TargetSpawner|Session|TargetActor"
```

---

## 6. RESEARCH verification checklist (with `OperatingMode=Research`)

Confirm in logcat:

- [ ] `[Mode] ===== TranquilMind OperatingMode = RESEARCH =====`
- [ ] `[Session] Started | ... | Mode=Research`
- [ ] `[Research][Logger] OPEN ok | session=... | path=...`
- [ ] `[Research] Schedule OK | trials=50 | GO=40 NOGO=10 | segNoGo=[3/3/4] | seed=20260708 | hash=4922870221080512783`
  - the **per-segment NOGO counts** (`segNoGo`) must differ by at most 1 (e.g., `3/3/4`)
  - for `seed=20260708` the schedule line is exactly:
    `[Research] Schedule: .....N......N.N...N.N......N......N...N..N.N......` (50 chars, exactly 10 `N`)
  - re-running the same seed must print the **same hash** (`4922870221080512783`) → deterministic replay
- [ ] Per-trial lines `[Research][Trial] Bx Tyy | GO/NOGO -> Outcome | RT=..ms | visible=~400ms | afterOffset=.. | win=1200ms | iti(sched/real)=..` — note **`visible` ≈ 400 ms on every trial**, including trials with an early response
- [ ] ITI values (`iti(sched/real)`) fall in **840–1560 ms**
- [ ] At least one of each outcome across the block: **GO Hit, GO Omission, NOGO Commission, NOGO CorrectRejection**
- [ ] **Fixed-exposure invariant:** an early GO response (~150–250 ms) still shows `visible≈400ms` and `visualTerminatedByResponse=false` in the JSONL — the bubble is **not** cut short by the response
- [ ] **Late-response invariant:** a response ~500–1100 ms after onset (after the bubble has disappeared) is still accepted (`afterOffset=true`, outcome Hit/Commission)
- [ ] **Duplicate-response invariant (test the behavior, not an exact string):** a second press after the outcome is decided (or during ITI) must **not** change the outcome, **not** create a second record, and **not** start/end another trial. Confirm: exactly **one** trial record per `trial` index in the JSONL, and the block's `Hit+Omit+CR+Comm+Void == 50`.
- [ ] No "resolve ALL targets" line appears (that is Demo-only)
- [ ] `[Research] BLOCK BEHAVIORAL-COMPLETE | trials=50 | Hit=.. Omit=.. CR=.. Comm=.. Void=..`
- [ ] **Persistence status distinct from behavioral completion:** `[Research][Logger] CLOSED healthy | ... records=51 failures=0` (records = 50 trials + 1 block summary). If instead you see `CLOSED UNHEALTHY` or any `[Research][Logger] WRITE FAILED`, the block ran but the log did **not** persist reliably.
- [ ] No crash / no `Fatal` in logcat
- [ ] JSONL file on device at `.../UnrealGame/TranquilMind/Saved/TranquilMind/Research/<sessionID>.jsonl` (`adb pull` to inspect); it ends with a `session_footer` line reporting `healthy` + `recordsWritten`

Behavioral sanity to eyeball in the headset:
- [ ] Bubble appears at ONE fixed location (does not travel in depth)
- [ ] Bubble is visible for a fixed ~0.4 s then disappears; a GO can still be "hit" for a moment after it vanishes (until 1.2 s); pressing early does **not** make the bubble vanish sooner

## 7. DEMO regression checklist (with `OperatingMode=Demo`, repackage)

- [ ] `[Mode] ===== TranquilMind OperatingMode = DEMO =====`
- [ ] Moving-target bubbles spawn and travel (unchanged behavior)
- [ ] Right trigger resolves targets as before
- [ ] GSR LIVE panel still updates
- [ ] Session ends naturally at ~120 s and Demo Summary panel shows
- [ ] No crash

---

## 8. If something fails

- Schedule line missing / `FATAL: schedule generation failed` / `Infeasible:` → a config value is over-constrained or out of range; check the `[/Script/TranquilMind.TMResearchSettings]` block. The generator fails loudly rather than emitting a relaxed schedule.
- No trials at all in Research → confirm the mode line says RESEARCH; confirm a `UTargetSpawnerComponent` exists in the level (same actor the Demo uses).
- Timings off → confirm `ResponseWindowMs=1200`, `StimulusVisibleMs=400` in the ini.
- `[Research][Logger] OPEN FAILED` / `WRITE FAILED` / `CLOSED UNHEALTHY` → the behavioral block may still have run, but the JSONL did not persist reliably; treat the run's data as suspect. Check device storage/permissions for the Saved path.
- Report the first `Error`/`Fatal` logcat line back for diagnosis.

## 9. Consistency notes (what the terms mean)

- **Stimulus offset** is always reported as two separate fields: `scheduledStimulusOffsetSec` (realized onset + 400 ms) and `realizedStimulusOffsetSec` (actual hide time). A scheduled value is never labelled realized.
- **Research stimulus exposure is fixed at 400 ms**; a response never shortens it (`visualTerminatedByResponse` is always `false`).
- **Research response window is 1200 ms** in Phase 1; **no adaptation is active** (`bEnableResponseWindowAdaptation=false`).
- **Research ITI** is seeded, uniform, **840–1560 ms**, and never coupled to performance.
- **NOGO count is exactly 10 of 50**, balanced per segment (counts differ by ≤1).
- **Demo behavior is unchanged.**
- **GSR** in the JSONL is the current **1 Hz latest sample, contextual only — NOT synchronized scientific EDA**, and nothing here is a claim of scientific validation.

Do not commit until both Research and Demo checklists pass.
