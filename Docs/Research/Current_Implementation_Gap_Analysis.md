# Current Implementation — Gap Analysis (DRAFT)

**Status:** DRAFT, read-only inspection. **No code was modified.** This compares the *current* C++ implementation against `TranquilMind_Protocol_Specification_v1.md` and `GoNoGo_Training_Evidence_Review.md`.
**Date:** 2026-07-08 · **Revised:** 2026-07-08 (user decisions applied — priorities reordered, §4)
**Inspected (read-only):** `Source/TranquilMind/…` — `TargetSpawnerComponent.{h,cpp}`, `TranquilMindTypes.h`, `TranquilMindSessionManager.{h,cpp}`, `TranquilMindPhysiologyReceiver.{h}` + partial `.cpp`, `TranquilMindTargetActor.h`; plus `Config/*`, materials, `Docs/Quest_Working_Baseline_Report.md`.

> Scope reminder: this is analysis only. Every "should change" below is a *recommendation for a future, separately-approved task*, not an instruction being executed now.

> **Design decisions adopted (2026-07-08) that this gap analysis now reflects.** Target population **adolescents 13–17**; research modes use **discrete flashed stimuli** (fixed location, ~400 ms visibility) while the demo may keep moving targets → **Demo and Research must be separated first**; core **GO:NOGO 80:20 frozen**; **response window is the sole adaptive axis** (so the dual ISI+noise staircase is fully replaced, not just trimmed); **scientific-EDA firmware roadmap** (32 Hz target) supersedes the "1 Hz maybe enough" framing for the science use case. Priority order rewritten in §4.

---

## 1. Executive summary

The codebase is a **well-structured demo scaffold with real research ambition already sketched in** (phased session HFSM, double-entry block ledger, dual staircase, hard-gate/spam/abort watchdogs, seeded RNG, physiology subsystem). But the **actively-running paths are demo/debug shortcuts**, and several of the "research" pieces conflict with the evidence-based protocol:

- **GO/NOGO generation:** running path is either **strict alternation** (demo/debug — scientifically invalid) or **memoryless Bernoulli** (Phase II — no run-length control, ratio drift). Neither is the constrained, seeded, replayable generator the protocol requires.
- **Adaptation:** current staircase adapts **two axes at once** (ISI *and* stimulus noise), which the evidence review flags as interpretability-destroying.
- **Response window:** three different values coexist (`1500` const, `10000` actor default, `2500` demo) — no single source of truth.
- **Timing:** ISI is essentially fixed (no jitter); demo overrides everything.
- **Physiology:** latest-sample-only, **not logged, not epoch-integrated**; 1 Hz is fine for demo, inadequate for EDA science.
- **Phase durations:** constants (1A=2 s, 1B=12 s) don't match their own display labels (00:00–01:00 / 01:00–02:00), a debug-shortening left in.

The good news: most fixes are **parameterization and generator/adaptation swaps**, not architectural rewrites. The ledger, phase machine, and event plumbing are largely reusable.

---

## 2. Element-by-element mapping

### 2.1 `TargetSpawnerComponent` — spawning & pacing
- **Current:** ticks; spawns one target at a time; pacing by elapsed-since-last-spawn vs an "effective ISI" chosen as `bDemoMode ? DemoMode_ISI_MS : (bDebugHardwareMode ? DebugHardware_ISI_MS : CurrentISI_MS)`. Demo/debug default **on** (`bDemoMode=true`, `bDebugHardwareMode=true`).
- **Spawn position** is a debug placement (200 cm in front of camera, or hardcoded fallback) with TEMP comments.
- **Gap:** pacing is a single fixed ISI, **no jitter** (Protocol requires jittered ITI). Demo/debug flags gate the entire behavior; the "real" path is rarely the one exercised. The component also assumes a **moving target**, which research mode no longer uses.
- **Verdict:** keep the component for **demo (moving-target)** mode; for **research mode**, drive **discrete flashed** stimuli at a **fixed location** with jittered ITI applied *after* each trial (config-driven). Demote demo/debug to explicit non-default modes. This is why "separate Demo and Research modes" is implementation priority #1 (§4).

