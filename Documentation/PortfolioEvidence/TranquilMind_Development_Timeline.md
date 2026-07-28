# TranquilMind — Development Timeline (Reconstructed from Evidence)

Audit date: 2026-07-28. Reconstructed from git history, repo documents, verification artifacts, saved logs, and dated session records. **Facts** carry evidence pointers; **design interpretation** is labeled as such. Design documents describe intent, never completion.

Legend for each milestone: Problem → Diagnosis → Change → Verification → Result → User observation → Decision → Status.

---

## M1 · Initial VR task prototype — ≤2026-06-21

- **Problem:** No working VR task loop existed; earliest config activity 2026-06-08 (baseline report).
- **Change:** Void map (`L_TranquilMind_Void`, `M_TheVoid`), `ATranquilMindSessionManager` phase machine (1A→1B→II), target actor + spawner, VR pawn.
- **Verification:** On-device logcat chain (10 checklist items) in `Docs/Quest_Working_Baseline_Report.md`.
- **Result:** Full loop confirmed: spawn 200 cm ahead → trigger → resolve → next → Phase II with `BaselineRT=900.0`.
- **User observation:** Working, but only with TEMP debug state (scale 1.0, resolve-all-trigger, hardcoded spawn).
- **Decision:** Freeze as baseline; create `Docs/Backup_20260621/` restore checkpoint (project was **not yet a git repo**).
- **Status:** DEVICE-VERIFIED (superseded in details by later passes).

## M2 · Quest packaging + input fixes — 2026-06-21

- **Problem:** APK wouldn't enter VR / had no content / crashed on VR shaders; Enhanced Input trigger axis always read 0.
- **Diagnosis (four independent root causes):** missing Meta VR manifest entries; `bStartInVR` absent so the OpenXR path never activated on Android; UE5's `-build` runs Gradle before staging so only `-package` embeds content; `r.MobileMultiView` not set at cook so VR shaders missing. Input: `GetInputAxisKeyValue` unusable in this setup.
- **Change:** `bPackageForMetaQuest=True` + APL manifest injection + `bStartInVR=True` + full BuildCookRun with `-package` + `vr.MobileMultiView=1`; input switched to `BindKey(OculusTouch_Right_Trigger_Click)`.
- **Verification/Result:** On-device baseline above; APK 137.6 MB. **Status:** DEVICE-VERIFIED. Commits `9d437a6`…`a478e6b` (git initialized at this point).

## M3 · Physiology UDP integration — 2026-06-22 → 2026-07-06

- **Problem:** No physiological signal in the experience.
- **Change:** ESP32 NodeMCU + Grove GSR firmware (1 Hz JSON over UDP :4210) → `UTranquilMindPhysiologyReceiver` (WorldSubsystem, thread-safe marshalling) → GSR status panel → demo summary panel. Commits `789d32c`, `b08a862`, `02d6e41`.
- **Verification:** Independent hop-by-hop checks (serial monitor, Mac UDP listener, Mac `-game` standalone, Quest logcat). Device: 57 packets/57 s; `[Physiology] GSR raw=2368 smooth=2361 qual=97`; 120 s demo with live panel and auto summary.
- **Result/Decision:** PASS; explicitly labeled contextual 1 Hz stream, not scientific EDA; 32 Hz firmware deferred to roadmap.
- **Status:** DEVICE-VERIFIED (`Docs/Verification/GSR_Quest_Phase2_2026-07-05.md`, `Docs/Portfolio/TranquilMind_Quest_GSR_Demo_Milestone.md`).
- Side finding (fact): Quest auto-pauses unworn headsets — "silent app" symptom documented with wake/relaunch recipe.

## M4 · Research direction pass (documents only) — 2026-07-08

- **Problem:** Running build was a demo/debug shortcut being mistaken for a research task (strict alternation "scientifically invalid"; three conflicting response windows; dual staircase "interpretability destroyed"; debug-shortened mislabeled phases).
- **Change:** Four DRAFT documents in `Docs/Research/` (gap analysis, Go/NoGo evidence review, Protocol Specification v1, bubble rendering research). **No code modified.**
- **Decision:** Adolescents 13–17; 80:20 frozen ratio; 400 ms stimulus; window-only adaptation (1200 ms start, 800–1400 bounds, 3-down/1-up on GO only); priority #1 "Separate Demo and Research modes".
- **Status:** Documents IMPLEMENTED as plans; all protocol values are PILOT DEFAULTS, not validated. (Design interpretation, clearly bounded by the docs themselves.)

