# TranquilMind — Project Fact Sheet (Verified Evidence Master)

Audit date: 2026-07-28. Documentation-only artifact; no code, assets, or config were modified to produce it.

**Authority rules applied:** `TranquilMind_Implementation_Spec.md` (external planning folder) is the sole implementation authority for future work. The repository, git history, saved logs, and verification artifacts are the sole authority for what has actually been implemented and validated. Nothing below is inferred from a design document alone; where a claim rests only on a session record (memory) without an archived artifact, that is stated.

**Status vocabulary:** IMPLEMENTED · AUTOMATION-VERIFIED · DEVICE-VERIFIED · VISUALLY-REJECTED · IN PROGRESS · PLANNED.

---

## 1. Project definition

TranquilMind is a production-quality Unreal Engine 5.7 / Meta Quest VR application that pairs a deterministic, reproducible Go/NoGo attention-task **Research pipeline** (seeded sequences, JSONL trial logging, live GSR physiological context) with a strictly separated, calm, presentation-grade **Demo pipeline** for portfolio demonstration.

Public-safe one-liner: *"A VR research platform for attention-task experiments on Meta Quest, with a physiologically-instrumented calm demo mode — engineered so the presentation layer can evolve without ever touching research reproducibility."*

Claim boundary (per Spec §14 and `Docs/Research/GoNoGo_Training_Evidence_Review.md` §9): the project is **not** a clinical treatment, therapy, or diagnostic instrument; a 120-second demo cannot demonstrate sustained attention or vigilance. Public copy must use calm/low-arousal language only.

## 2. Platform and device stack

| Layer | Fact | Evidence |
|---|---|---|
| Engine | Unreal Engine 5.7, single C++ runtime module, OpenXR plugin | `TranquilMind.uproject`; `Source/TranquilMind/TranquilMind.Build.cs` |
| Target device | Meta Quest 2 (Qualcomm SM8250 "KONA", Android 14 / Horizon OS, Vulkan 1.1.0), 72 Hz | `Docs/Quest_Working_Baseline_Report.md` §2; Pass 0C session record 2026-07-28 |
| Rendering | Mobile forward renderer, `vr.MobileMultiView=1`, mobile Lumen/RT/Substrate disabled, arm64 only | `Config/Android/AndroidEngine.ini`; `Config/DefaultEngine.ini` |
| Package | `com.tranquilmind.vr`, MinSDK 29 / TargetSDK 35, data-inside-APK | `Config/DefaultEngine.ini` |
| VR entry | `bStartInVR=True`, `bPackageForMetaQuest=True`, APL manifest injection | `Config/DefaultGame.ini:7`; `Source/TranquilMind/TranquilMind_APL.xml`; Build.cs |
| Physiology hardware | ESP32 NodeMCU (ESP32-D0WD-V3) + Grove GSR sensor, GPIO34, 12-bit ADC, Wi-Fi UDP JSON at ~1 Hz | `Docs/Portfolio/TranquilMind_Quest_GSR_Demo_Milestone.md`; firmware sketches in `~/Documents/Arduino/ESP32_GSR_UDP` (outside repo) |
| Dev host | macOS, UE 5.7 at `/Users/Shared/Epic Games/UE_5.7` (private path — redact in public material) | Tools/Verification/README.md |

## 3. Project goals

1. A reproducible Go/NoGo research task platform (deterministic sequences, honest logging, protocol per `Docs/Research/TranquilMind_Protocol_Specification_v1.md` — DRAFT, pilot defaults).
2. A calm, premium presentation demo (aesthetic: minimal, "alive without looking animated"; no arcade FX).
3. Absolute separation: every visual improvement must leave Research Mode behavior byte-for-byte unchanged (Spec §1.1 regression gate).
4. Physiological context stream (GSR) integrated end-to-end, explicitly labeled contextual, **not** synchronized scientific EDA.
5. Portfolio/public case-study material with defensible, evidence-bounded claims.

## 4. Research Pipeline architecture — Status: IMPLEMENTED + AUTOMATION-VERIFIED (working tree, uncommitted)

Classes (all currently **untracked in git** — see §15):