### 2.2 GO/NOGO generation — `ChooseNextStimulusType()`
- **Current:**
  - `bDebugHardwareMode` → **strict alternate** (`DebugAlternateTypeCounter++ % 2`).
  - Phase 1B Baseline → **all GO**.
  - Phase II → **Bernoulli** `StimulusRandomStream.FRand() < 0.80` (seed `5202`).
- **Gaps vs Protocol §4:**
  - Alternation is perfectly predictable → invalid for any scored use.
  - Bernoulli has **no max-consecutive-GO/NOGO limit**, **no min-gap**, **no segment balancing**, **no exact NOGO count** (ratio drifts block to block), **no first-trial NOGO prohibition**, and **no separate training/evaluation seeds**.
  - It *is* seeded/replayable (good), but replay of an uncontrolled sequence isn't enough.
- **Verdict:** **replace** the generation logic with the seeded constrained randomizer (Protocol §4). This is the single most important scientific fix.

### 2.3 Response window
- **Current values (conflicting):** `TranquilMind::RESPONSE_WINDOW_MS = 1500` (used in `RecordTrialOutcome` RT validity gate); `ATranquilMindTargetActor::ResponseWindow_MS = 10000` default; demo sets `2500`. Scoring validity uses 1500, but the target's own expiry uses its own field.
- **Gap:** no single authoritative window; the value that gates "valid RT" (1500) differs from what the target actually enforces. Protocol wants a **single source**, and for the adopted **adolescent** target the **start value is 1200 ms (adaptive, bounds 800–1400 ms)** — not an adult 1000 ms.
- **Verdict:** **revise** to one config-driven window; reconcile the RT-validity gate and the target/trial expiry to the same value; this same window is the **sole adaptive axis** (§2.5). In the discrete-flashed model, the window starts at stimulus onset and may extend past the ~400 ms visibility.

### 2.4 ISI / timing
- **Current:** `MIN_ISI_MS=800`, `MAX_ISI_MS=4000`; live `CurrentISI_MS` from staircase; demo `3000`, debug `2000`. **No jitter** — pacing is deterministic given the ISI.
- **Gap:** Protocol requires jittered ISI (predictability confound, Evidence Review §3.5). Also the ISI here doubles as a **difficulty axis** (staircase), conflating pacing with adaptation.
- **Verdict (resolved):** ISI/ITI is a **fixed, jittered pacing parameter — frozen, NOT an adaptation axis**. Adaptation moves the response window only (§2.5). Add jitter as config; remove ISI from the staircase entirely.

### 2.5 Staircase / adaptive difficulty — `StepTrackA_ISI` + `StepTrackB_Noise`
- **Current (two independent tracks, per 30 s block):**
  - **Track A (ISI):** if `N_Omission >= 2` → easier (ISI += 100 ms); if `N_Omission==0` **and** `RT_Median < RT_Baseline` (vs last clean block) → harder (ISI −= 100 ms); else deadband counter, and after 3 holds a −50 ms micro-perturbation. A custom "conditional-down / 2-omission-up" rule with deadband — **not** a canonical 3-down/1-up.
  - **Track B (Noise):** if `N_Commission==0` → harder (noise += 0.10); if `N_Commission>=1` → easier (noise −= 0.10); else deadband, +0.05 micro after 3 holds.
- **Gaps:**
  - **Two axes adapt simultaneously and independently every block** → interpretability destroyed (Evidence Review §3.11, Protocol §1). A performance change can't be attributed to ISI or noise.
  - 30 s blocks at 80:20 yield **very few NOGO trials per block** (~a handful), so Track B's `N_Commission==0/≥1` decisions rest on tiny samples → noisy adaptation.
  - Adapts on **ISI (pacing)** and **noise (discrimination)**, but the Protocol's recommended default axis is the **response window** — neither track touches it.
  - "Micro-perturbation always makes it harder" biases difficulty upward during deadband.
- **Verdict (resolved):** **fully replace** both tracks with a **response-window-only** staircase. ISI/noise no longer adapt (both frozen). Provisional **3-down/1-up**, step **50 ms**, adolescent start **1200 ms**, bounds **800–1400 ms**; evaluation blocks non-adaptive; **do not step until the block has ≥8–10 NOGO trials** (and ~40–50 total). Keep the deadband/hysteresis idea only if it applies to the single window axis. Keep the ledger that feeds it. The `NoiseAlpha` machinery becomes a frozen fixed value (stimulus visual difficulty is frozen).