## M5 · Research/Demo separation implemented — ≤2026-07-20

- **Change (fact, from working tree):** `UTMResearchRunner`, `FTMSequenceGenerator`, `FTMTrialLogger`, `UTMResearchSettings` + `tranquilmind.OperatingMode`; JSONL schema v2; 8 automation tests.
- **Verification:** Archived run `Verification/Phase1/20260720_075848`: build COMPILED, **8/8 tests pass** (summary + raw logs, `### EXIT: 0`), Research APK packaged (sha256 `09fadb7a…3932b9`), config restored byte-exact (hash-verified).
- **Result:** Mac-side half of Phase 1 fully evidenced. **Device half never ran** — `adb devices` empty, `jsonl/`/`demo/` dirs empty, run pointer still open, no FINAL_REPORT.md.
- **Status:** AUTOMATION-VERIFIED; on-device Research capture OUTSTANDING.

## M6 · Verification toolkit — ≤2026-07-20

- **Change:** `Tools/Verification/` fail-closed pipeline (build/test → package → device capture → restore/report), strict JSONL inspector with hard-coded default contract (seed 20260708, hash 4922870221080512783), fail-closed automation parser, 12 toolkit unit tests, fake-fixture negative controls.
- **Design principle (stated in README):** never marks a step PASS without evidence; human visual checks stay "NOT VERIFIED" unless confirmed.
- **Status:** IMPLEMENTED, self-tested.

## M7 · Visual motion architecture (Bubble Motion V3) — 2026-07-22

- **Problem:** Presentation motion needed without touching gameplay/Research timing.
- **Change:** `UTMVisualMotionComponent`; hierarchy `GameplayAnchor` (root) → `VisualMotion` → `MeshComponent`; gameplay reads the anchor only; inert-by-default + `ForceInert()` on the Research path (actor-tick-disable does not stop component ticks — the trap that motivated it); spawn ease-in 0.35 s from scale 0.6.
- **Status:** IMPLEMENTED; later AUTOMATION-VERIFIED via lifecycle/exit tests. (Session record 2026-07-22.)

## M8 · Research hard-gate contamination bug — found 2026-07-27, fixed same day

- **Problem (fact):** In Research mode, a gaze hard-gate resume spawned a self-scoring Demo bubble at the Research stimulus location; its outcome entered the SessionManager ledger with **no corresponding JSONL line** — silent ledger/JSONL divergence. Trigger conditions ordinary (3 s off-centre gaze, 3 s dwell back).
- **Diagnosis:** `OnHardGateResumed` → `HandleHardGateResumed` → `RestartTrial` → `SpawnNextTarget`, all unconditionally bound in both modes; no `OperatingMode` check anywhere on the chain.
- **Change (Pass 0A):** Research-mode early-refusal guards in `RestartTrial()` (`TargetSpawnerComponent.cpp:303-327`) and `SpawnNextTarget()` (`:544-552`), rate-limited suppression logging; guard comments and `TMSpawnContainmentTest.cpp` header preserve the bug narrative.
- **Verification:** `TranquilMind.Research.SpawnContainment.HardGateResume` drives a REAL gate trip+resume in Research → zero actors, ledger unchanged; positive control proves the same harness still spawns in Demo.
- **Status:** IMPLEMENTED + AUTOMATION-VERIFIED (tests in tree; post-July-20 archived run outstanding). This is the project's flagship "research containment + negative control" story.

## M9 · Demo player-relative session frame (Pass 0B) — 2026-07-27