- `UTMResearchRunner` (`Source/TranquilMind/Public/TMResearchRunner.h`) — UObject trial state machine (Idle / InterTrialInterval / TrialActive / BlockComplete), driven only by `UTargetSpawnerComponent` when `OperatingMode == Research`. Monotonic `FPlatformTime::Seconds()` clock; fixed 400 ms stimulus exposure never shortened by a response; response window 1200 ms from onset; duplicate/late responses ignored-but-counted; stimulus location computed once per run (camera + 200 cm).
- `FTMSequenceGenerator` (`TMSequenceGenerator.cpp`) — self-contained SplitMix64 RNG, seeded backtracking; exact NOGO count (50 trials → 40 GO / 10 NOGO), max GO run 6, max NOGO run 1, no NOGO first, 3 segments with per-segment quotas differing ≤1; infeasible configs fail loudly; FNV-1a 64 sequence hash.
- `FTMTrialLogger` (`TMTrialLogger.cpp`) — JSONL at `Saved/TranquilMind/Research/<SessionID>.jsonl`; schemaVersion 2; `session_header` / `trial` / `block_summary` / `session_footer`; per-trial seed + seqHash; logger health latches unhealthy on any write failure; interrupted runs finalized exactly once (active trial → single Void record, footer once).
- `UTMResearchSettings` / `FTMResearchConfig` — DeveloperSettings (config=Game); `OperatingMode` (default Demo) overridable by CVar `tranquilmind.OperatingMode` (-1/0/1); TrainingSeed 20260708, EvaluationSeed 19850611; `bEnableResponseWindowAdaptation=false` (no adaptation active).
- GSR context: `UTranquilMindPhysiologyReceiver` (UDP :4210) sample embedded per trial as `gsr{raw, smoothed, quality, deviceTsMs}` with the in-file note "1Hz latest sample, contextual only, NOT synchronized EDA".

**Determinism fingerprint (exact):** seed `20260708` → sequence `.....N......N.N...N.N......N......N...N..N.N......`, hash `4922870221080512783`, segment starts [0,17,34], segment NOGO [3,3,4]. Locked by the `DefaultContract` automation test and hard-coded into the fail-closed inspector `Tools/Verification/phase1_inspect_jsonl.py`.

## 5. Demo Presentation Pipeline architecture — Status: IMPLEMENTED + AUTOMATION-VERIFIED; geometry DEVICE-TESTED with two visual FAILs (§10, §12)

- `UTMDemoSessionFrameSubsystem` (`TMDemoSessionFrame.h`, TickableWorldSubsystem) — player-relative session frame. Immutable orientation basis latched from the first valid head pose (lazy, at first Demo spawn, never BeginPlay); XR readiness classified `NoXR / Initializing / Tracked`; bounded 5 s pose wait then flagged fallback; environment anchors use the immutable capture origin while active-bubble spawns use the *current* camera position with immutable orientation (leaning preserves comfort distance; targets never chase head rotation). Not created in Research (two layers: `ShouldCreateSubsystem` + capture refusal).
- `UTMVisualMotionComponent` (`TMVisualMotionComponent.h`) — generic presentation motion rig. Target hierarchy: `GameplayAnchor` (root, gameplay-owned) → `VisualMotion` → `MeshComponent`. Inert by default; `ForceInert()` on the Research path; idle float 1.0 cm / drift 0.5 cm / 9 s period / 0.5° rotation; spawn ease-in 0.35 s from scale 0.6; visual exit 0.35 s to scale ×0.94 with material fade.
- `UTargetSpawnerComponent` — ISI cadence, pending-spawn **exactly-once** handshake (one coalesced request recorded before any stimulus draw; consumed once on frame capture, valid-pose or timeout alike; reentrancy-guarded; teardown-safe).
- Material seam: map-profile gate (`L_TranquilMind_Environment` → `MI_Bubble_Go/NoGo`; Void/Research → `MI_Target_Go/NoGo`). Known intentional tech debt: hardcoded map-name check (two seams incl. `TranquilMindUISubsystem`).
- Target lifecycle: spawn → InitializeTarget → travel along the session-frame approach axis → resolve (Triggered/Void/Expired, single ledger record via `bResolved` gate) → visual exit (Environment profile only) → destroy.