### 2.6 Outcome logic — omission / commission / correct rejection / hit / void
- **Current:** `ETMTrialOutcome{Hit, Omission, CorrectRejection, Commission, Void}` recorded via `RecordTrialOutcome`; RT added only if `>0 && <= RESPONSE_WINDOW_MS(1500)`; per-block counters + median RT; `Void` for non-scored exposures. Target actor resolves via trigger, void, or expiry.
- **Note (demo shortcut):** in the running `HandleTriggerPulled` **DEBUG** path, a trigger **resolves ALL active targets** and **bypasses gaze tie-break** (`SelectTargetByTieBreak` exists but is commented out). So the current trigger→outcome binding is demo-simplified, not the per-target gaze-bound resolution the tie-break code implies.
- **Verdict:** the **outcome taxonomy is correct and defensible** — keep it. **Restore** per-target resolution (tie-break) for scored use; the "resolve all" path is demo-only. Reconcile the RT-validity window (§2.3).

### 2.7 HardGate / spam / abort watchdogs
- **Current:** gaze-smoothing hard gate (off-center accumulation → suspend, dwell → resume, 30 s suspend → abort), spam debounce (`DEBOUNCE_MIN_MS=50`, spam block threshold 15, 3 consecutive spam blocks → abort), HMD-removal 10 s abort. Demo mode **bypasses** hard-gate suspend (`bBypassHardGateSuspend=true`).
- **Verdict:** these are **reasonable data-quality/safety mechanisms** and largely **leave untouched** — but they are *quality control*, not inhibition science. Keep; ensure their parameters are config and their events are logged. Note that demo bypass must never be on for scored sessions.

### 2.8 Session timing / phase machine
- **Current:** `SESSION_TOTAL_SEC=1200` (20 min); phases 1A Desensitization, 1B Baseline, II Core, III Cooldown. But `PHASE_1A_END_SEC=2.0`, `PHASE_1B_END_SEC=12.0` while the enum **display names claim 00:00–01:00 and 01:00–02:00** (i.e., 60 s / 120 s). So the actual 1A=2 s, 1B=10 s are **debug-shortened and mislabeled**. Demo mode ignores phases entirely (120 s hard stop).
- **Gaps:** phase durations don't match labels; baseline of ~10 s is far too short to establish a stable RT (or GSR) baseline (Protocol wants 60–90 s baseline). Block = fixed 30 s regardless of trial count.
- **Verdict:** **revise** phase durations to match the protocol (and fix the labels); make block boundaries **trial-count-aware** (≥8–10 NOGO) rather than a flat 30 s wall-clock, or lengthen blocks.

### 2.9 GSR epoch logic / physiology
- **Current:** `UTranquilMindPhysiologyReceiver` is a WorldSubsystem listening UDP on port **4210**, parsing JSON into `FTMPhysiologySample{Raw, Smoothed, Quality, TimestampMs, bIsValid}`, caching **only the latest sample** (`GetLatestSample`). Console var `tranquilmind.physiology.enabled`. **No buffering, no logging to disk, no baseline computation, no block/epoch integration** into `FSessionStats`/`FBlockStats`.
- **Gaps vs Protocol §6:** no baseline establishment, no epoch-boundary trend, no preservation of raw+smoothed+quality+timestamps+behavioral-event alignment (only latest value survives). Rate assumed 1 Hz (firmware) → fine for demo viz + coarse trend, **inadequate for scientific EDA** — and scientific EDA is now the stated future goal.
- **Verdict (two-stage, resolved):**
  - **Stage 1 (software, v1):** add a logged, timestamped physiology buffer aligned to trial/block markers, plus baseline + coarse epoch-trend computation at block boundaries. **No closed-loop control.** Keep 1 Hz demo stream as-is.
  - **Stage 2 (firmware, separate future track — out of scope now):** raise the ESP32 sampling toward the **32 Hz** scientific-EDA target with **batched UDP transport, board timestamps, packet sequence numbers, raw-sample preservation, calibrated conductance units if feasible, motion-artifact/quality flags, and stimulus/response synchronization**, then **validate against a research-grade EDA device**. Per Protocol Spec §6.3, **32 Hz alone is necessary-not-sufficient**. This stage touches ESP32 firmware and is explicitly **not** done here.
  - **Do not** wire GSR into difficulty in v1.