- **Problem:** Spawn was camera-relative but travel was hard-coded world −X (`TravelDirection_World` written 3×, never read), so trial geometry depended on head yaw — end distance varied 0.75 m–3.25 m by facing.
- **Change:** `UTMDemoSessionFrameSubsystem` (WorldSubsystem; zero added actors because the pawn scans all actors on the input path); lazy latch on first Demo spawn; immutable basis; `Right = Up × Forward` verified; origin/orientation split (environment vs active-spawn); Research inertness two-layered.
- **Verification:** 6 SessionFrame automation tests incl. Research negative control. **Status:** IMPLEMENTED + AUTOMATION-VERIFIED; DEVICE-VERIFIED capture in Pass 0C.

## M10 · XR readiness classification (Pass 0B.1) — 2026-07-27

- **Problem:** `IsHeadTrackingAllowedForWorld` alone is not proof of a usable pose (headset-on-desk, early-boot cases).
- **Change:** Three-way `NoXR / Initializing / Tracked` classification; bounded 5 s pose wait (ticks only while waiting); flagged fallback (`bCaptureTimedOut`); fallback ladder HMD → pawn → world-axis.
- **Verification:** `Demo.SessionFrame.XRReadiness` + Basis/Orthonormal tests. On device: `Captured #1 | Source=HMDCamera | TimedOut=no` at T=2.01 s.
- **Status:** IMPLEMENTED + AUTOMATION-VERIFIED + DEVICE-VERIFIED.

## M11 · Pending-spawn exactly-once handshake (Pass 0B.2) — 2026-07-27

- **Problem:** A spawn attempt during XR-Initializing silently fell through to the legacy live-camera path (untracked pose); also risk of RNG-draw consumption by deferred attempts and reentrancy double-spawn.
- **Change:** Frame check moved BEFORE `ChooseNextStimulusType()`; one coalesced `bDemoSpawnPendingFrame`; consumed exactly once on `OnDemoFrameCaptured` (valid pose or timeout); `TGuardValue` reentrancy guard; Demo-only subscription; teardown-safe; test seam `PoseSampleOverrideForTests`.
- **Verification:** 5 PendingSpawn tests (valid-pose, timeout, teardown, Research-never-enters, post-capture cadence). Session record: 21/21 suite green 2026-07-27.
- **Status:** IMPLEMENTED + AUTOMATION-VERIFIED.

## M12 · BP_Target hierarchy verification — 2026-07-27

- **Problem:** An earlier audit claimed BP_Target carried a stray second StaticMeshComponent with an `MI_Target_Go` override.
- **Diagnosis/Result (fact):** Claim **wrong** — binary name-table inspection found no such nodes; BP_Target is data-only over the native GameplayAnchor→VisualMotion→Mesh hierarchy; `ResavePackages` produced a byte-identical file (MD5 unchanged); exactly one primitive component, duplicate sphere impossible.
- **Status:** VERIFIED (session record); no change needed.

## M13 · Environment Android cook — 2026-07-27/28

- **Problem:** `GameDefaultMap = L_TranquilMind_Void` and reachability-driven cook meant the Environment map/HDRI/bubble pipeline had **never been cooked or seen on device**.
- **Change:** `+MapsToCook` backstop for both maps in `DefaultGame.ini`; launch-map override via device-side `UECommandLine.txt` (path trap documented: one level deeper than the obvious location, wrong path silently boots Void); Gradle offline workaround for a blocked CDN.
- **Verification/Result:** Pass 0C APK (~146–153 MB) ran the Environment map on Quest 2. **Status:** DEVICE-VERIFIED.

## M14 · Quest 2 performance baseline (Pass 0C) — 2026-07-28

- **Result (fact, session record):** 72/72 FPS locked; Tear=0/Stale=0 in 224/242 samples; App GPU ≈6.9–7.7 ms of 13.9 ms at the lowest DVFS level; CPU ≈35–43%; 37.5→38.0 °C; ≈2.6 GB free. Research-mode spot check PASS on device.
- **Interpretation (labeled):** ample headroom for the planned ambient layer; adopted as the budget reference for Pass 2/3.
- **Caveat:** raw capture not archived in repo. **Status:** DEVICE-VERIFIED (session record); archive outstanding.

## M15 · Original bubble face-filling geometry → revisions — 2026-07-27/28