## 6. Protected Research invariants (Spec §1, R1–R7) and their current enforcement

| Invariant | Enforcement in code | Status |
|---|---|---|
| Trial logic / timing / scoring untouched by presentation | Runner owns all timing; VisualMotion contract "gameplay never reads it" | IMPLEMENTED |
| JSONL reproducibility | Deterministic generator + logger; DefaultContract test | AUTOMATION-VERIFIED (archived run 2026-07-20, 8/8) |
| No Demo code in Research runs | 5 guard layers: `SpawnNextTarget` refusal, `RestartTrial` refusal, mode-gated tick, subsystem never created + capture refusal, `ForceInert` | AUTOMATION-VERIFIED (containment tests in tree; no archived run yet — see §11 gap) |
| No shared random streams | Sequence RNG self-contained; ITI stream seeded `ScheduleSeed ^ 0x17171717`; motion phase is a hash, not a stream draw | IMPLEMENTED |
| Byte-identical JSONL regression gate after every stage | Tooling exists (`phase1_inspect_jsonl.py`, fail-closed) | PLANNED as an executed byte-diff — **no recorded before/after byte-comparison artifact exists** |

## 7. Hardware and physiology integration — Status: DEVICE-VERIFIED (2026-07-05/06)

- Chain: Grove GSR electrode → ESP32 (EMA smoothing, 1 Hz JSON `{"src":"esp32_gsr","raw":…,"smoothed":…,"ts":…,"qual":…}`) → Wi-Fi UDP :4210 → `UTranquilMindPhysiologyReceiver` (WorldSubsystem, background-thread receiver, game-thread marshalling) → in-headset GSR status panel + demo summary panel.
- Verified: UDP bind on device; 57 packets / 57 s continuous; parsed log line `[Physiology] GSR raw=2368 smooth=2361 qual=97 ts=804445`; 120 s demo session ending with summary panel (`[DemoMode] Session ended | Elapsed=120.0 s`). Every hop verified independently (serial monitor, Mac UDP listener, Mac standalone, Quest logcat).
- Evidence: `Docs/Verification/GSR_Quest_Phase2_2026-07-05.md` (commit `789d32c`), `Docs/Portfolio/TranquilMind_Quest_GSR_Demo_Milestone.md` (commit `02d6e41`). ⚠ The Phase 2 doc contains Wi-Fi SSID and LAN IPs — **not public-safe as-is**.
- Explicit limits (recorded in protocol spec §6): 1 Hz latest-sample only; no buffering to scientific standard; 32 Hz EDA firmware is a PLANNED roadmap; GSR-driven difficulty and emotion inference are prohibited.

## 8. Current effective Demo parameters (working tree, exact)

| Parameter | Value | Source |
|---|---|---|
| Spawn distance | 350 cm (camera-relative, session-frame forward) | `TargetSpawnerComponent.h:110` |
| Target scale | 0.45 (engine sphere r=50 cm → visual radius 22.5 cm) | `TargetSpawnerComponent.h:118` |
| Approach speed | 20 cm/s (constant; travel 50 cm; end centre 300 cm; nearest surface ≈277.5 cm; angular diameter ≈7.4°→8.6°) | `TargetSpawnerComponent.h:120-132` |
| ISI | 3000 ms · Response window 2500 ms · Session 120 s | `TargetSpawnerComponent.h:83-93` |
| Stimulus mix | Hardware-debug strict alternation is the default path (`bDebugHardwareMode=true`); seeded Bernoulli (seed 5202, GO 0.80) exists as the alternative | `TargetSpawnerComponent.h:70,170`; `.cpp:25` |
| Idle motion | float 1.0 cm, drift 0.5 cm, period 9 s, rotation 0.5° | `TMVisualMotionComponent.h:48-64` |
| Spawn ease-in | 0.35 s from scale 0.6 | `TMVisualMotionComponent.h:69,80` |
| Visual exit | 0.35 s, scale ×0.94, fades CenterOpacity/EdgeOpacity/BubbleBrightness | `TMVisualMotionComponent.h:85,91`; `TranquilMindTargetActor.cpp:492-519` |
| Bubble materials | `M_Bubble_Master` (Translucent/Unlit/TwoSided, no SceneColor); Go vs NoGo differ by hue alone; CenterOpacity 0.001, EdgeOpacity 0.58, Fresnel 3.7 | 2026-07-27 material audit (session record); assets `Content/Environment/Materials/` |