---

## 3. Gap analysis buckets

### 3.1 Scientifically-defensible elements already present (keep)
- Outcome taxonomy (Hit/Omission/CorrectRejection/Commission/Void) and **double-entry per-block + per-session ledger** (`FBlockStats`/`FSessionStats`).
- **Seeded RNG** with an auditable seed (`StimulusRandomSeed`) — good foundation for reproducibility (just needs the constrained generator on top).
- 80:20 GO:NOGO intent (`GoProbability=0.80`) matches the recommended ratio.
- Median-RT computation per block; baseline-RT capture from block 0.
- Phase-structured session (baseline vs core vs cooldown) — right shape.
- Data-quality machinery: debounce/spam, hard gate, HMD-removal abort, void-rate QA (`QA_MAX_VOID_RATE`).
- Physiology subsystem exists, parses raw/smoothed/quality/timestamp — right fields.
- Tie-break gaze-binding logic (`SelectTargetByTieBreak`) exists and is sound (currently disabled).

### 3.2 Demo-only shortcuts (must be gated off / relabeled for scored use)
- `bDemoMode=true`, `bDebugHardwareMode=true` **defaults**.
- **Strict GO/NOGO alternation** in demo/debug.
- Trigger **resolves ALL targets**, gaze tie-break **bypassed**.
- Single-target-on-screen cap (fine for scoring, but currently a debug rule).
- Debug spawn placement (200 cm front / hardcoded).
- Demo 120 s hard stop that ignores the phase machine.
- Hard-gate suspend bypass in demo.

### 3.3 Parameters requiring revision
- **Response window** unified to one config value (reconcile 1500/10000/2500) → **adolescent start 1200 ms, bounds 800–1400 ms, adaptive**; not adult 1000 ms.
- **ITI** → add jitter (±30%); **frozen pacing**, removed from adaptation.
- **Phase durations** → match protocol + fix mislabeled enum display names; baseline ≥ 60–90 s.
- **Block definition** → trial-count-aware (≥8–10 NOGO) instead of flat 30 s.
- **Stimulus duration** → **~400 ms, frozen** (research mode).

### 3.4 Logic requiring replacement
- **Stimulus-presentation model (research mode)** → **discrete flashed** (fixed location, ~400 ms visibility, disappear, respond within active window, jittered ITI) replacing moving-target behavior; **demo mode keeps moving targets** → the two modes must be separated first.
- **GO/NOGO generation** → seeded constrained randomizer (run-length caps, min-gap, exact 80:20 count, segment balancing, no-NOGO-first, separate seeds, schedule logging/replay).
- **Dual-track staircase** → **response-window-only** adaptation, evaluation blocks non-adaptive; ISI + noise frozen.
- **GSR handling** → Stage 1: logged, epoch-aligned buffer + baseline/trend (no closed loop). Stage 2 (separate firmware track): 32 Hz + batched/timestamped/sequence-numbered transport + validation.

### 3.5 Logic that should remain untouched
- Interrupt priority hierarchy and abort/watchdog state machine (data-quality/safety), aside from making parameters config and ensuring logging.
- Ledger structures and aggregation.
- Event/delegate plumbing between SessionManager and spawner.
- UDP receiver transport (port/parse) — transport is fine; only add logging/rate decisions on top.

### 3.6 Fields that must become configuration data (not hard-coded constants)
Currently hard-coded in `TranquilMindTypes.h` / spawner and should be externalized to a config/data asset so protocols A/B/C differ by data, not code:
`GoProbability`, `RESPONSE_WINDOW_MS`, `MIN/MAX_ISI_MS` + **new ISI mean/jitter**, `BLOCK_DURATION_SEC` (→ trial-count target), phase end times, staircase steps/deadbands/bounds (`ISI_STEP_*`, `NOISE_STEP_*`, `DEADBAND_*`, `MIN/MAX_NOISE_ALPHA`), debounce/spam thresholds, hard-gate timings, `StimulusRandomSeed` (+ separate eval seed), and **all new constrained-randomizer parameters** (`maxConsecutiveGo/NoGo`, `minGapAfterNoGo`, `forbidNoGoFirstN`, `segmentCount`, `targetNoGoRatio`). Rendering tunables (opacity/Fresnel/rim/colors) likewise → material parameters (see Bubble doc §4.3).