- **Problem (fact):** Original Demo geometry: spawn 200 cm, TEMP scale 1.0 (r=50 cm sphere) → 29° angular diameter at spawn, **84° at closest approach**, surface 25 cm from the eye; travel world-locked; described by the user as face-filling ("扑脸"); Pass 0C confirmed comfort-distance FAIL on device.
- **Diagnosis:** First size/distance; after Pass 1A still face-filling in PIE → **looming rate, not size** (user confirmed scale right). The scoping review's proxemics/looming analysis "fully accounts for" the experience (design interpretation grounded in cited literature).
- **Change:** Pass 1A `DemoMode_SpawnDistance_CM=350`, `DemoMode_TargetScale=0.45`; Pass 1A.1 `DemoMode_ApproachSpeed_CMPerSec=20`; verified numbers: travel 50 cm, end centre 300 cm, nearest surface 277.5 cm, angular diameter 7.37°→8.60°; per-spawn diagnostic readback.
- **Verification:** `Demo.SessionFrame.SpawnRegression` automation test; PIE observation.
- **User observation:** scale right; "motion too subtle" in PIE — tuning deferred.
- **Status:** IMPLEMENTED + AUTOMATION-VERIFIED; original geometry VISUALLY-REJECTED; revised geometry awaiting device re-check.

## M16 · Failed 350 ms visual exit (Pass 1A.2) — 2026-07-28

- **Change (fact):** `BeginVisualExit()` — 0.35 s transform envelope (scale ×0.94) + one-shot MID fade of CenterOpacity/EdgeOpacity/BubbleBrightness, deferred destroy; Environment-profile-gated; Research/Void keep immediate destroy. 2 VisualExit automation tests (no double-record, exit-safe gameplay, immediate destroy off-profile).
- **Result:** Functionally correct; **visually rejected**: too short and opacity-only (Spec C1), terminal flash (C5, cause UNRESOLVED — an earlier hardware explanation was retracted), continued apparent approach (C6), overlap risk (C7).
- **Decision:** No further exit tuning until Stage 1 instrumentation identifies the flash cause (Spec §11 blocking rule); planned replacement is the 400 ms compound exit (design, not built).
- **Status:** IMPLEMENTED + AUTOMATION-VERIFIED + VISUALLY-REJECTED.

## M17 · Literature and implementation-plan work — 2026-07-27/28 (external folder)

- **Change (fact):** Scoping Review Phase 1 (Domains A–C; 12 verified + ~15 consulted-unverified sources; self-labeled "not a complete literature review"), Complete Visual Timing Plan (supersedes Phase 1 recommendations; corrects 5 errors including an overlap-arithmetic error and a retracted flash diagnosis), Design Decision Table D1–D22, Implementation Spec (11 stages, all planned; stop conditions; claim-language rules), Evidence Matrix E01–E15, Source Register.
- **Key honesty markers (quotes):** "None of the specific numbers in the current build appear in any paper." / Domains E, G, H have 0 verified sources. / Red-color psychology is the "highest-risk unverified claim."
- **Status:** Documents complete as plans; **nothing in them is implemented** beyond what M5–M16 already evidence. One internal inconsistency noted: D20's DVFS stop rule is stricter than Spec §9.3.

## M18 · Stage 1 instrumentation readiness — 2026-07-28 (current state)

- **Fact:** `Stage1_Instrumentation_Kit.md` + `TMStage1Trace.h/.cpp` + `parse_tm_trace.py` exist in the external planning folder only. Kit header: "**Status: NOT EXECUTED.** … No cycles have been captured. No findings exist." Call-site prerequisites P1–P12 are blank. No trace hooks exist anywhere in `Source/` (verified by search).
- **What Stage 1 will do:** log-only tracer (SPAWN/MID_CREATE/PHASE/OUTCOME/EXIT_*/MAT_WRITE/VIS_FALSE/HIDDEN_TRUE/DESTROY_REQ/per-frame SNAP) to identify the exit-flash cause per five checks; then fixed-seed JSONL regression.
- **Status:** IN PROGRESS (kit authored; integration and capture not started). Per the Spec: no commit, no package, stop and report at stage end.