Per-spawn `[DemoPresentation]` diagnostic logs read back actor-received values and predicted geometry (`TargetSpawnerComponent.cpp:747-788`) — receipt-proof instrumentation.

## 9. Quest performance baseline — Status: DEVICE-VERIFIED (session record 2026-07-28); raw capture NOT archived

Pass 0C, Quest 2, Environment map (HDRI dome + 1 translucent bubble + 3 text panels), full 120 s demo:

- **72/72 FPS locked**; Tear=0, Stale=0 in 224/242 samples (stales at load only).
- App GPU ≈6.9–7.7 ms of the 13.9 ms budget, ≈62% GPU utilization **at the lowest DVFS level (CPU4/GPU 2/2)**; CPU ≈35–43%; temperature 37.5→38.0 °C stable; ≈2.6 GB free memory.
- XR frame verification on device: `Captured #1 | Source=HMDCamera | TimedOut=no` at T=2.01 s; guardian-relative heading confirmed STAGE-origin arbitrariness.

⚠ Evidence caveat: these numbers exist in session records/memory; the raw logcat/VrApi capture is **not archived in the repository**. Back it up or re-capture (see Final Report). Spec performance tiers (7.7/9.5/11.0 ms, B1–B4 gates) are PLANNED acceptance criteria, not yet formally executed.

## 10. Completed engineering changes (chronological, with status)

| Change | Date | Evidence | Status |
|---|---|---|---|
| Initial VR task prototype: Void map, target spawning, trigger response, phase state machine | ≤2026-06-21 | commits `9d437a6`…`a478e6b`; `Docs/Quest_Working_Baseline_Report.md` | DEVICE-VERIFIED |
| Quest APK packaging fixes (Meta manifest/APL, `bStartInVR`, `-package` stage, MobileMultiView) | 2026-06-21 | `Config/Android/AndroidEngine.ini`, `TranquilMind_APL.xml`, Build.cs | DEVICE-VERIFIED |
| Enhanced-Input axis failure → `BindKey(OculusTouch_Right_Trigger_Click)` | 2026-06-21 | baseline report §root-cause | DEVICE-VERIFIED |
| GSR UDP physiology receiver + panels + demo summary | 2026-07-05/06 | commits `789d32c`, `b08a862`, `02d6e41`; verification docs | DEVICE-VERIFIED |
| Void visual polish + pastel scene pass | 2026-07-06 | commits `51dbcb5`, `4c60f0d` (HEAD) | IMPLEMENTED (visual, user-accepted at the time) |
| Research pipeline (runner/generator/logger/settings) + 8 automation tests | ≤2026-07-20 | untracked sources; archived run `Verification/Phase1/20260720_075848` | AUTOMATION-VERIFIED (8/8, archived) |
| Phase 1 verification toolkit (fail-closed scripts + inspector + 12 unit tests) | ≤2026-07-20 | `Tools/Verification/` | IMPLEMENTED (self-tested) |
| Research-mode Android package (APK sha256 `09fadb7a…3932b9`) | 2026-07-20 | `ArchivedBuilds/Phase1Verification/20260720_075848` | IMPLEMENTED (packaged; never device-captured) |
| Bubble Motion System V3 (GameplayAnchor→VisualMotion→Mesh; inert-by-default) | 2026-07-22 | `TMVisualMotionComponent.*`; session record | IMPLEMENTED + AUTOMATION-VERIFIED |
| Pass 0A contamination guards (`RestartTrial`/`SpawnNextTarget` Research refusal) | 2026-07-27 | `TargetSpawnerComponent.cpp:303-327, 544-552`; `TMSpawnContainmentTest.cpp` | IMPLEMENTED + AUTOMATION-VERIFIED (tests in tree; run not archived) |
| Pass 0B/0B.1 player-relative session frame + XR readiness classification | 2026-07-27 | `TMDemoSessionFrame.*`; 6 SessionFrame tests | IMPLEMENTED + AUTOMATION-VERIFIED; frame capture DEVICE-VERIFIED (Pass 0C log line) |
| Pass 0B.2 pending-spawn exactly-once handshake | 2026-07-27 | `TargetSpawnerComponent.cpp:587-622, 886-897`; 5 PendingSpawn tests | IMPLEMENTED + AUTOMATION-VERIFIED |
| BP_Target hierarchy preflight (byte-identical resave; no stray mesh; data-only BP) | 2026-07-27 | session record (`ResavePackages` MD5 unchanged) | VERIFIED (session record) |
| Environment map added to cook (`+MapsToCook` backstop) | 2026-07-27/28 | `Config/DefaultGame.ini:13-15` | DEVICE-VERIFIED (Environment ran on device in Pass 0C) |
| Pass 1A/1A.1 geometry revision (350 cm / 0.45 / 20 cm/s + diagnostics) | 2026-07-28 | `TargetSpawnerComponent.h:110-132`; SpawnRegression test | IMPLEMENTED + AUTOMATION-VERIFIED; on-device re-check pending |
| Pass 1A.2 post-resolve visual exit (0.35 s) | 2026-07-28 | `TMVisualMotionComponent.h:153-176`; 2 VisualExit tests | IMPLEMENTED + AUTOMATION-VERIFIED; **VISUALLY-REJECTED** as final design (Spec C1: "Too short, opacity-only"; terminal flash C5 unresolved) |