---

## 4. Priority ordering for a future (separately-approved) implementation pass

**Revised order (adopted 2026-07-08):**

1. **Separate Demo and Research modes.** Make mode an explicit, non-default selection; demo (moving targets, 120 s, non-scientific) and research (adolescent, discrete flashed, logged) share no accidental state. This must come first because the two modes now use *different stimulus-presentation models* — nothing downstream is safe until they are cleanly split.
2. **Implement discrete flashed stimuli for Research mode.** Fixed spatial presentation location; ~400 ms visibility then disappear; response accepted within the active response window (which may extend past visibility); jittered ITI after each trial. Replaces the moving-target `ATranquilMindTargetActor` behavior *in research mode only*.
3. **Add constrained seeded sequence generator.** Replace alternation/Bernoulli with the exact-80:20, run-length-capped, segment-balanced, no-NOGO-first, replayable generator (separate train/eval seeds).
4. **Unify response-window timing.** Reconcile the conflicting `1500 / 10000 / 2500` values into one config-driven window; adolescent start 1200 ms, bounds 800–1400 ms; add ITI jitter; externalize to config.
5. **Implement response-window-only adaptation.** Replace the dual ISI+noise staircase entirely with a single response-window staircase (provisional 3-down/1-up, step 50 ms); ratio, stimulus duration, ITI, and stimulus difficulty frozen; evaluation blocks non-adaptive; adaptation gated on ≥8–10 NOGO trials/block.
6. **Redefine blocks by trial / NOGO count.** Replace flat 30 s wall-clock blocks with trial-count-aware blocks (≥8–10 NOGO); lengthen baseline to 60–90 s; fix the mislabeled phase-duration enum display names.
7. **Upgrade physiology logging, then (later) firmware rate.** First: logged, timestamped, event-aligned buffer + baseline + coarse epoch trend (no closed loop). Later, separate firmware track: raise ESP32 rate toward the **32 Hz** scientific-EDA target with batched UDP, board timestamps, packet sequence numbers, quality/artifact flags, and validation against a research-grade device (Protocol Spec §6.3). 32 Hz alone is necessary-not-sufficient.
8. **Keep bubble rendering as a separate track** (Bubble doc), decoupled from the training-logic work above.

Cross-cutting (fold into the steps above, not a separate phase): **restore per-target tie-break resolution** for research scoring (the "resolve all targets" trigger is demo-only), and **externalize all hard-coded parameters to config** (§3.6).

None of the above is performed in this task.

---

## 5. Sources (implementation facts)
Local source inspection (this repo, read-only):
- `Source/TranquilMind/TranquilMindTypes.h` — constants, enums, `FBlockStats`/`FSessionStats`.
- `Source/TranquilMind/Public/TargetSpawnerComponent.h` and `Private/TargetSpawnerComponent.cpp` — spawning, pacing, `ChooseNextStimulusType`, demo/debug flags, `HandleTriggerPulled`, tie-break.
- `Source/TranquilMind/TranquilMindSessionManager.{h,cpp}` — phase HFSM, `RecordTrialOutcome`, `SealCurrentBlock`, `StepTrackA_ISI`, `StepTrackB_Noise`, hard-gate/watchdogs.
- `Source/TranquilMind/TranquilMindPhysiologyReceiver.h` (+ partial `.cpp`) — UDP GSR subsystem, `FTMPhysiologySample`, port 4210.
- `Source/TranquilMind/Public/Private/Private/TranquilMindTargetActor.h` — target motion/resolution, `ResponseWindow_MS`.
- `Config/DefaultEngine.ini`, `Config/Android/AndroidEngine.ini` — render config.
- `Docs/Quest_Working_Baseline_Report.md` — verified device behavior, example logs.

Protocol/evidence rationale: `TranquilMind_Protocol_Specification_v1.md`, `GoNoGo_Training_Evidence_Review.md`.