## 11. All automated validations

- **Archived, executed run (2026-07-20, `Verification/Phase1/20260720_075848/tests/automation_summary.json`):** 8/8 pass — `TranquilMind.Research.Sequence.{BoundaryStress, Constraints, DefaultContract, Determinism, ExactCounts, InfeasibleFailsLoudly, SegmentBalance}` + `TranquilMind.Research.Lifecycle.Shutdown`. Cross-checked against raw UE logs; both runs `### EXIT: 0`.
- **Current working tree defines 23 automation tests** (the 8 above + 6 SessionFrame + 5 PendingSpawn + 2 SpawnContainment + 2 VisualExit). Session records report "21/21" green on 2026-07-27 (before the VisualExit pair), but **no archived automation summary exists for the post-July-20 suite** — a re-run + archive is the top verification gap.
- **Toolkit self-tests:** 12 unittest methods in `Tools/Verification/tests/test_phase1_toolkit.py` (fail-closed parser/inspector, exact-byte config restore, APK/JSONL selector rejection, "unconfirmed human checks stay NOT VERIFIED").
- **Negative controls:** `Research.SpawnContainment.HardGateResume` (real gate trip in Research → zero spawns, ledger unchanged) with Demo positive control `HardGateResumeStillSpawns`; `Research.SessionFrame.NotCreatedOrCaptured`; `Research.PendingSpawn.NeverEnters`; `Sequence.InfeasibleFailsLoudly`; toolkit fake-JSONL fixtures explicitly labeled "FAKE test data… NOT real verification results".

## 12. All device validations

| Date | Validation | Result |
|---|---|---|
| 2026-06-21 | Quest 2 working baseline: 10-item chain (launch → render → spawn → trigger → resolve → Phase II) | PASS (with TEMP debug state) |
| 2026-07-05 | GSR UDP on Quest: bind, 57 s continuous ~1 Hz reception, parsed fields, no crash | PASS |
| 2026-07-05/06 | 120 s demo session with live GSR panel + auto summary panel | PASS |
| 2026-07-20 | (Phase 1 run) — **no device attached**; device capture, JSONL pull, hash never ran | NOT RUN |
| 2026-07-28 | Pass 0C on Quest 2: cook/install, XR frame capture, 72 Hz perf, Research-mode spot check | PASS |
| 2026-07-28 | Pass 0C visual acceptance: active-bubble comfort distance | **FAIL — VISUALLY-REJECTED** (face-filling, confirmed on device) |
| 2026-07-28 | Pass 0C visual acceptance: bubble boundary legibility vs HDRI | **FAIL — VISUALLY-REJECTED** (deferred to material pass) |

## 13. Known failures (open)

1. **Terminal flash at visual exit (Spec C5)** — reproducible in desktop Environment PIE; cause UNRESOLVED (an earlier LCD-hardware explanation was formally retracted). Blocking rule: no exit-timing tuning before Stage 1 instrumentation.
2. **350 ms visual exit inadequate (C1)** + continued apparent approach during exit (C3/C6) + visual overlap risk with next bubble (C7) — recorded as the failing baseline the Spec's compound-exit redesign targets.
3. **Face-filling looming** — even at 350 cm / 0.45 the approach *rate* reads as looming (diagnosis: looming rate, not size); `DemoMode_ApproachSpeed=20` was the response; device re-verification pending.
4. **`APP_CMD_LOST_FOCUS` / hard gate at ~14–15 s** (2026-06-21) — cause never identified; possibly stale; re-test on current build.
5. Demo alternation default (`bDebugHardwareMode=true`) makes next trial type fully predictable — acceptable for demo, must be relabeled demo-only (protocol spec) — plus three conflicting response-window constants across code paths (gap analysis §2.3).

## 14. Current technical debt

- Nested source paths `Source/TranquilMind/{Private,Public}/Private/Private/TranquilMindTargetActor.*` (cosmetic, pre-existing; rename touches include paths).
- TEMP debug blocks awaiting restoration: tie-break selection disabled (resolve-all-on-trigger), single-target cap, depth auto-void commented out, actor-path TEMP `SetActorScale3D(1.0)` (superseded on the Demo path by pushed values).
- Hardcoded map-name gates (2 seams: target actor + UISubsystem) → future visual-profile system.
- UI panels + `ATranquilMindVoidVisualLayer` are not mode-gated and run during Research; panels call `SetText` every tick.
- Hard-gate centre is world +X with STAGE tracking origin (guardian-dependent); `bBypassHardGateSuspend` forced true in Demo.
- 10 `RemoveAtSwap` deprecation warnings (`TargetSpawnerComponent.cpp:296-647`); duplicate `[AndroidRuntimeSettings]` header in `DefaultEngine.ini`; `MI_Bubble_Clear`/`MI_Bubble_Selected` authored but unreferenced; `M_Bubble_Master` TwoSided doubles overdraw for zero visible pixels; no `bUsedWith*` flags (ISM/Niagara would break on device).
- **~3 weeks of work uncommitted** (last commit 2026-07-06): entire Research pipeline, all tests, toolkit, Environment content, verification artifacts are untracked.
- Phase 1 run `20260720_075848` still open (`.current_run` pointer), no `FINAL_REPORT.md`, empty `jsonl/`, `demo/`, `demo_logs/`.

## 15. In-progress work

- **Stage 1 flash instrumentation** — kit authored externally (`TMStage1Trace.h/.cpp`, `parse_tm_trace.py`, capture protocol) but explicitly "**NOT EXECUTED**… No cycles captured. No findings exist." Not yet copied into the Unreal project; call-site prerequisites P1–P12 unfilled. Status: IN PROGRESS (readiness only).
- Demo geometry/motion tuning after Pass 1A (device re-check of 20 cm/s; PIE "motion too subtle" noted).
- Uncommitted working tree awaiting a checkpoint commit.

## 16. Planned but not implemented (per Spec — none of this exists in code)

- Nine-phase presentation lifecycle (Spawn/Entrance/Hold/Approach+Response/Lock/Persistence/Exit/Hidden/Destroy; A-prime 400/200/2500/300/400 ms; cadence 4200 ms; no-overlap).
- 400 ms compound exit (opacity + recede 8–12 cm + scale→0.97 + rim-zero at 70%).
- Decelerating approach (Model 2, velocity 0 at outcome lock).
- Go/NoGo identity redesign: motion-direction primary cue, **Quiet Core** single-mesh NoGo (in-material radial masking; no second mesh by default; ≥95% discrimination incl. greyscale + CVD; luminance within 5%).
- View-independent outer-contour legibility material (Fresnel demoted).
- Ambient layer (far 3 max / mid 2 max / wisps 0–2 only with ≥3 ms headroom) — empty field is the instrumentation baseline, not the design.
- Optional staging experiment (Stage 10), A/B cadence test (A-prime vs B-prime), performance gates B1–B4, Stages 1–11 ordering, 32 Hz EDA firmware, adolescent research protocol B/C (ethics, consent), MAX30102 HR/SpO₂.
