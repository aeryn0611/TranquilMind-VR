# TranquilMind — Implementation Spec

> **Revision 2 — 2026-07-28 (post Stage 1, post pre-Stage-2 maintenance).**
> Pre-Part-3 verbatim copy archived at
> `Documentation/Spec_Archive/TranquilMind_Implementation_Spec_pre_Part3_20260728.md`
> (sha256 `696fc36d99a38e8f5cef3592bf0c0a638a56edd202634a220418e555d58a30a1`).
>
> **When this file conflicts with any earlier review, table, or plan, this Implementation Spec wins.**

**This is the only implementation authority.** It contains no literature review, no citations, and no competing candidate values except where an A/B test is explicitly named.

**Authority rule for Revision 2.** Stage 1 produced runtime evidence from 80 instrumented natural-expiry cycles (40 headless + 40 authoritative Environment PIE with a human observer). **Where that evidence contradicts an assumption in Revision 1 about the existing implementation, the evidence wins and this document has been corrected.** The Complete Plan, Decision Table and Literature Review remain reasoning documents, are *not* authority, and were not edited; residual conflicts are listed in `Documentation/PART3_CROSS_DOCUMENT_CONFLICT_REGISTER.md`.

**Target:** TranquilMind Demo Presentation Pipeline. Unreal Engine 5.7, Meta Quest 2 (72 Hz) baseline, desktop Environment PIE for instrumentation.

**Do not commit. Do not package. Do not build a distributable. Do not tag a release.** Stop at the end of each stage and report. See §12.

---

## 1. Protected Research invariants

The Demo may change its own presentation timing and cadence. It may **not** change any of the following.

| # | Protected | Meaning |
|---|---|---|
| **R1** | Research trial logic | Trial construction, sequencing, Go/NoGo assignment, block structure |
| **R2** | Research response timing | Research response-window constants and the clock they are measured against |
| **R3** | Research scoring | Hit / miss / correct-inhibit / false-alarm determination and any derived measure |
| **R4** | Research JSONL | Schema, field names, field order, value formats, timestamps, file naming |
| **R5** | Research reproducibility | Seeded determinism — identical seed must yield identical **canonical** output (§1.1) |
| **R6** | Research physiology integration | Slow-physiology gating logic, sampling, and its coupling to trial state |
| **R7** | Research random streams | No Demo presentation code may draw from, advance, or reseed a Research stream |

### 1.1 Regression gate — runs at the end of EVERY stage

**Revision 2 change.** Revision 1 required the raw JSONL to be *byte-identical*. That requirement is **unsatisfiable and was never satisfiable**, before or after any change: the schema carries a fresh session GUID, wall-clock timestamps and monotonic process-uptime values. This was measured empirically by capturing **two independent pre-change runs** and diffing them field by field — they already differed. A gate that can never pass is not a gate.

The requirement is replaced by a **two-layer gate**. Both layers must pass.

#### Layer A — raw-artifact integrity

```
A1. Every raw JSONL is preserved unchanged. A raw file is never rewritten,
    normalised, regenerated or "fixed" to make it deterministic.
A2. Schema version, field names, field order, field types and trial count are
    checked against the baseline.
A3. The SHA-256 of every raw file is recorded.
```

Failing A1–A3 is a Research failure. **A difference confined to registered nondeterministic metadata (§1.2) is NOT by itself a Research failure** — see §12 S1.

#### Layer B — canonical deterministic regression

```
B1. Apply the versioned canonical-projection tool to baseline and candidate.
B2. The tool excludes ONLY fields listed in the versioned
    nondeterministic-field register (§1.2). No ad-hoc exclusions.
B3. The canonical projections must be BYTE-IDENTICAL.
B4. Archive, for every gate run:
      - raw inputs
      - canonical outputs
      - projection tool version + hash
      - the excluded-field register version
      - SHA-256 of every artifact
      - the diff result
```

**The gate itself must be validated before it is trusted.** Project two independent *pre-change* runs; if they do not project identical, the projection is wrong — not the code. This validation is mandatory whenever the register or the tool changes.

Current tool: `Documentation/Stage1/baseline/canon_jsonl.py`.
Current reference (seed 20260708, 50 trials, no input):
`6ae4fd8c2e630806b089615c612b03d1cf41b013c5b566da9393960b21ac97eb`.

#### 1.2 Nondeterministic-field register (versioned)

**Register v1 — 2026-07-28.** Established by measurement across two pre-change runs, not by assumption. Every entry states *why* it varies.

| Field | Reason it is nondeterministic |
|---|---|
| `sessionId` | fresh GUID per run |
| `startedUtc`, `endedUtc` | wall clock |
| `scheduledStimulusOnsetSec`, `scheduledStimulusOffsetSec`, `responseWindowDeadlineSec` | absolute monotonic uptime; and the runner schedules trial *n+1* from the **realized** completion of trial *n*, so frame quantisation accumulates down a block |
| `realizedStimulusOnsetSec`, `realizedStimulusOffsetSec`, `behavioralResolutionTimestampSec`, `trialCompleteTimestampSec`, `responseTimestampSec` | frame-quantised measurement |
| `realizedItiMs` | frame-quantised measurement (±0.1 ms observed) |

**Protected deterministic content — never excludable:**

```
trial construction · Go/NoGo sequence · seeded random draws · seqHash ·
scheduled ITI (scheduledItiMs) · response window (responseWindowMs) ·
scoring outcome · rtMs · hadResponse · validity and void reason ·
block structure · physiology-gating decisions where deterministic ·
every research-semantic field · all footer aggregates
```

GUIDs, wall-clock timestamps, process uptime and measured frame quantisation may be excluded **only** when explicitly registered above. Adding an exclusion requires review, a register version bump, and re-validation of the gate.

This gate applies to every stage without exception, **including stages that appear purely visual.** Capture the baseline before a stage begins and keep it for the duration of the work.

---

## 2. Current code baseline — this is a brownfield refactor

**Revision 2: rewritten from Stage 1 runtime evidence.** Revision 1's entries were pre-instrumentation assumptions; several were wrong. Evidence: `Documentation/Stage1/STAGE1_REPORT.md`, 80 instrumented cycles.

| # | Component | Evidence-based status |
|---|---|---|
| **C1** | **350 ms visual exit** | **Implemented. Visually abrupt** (observer). Drives **opacity *plus* relative scale (1.0 → 0.94)** — *not* opacity alone, as Revision 1 stated. Final material write, completion delegate, Destroy request and EndPlay **can and do occur on the same frame** (separation `{0}` frames in every measured cycle). **No Hidden frame exists.** No `SetVisibility(false)` and no `SetHiddenInGame(true)` anywhere on the Demo exit path |
| **C2** | **MID-based fade** | Creates a **fresh MID at exit start on each fresh actor**. **No object pool exists.** **No concurrent cross-actor MID sharing was proven** — the one repeated `miduid` observed was `UE_ID_RECYCLED` (non-overlapping lifetimes). Terminal flash **NOT reproduced** in the authoritative instrumented PIE build. **C2 remains NOT YET PROVEN as a visual-defect cause.** Revision 1's "prime flash suspect — a MID replaced or re-acquired on pool reuse" is **withdrawn**: there is no pool, therefore no reuse path |
| **C3** | **VisualMotion exit state** | **Actor-level approach translation is frozen at exit start** — the actor transform is bit-identical across every exit frame, because `FinishResolution()` disables the actor tick before the exit begins. Only small relative drift (≈0.2 cm) and the scale settle remain. **Revision 1's claim that actor approach continues during exit is WITHDRAWN.** Disposition: **NOT YET PROVEN / reassess during lifecycle work** |
| **C4** | **Progress and completion delegates** | **Verified functional.** One completion per cycle, each preceded by a monotone progress series terminating at exactly 1.0. **Retain** as likely lifecycle-transition mechanism |
| **C5** | **Terminal flash** | **NOT REPRODUCED in the current instrumented build.** Previously reported; did not reproduce under authoritative human PIE observation across 40 cycles. **No root cause is required or invented.** Same-frame teardown is recorded as **architectural fragility**, not as a proven flash cause |
| **C6** | Continued apparent approach during fade | **Not supported by evidence.** See C3. Whatever was perceived, it is not actor translation, not mesh-component translation, and not a growing scale — the scale *shrinks* |
| **C7** | **Visual overlap with the next bubble** | **CONFIRMED** by trace *and* human PIE. Caused by **presentation ownership being coupled to `ActiveTargets`**: the actor is deregistered at resolution but keeps rendering for the whole exit. `visiblepresentations` reached **2** while `activetargets` remained **1**. **This is a presentation-ownership defect, not a scoring-concurrency defect** |

**Stage 2 is a refactor of C1–C4, not a greenfield build.** Dispositions assigned by Stage 1:

| | Disposition |
|---|---|
| C1 | **REPLACE in Stage 3** — abrupt, and lacks Hidden / deferred destroy |
| C2 | **NOT YET PROVEN** |
| C3 | **NOT YET PROVEN / reassess in Stage 2** |
| C4 | **RETAIN** |

---

## 3. Clock semantics

| Name | Definition | Value |
|---|---|---|
| **PresentationSpawnTime** | Actor creation / start of Phase 2 Entrance | *t* = 0 |
| **DemoTrialOnsetTime** | **Start of Phase 4**, after Entrance and Readable Hold | configuration-dependent (§4.7) |
| **ReactionTime** | `accepted_input_time − DemoTrialOnsetTime` | 0 … scored-window length |
| **ScheduledPhase4End** | `DemoTrialOnsetTime + ScoredResponseDuration` | **fixed, independent of any response** |
| **Cadence** | `PresentationSpawnTime(n+1) − PresentationSpawnTime(n)` | configuration-dependent (§4.7) |

Rules:

- **Demo summary reaction time uses `DemoTrialOnsetTime` as zero.** Never PresentationSpawnTime.
- **Outcome logging occurs only from Phase 4 onward.** No outcome, partial outcome, or provisional state is written during Phases 2, 3, 6 or 7.
- **Research timestamps remain unchanged.** The Research Pipeline keeps its own clock, onset definition and timestamp fields (R2, R4).
- Demo RT and Research RT are **not interchangeable.** Do not compare or pool them.
- **`ScheduledPhase4End` is a schedule, not an event.** Phases 6 and 7 are timed from it, never from the moment a response arrived (§5).

---

## 4. Lifecycle state machine

### 4.1 Phase table

Durations come from the **active timing configuration** (§4.7). Structure is configuration-independent.

| # | Phase | Input | Scored | Visible | In ActiveTargets | Owns LiveDemoPresentation | Motion |
|---|---|---|---|---|---|---|---|
| 1 | Spawn | none | no | **no** | **no** | **YES (from creation)** | 0 |
| 2 | **Entrance** | **IGNORED** | no | yes (alpha 0→1) | **no** | yes | 0 |
| 3 | **Readable hold** | **IGNORED** | no | yes (alpha 1) | **no** | yes | 0 |
| 4 | **Approach + response** | **ACCEPTED** | **yes** | yes (alpha 1) | **YES** | yes | decelerating |
| 5 | Outcome lock | closed | commit | yes | **no** | yes | frozen |
| 6 | **Persistence** | **IGNORED** | no | yes (alpha 1) | **no** | yes | 0 |
| 7 | **Exit** | **IGNORED** | no | yes (alpha 1→0) | **no** | yes | recede only |
| 8 | **Hidden** | none | no | **no** (alpha hard-clamped 0, visibility off) | **no** | yes | 0 |
| 9 | Destroy / pool return | none | no | no | no | **released here, and only here** | cleared |
| — | **Clear gap** | — | — | **nothing visible** | — | none | — |

**Phase 8 (Hidden) must exist and must last at least one full frame.** It does not exist in the current code (§2 C1) and is introduced in Stage 3.

### 4.2 Input rule — explicit

Input during Entrance, Readable Hold, Persistence and Exit is **discarded at the point of receipt**: not buffered, not queued, not replayed into Phase 4, not scored, does not become a commission error.

Rationale: buffering pre-onset input would record a response against a target that was not yet answerable, producing reaction times negative relative to `DemoTrialOnsetTime`.

### 4.3 Target-array rule — explicit

The actor is registered in `ActiveTargets` **only during Phase 4.** Registered at Phase 4 entry, deregistered at Outcome lock.

Do not register-then-filter. Do not use an `bIsActive` flag on a permanently registered actor.

**`ActiveTargets` is a scoring-eligibility set, not a presentation set.** See §4.9.

### 4.4 Spawn suppression

```
if (PresentationSpawnTime + VisualLifetime > SessionEnd) → do not spawn
```

### 4.5 Spatial parameters

**Revision 2: corrected.** Revision 1 required spawn 3.50 m, settle 3.00 m, a 2500 ms scored window, peak ≤20 cm/s, and monotonic deceleration to ≈0. **Those five are not simultaneously satisfiable.** A 50 cm traverse in 2.5 s needs a *mean* of 20 cm/s; with a monotonically decelerating curve ending at ≈0 the peak must be well above 20 cm/s (≈40 cm/s for a linear ramp-down). Revision 1's own measured baseline shows the tension: at a *constant* 20 cm/s the bubble travels exactly 50 cm and ends at 3.00 m — but constant speed is not deceleration, and its terminal velocity is not ≈0.

**Do not raise peak speed to preserve the 3.00 m endpoint.** Comfort and calmness outrank a round number.

| Parameter | Requirement |
|---|---|
| **Spawn centre distance** | **3.50 m** (fixed) |
| **Peak approach speed** | **≤20 cm/s** (hard ceiling) |
| **Curve** | **monotonic deceleration**, not braked |
| **Velocity at `ScheduledPhase4End`** | **≈0** |
| **Settle endpoint** | **derived from the selected curve — an output, not an input** |
| **Initial nominal target** | **≈3.15–3.25 m** (working figure only) |
| **Hard floor, nearest surface — clamped constant** | **≥2.00 m** |
| Visual radius | 0.225 m (unscaled 0.50 m × scale 0.45) |

**The final settle endpoint is accepted only after Stage 5 motion-curve validation.** Until then ≈3.15–3.25 m is a planning figure, not a requirement.

**Do not claim that 3.00 m and ≤20 cm/s monotonic deceleration are simultaneously mandatory.** They are not.

Assert the 2.00 m nearest-surface floor in code as a clamped constant, not a convention.

### 4.6 Exit composition — Stage 3, 400 ms

| Channel | From → To | Curve | Completes at |
|---|---|---|---|
| Opacity | 1.0 → 0.0 | ease-out | 100% |
| Position | settle → +8–12 cm **away** (slight recede) | ease-out | 100% |
| Scale | 1.0 → 0.97 (subtle contraction) | ease-out | 100% |
| **Edge/rim analogue** width | current → 0 | ease-out | **70%** |
| **Edge/rim analogue** intensity | current → 0 | ease-out | **70%** |

The edge/rim analogue must reach zero before the body does. Then **hard alpha clamp → visibility off → at least one full Hidden frame → Destroy / pool return on a LATER frame.**

**Parameter-availability warning — do not assume these exist.** The shared master material currently exposes exactly: `CenterOpacity`, `EdgeOpacity`, `BubbleBrightness`, `FresnelExponent`, and vectors `EdgeColorA`, `EdgeColorB`. **There is no rim-width parameter and no rim-intensity parameter.** Stage 3 must **first** map the two rim channels onto real parameters, or explicitly add them to the master material, and record which was done. Do not write a spec-shaped call against a parameter that does not exist.

Also measured: **`CenterOpacity`'s base value is ≈0.000995** — the core is already transparent, so fading that channel is a no-op. The visible channels today are `EdgeOpacity` (base ≈0.577) and `BubbleBrightness` (base ≈2.19 Go / ≈2.29 NoGo).

### 4.7 Timing configurations

Exactly one configuration is active at a time.

| | **Stage 2 Compatibility** | **A-prime** | **B-prime** |
|---|---|---|---|
| Entrance | **0 ms** | 400 | 500 |
| Readable Hold | **0 ms** | 200 | 300 |
| **Scored Response** | **2500 ms** | 2500 | 3000 |
| Persistence | **0 ms** | 300 | 400 |
| Exit | **350 ms** | 400 | 500 |
| **Visual lifetime** | **2850 ms** | 3800 | 4700 |
| **Cadence** | **3000 ms** | 4200 | 5000 |
| **Clear gap** | **150 ms** | 400 | 300 |
| Used by | **Stage 2 only** | Stage 4 default | Stage 4 alternative |

#### Stage 2 Compatibility configuration — normative

**Purpose: Stage 2 changes lifecycle structure and ownership ONLY.** It must preserve the current effective visible timing as closely as possible, so any behavioural difference observed after Stage 2 is attributable to the restructuring and to nothing else.

Stage 2 **must not**:

- introduce A-prime timing (that is Stage 4, and Stage 4 only);
- introduce the 400 ms compound exit or the Hidden-frame timing change (that is Stage 3);
- change any material or material parameter;
- change approach motion or the motion curve (that is Stage 5).

Stage 2 **must** implement the 9-phase structure and the ownership split (§4.9) *at the compatibility timings above*. Entrance, Readable Hold and Persistence are structurally present with **zero duration** — the phases exist and are traversed, they simply have no dwell. This keeps the state machine honest while leaving observable timing untouched.

A-prime remains **Stage 4 only**. B-prime remains the Stage 4 named alternative; pass criterion neither "rushed" nor "monotonous".

### 4.8 Named A/B test — readable hold

Toggle Readable Hold to 0 ms and compare **at Stage 4**. If no perceptual difference, remove the phase. (Stage 2 compatibility already runs it at 0 ms; that is a structural convenience, not this experiment.)

### 4.9 Presentation ownership — normative

**Revision 2: new section.** This is the root of C7.

Two concepts, deliberately separate:

| | **`ActiveTargets`** | **`LiveDemoPresentation`** |
|---|---|---|
| Contains | only actors currently eligible for Phase-4 input and scoring | the single official active Demo bubble |
| Begins | Phase 4 entry | **presentation creation** (Phase 1) |
| Ends | Outcome lock | **only after Hidden completes AND Destroy / pool-return completes** (end of Phase 9) |
| Purpose | scoring eligibility | visual exclusivity |

**The spawn gate must not rely only on `ActiveTargets.Num()`.** That is precisely the measured defect: deregistration happens at resolution, the actor keeps rendering for the whole exit, and a gate reading registration rather than visibility admits a successor into a frame where the predecessor is still substantially visible.

`LiveDemoPresentation` **prevents a successor from becoming visibly active while the previous official presentation remains visible.**

Implementation may use a weak pointer, a lifecycle registry, or an equivalent structure. **The semantics are mandatory; the mechanism is not.**

#### 4.9.1 Exactly-once cleanup

`LiveDemoPresentation` must be released **exactly once** on every one of these paths:

```
natural expiry              early accepted response       hard-gate entry
hard-gate resume            session restart               session end
world teardown              pending-spawn cancellation    actor destroyed unexpectedly
failed spawn                map transition
```

**A stale `LiveDemoPresentation` reference must never permanently block spawning.** Release must be robust to an actor that died without running its normal path — a weak reference that has gone invalid counts as released. Assert this: a gate that can deadlock the Demo is worse than the overlap it prevents.

---

## 5. Early-response semantics — normative

**Revision 2: new section.** Defines exactly what happens when valid input is accepted before `ScheduledPhase4End`.

Required sequence, in order:

```
1. The first valid Phase-4 input is accepted.
2. Outcome and reaction time are committed IMMEDIATELY and EXACTLY ONCE.
3. Input acceptance closes immediately.
4. The actor is removed from ActiveTargets immediately.
5. All later input is discarded (§4.2) — not buffered, not queued, not scored.
6. The VISUAL PRESENTATION CONTINUES until ScheduledPhase4End.
7. Persistence and Exit begin from SCHEDULED presentation time,
   NOT from response time.
8. Cadence and visual lifetime are therefore INDEPENDENT of reaction speed.
```

**The visual lifetime of Go and NoGo must not vary as a function of user reaction time.** A fast responder and a slow responder must see identical presentation timing; only the scoring differs. Anything else leaks response speed into the visual rhythm, makes cadence a function of performance, and makes the clear gap unpredictable.

**Natural expiry follows the same scheduled visual timeline.** Natural expiry and early accepted response **converge on the same scheduled exit path** — they differ only in when scoring was committed (step 2) and in the recorded outcome.

Scoring is immediate; presentation is scheduled. Those two clocks are deliberately decoupled.

---

## 6. Material parameter ownership

One shared master material. Four instances: Go, NoGo, Selected, Ambient-lightweight.

| Group | Owner | Parameters | Update rule |
|---|---|---|---|
| **A — shared base legibility** | identical across all Active instances | outer-contour width, outer-contour colour (**neutral/achromatic**), outer-contour intensity, core opacity, Fresnel power, Fresnel intensity, target luminance | Set once at Entrance start. **Never per frame** |
| **B — Go/NoGo identity** | differs per instance | inner-rim colour, internal structure, internal motion direction, internal motion amplitude | Set once at Entrance start. **Never per frame** |
| **C — exit only** | exit driver | the exit-controlled scalars (§4.6) | Written only during Phase 7 |
| **D — forbidden per-frame** | — | everything in Groups A and B | Per-frame writes are prohibited |

**Primary legibility cue is a view-independent outer contour, not Fresnel.** Fresnel is a secondary form cue and must never be the sole contour. Do not solve far-boundary legibility by increasing core opacity or by increasing target scale.

**Current-parameter reality (measured).** The master material exposes `CenterOpacity`, `EdgeOpacity`, `BubbleBrightness`, `FresnelExponent`, `EdgeColorA`, `EdgeColorB`. The Group A/B/C names above are **role names, not existing parameter names.** Stage 6 owns the mapping and any additions.

---

## 7. Go / NoGo identities

### 7.1 Redundant cue stack — priority order

| Priority | Cue | Go | NoGo |
|---|---|---|---|
| **1** | Internal motion direction | **outward** / opening | **inward** / settling |
| **2** | Internal structure | dispersed interior | present diffuse central core |
| **3** | Hue (never load-bearing) | cool cyan inner rim | pale lavender / opal inner rim |

**Do not use red.** Silhouette, outer contour and apparent size are **identical** between Go and NoGo.

**Current status (observer, Stage 1): Go / NoGo discrimination is poor, and the bubble boundary is hard to see at distance.** Recorded as Stage 6/7 inputs, not Stage 1 findings.

### 7.2 Quiet Core — single-mesh, in-material, mandatory default

Implement the core **inside the shared master material** using local-position-driven radial masking. **Do NOT add a second translucent sphere mesh by default.**

A second core mesh may be evaluated later **only if all three hold:**

1. Single-layer discrimination has **failed** the §7.3 test
2. Measured Quest GPU and overdraw headroom **allows** it
3. Stereo consistency is **confirmed on device** — not verified in PIE

All three, not any one. If discrimination fails but headroom does not allow a second mesh, strengthen **internal motion** (priority 1); do not add geometry.

### 7.3 Discrimination acceptance test

| Condition | Pass |
|---|---|
| Full colour, 3.5 m | ≥95% correct |
| **Full-frame greyscale** | **≥95% correct** |
| **CVD simulation filter** | **≥95% correct** |

Greyscale failure means cues 1 and 2 are not working and the design is still colour-dependent.

### 7.4 Salience matching

Go and NoGo matched within **5%** on rendered-frame mean and peak luminance, apparent size, contrast against background, and onset salience. Measure against three HDRI regions.

### 7.5 Motion amplitude and period

**Tune on device, not in PIE.** Amplitude must resolve at 3.5 m; period slow enough that it does not read as pulsing.

---

## 8. Ambient budget

### 8.1 Performance-test starting point (measurement baseline, NOT the design)

Far ambient **0** · Mid ambient **0** · Cloud wisps **0**.

### 8.2 Intended production target (after measured approval at each step)

| Layer | Distance | Target | Material | Condition |
|---|---|---|---|---|
| **Far ambient** | 8–15 m | **up to 3** | unlit **opaque or masked** billboard | GPU capture within tier after each object |
| **Mid ambient** | 4–7 m | **up to 2** | unlit lightweight translucent, single-layer | GPU capture within tier after each object |
| **Cloud wisps** | 6–12 m | **0 by default** | unlit translucent card | **up to 2, only if ≥3 ms headroom remains after Mid** |

**An empty ambient field is not the final visual design.** Add **one object at a time** with a GPU capture between each. Far ambient before Mid.

### 8.3 Ambient invariants

- No ambient actor is ever registered in `ActiveTargets`
- No ambient actor ever owns `LiveDemoPresentation`
- No ambient actor accepts input
- No ambient actor draws from a Research random stream
- Ambient actors are pooled; pooled state must be fully reset on reuse

---

## 9. Staging rules — optional experiment only

**Default: 1 visible approaching bubble, 100%. Staging is not enabled by default.**

If the Stage 10 experiment runs, all of the following are mandatory:

| Rule | Requirement |
|---|---|
| Actor type | **Separate class.** Not an Active bubble with a flag |
| `ActiveTargets` | **Never registered** |
| `LiveDemoPresentation` | **Never owns it** |
| Scoring | No code path from a staging actor reaches scoring |
| Role identity | **None.** Neutral colour. No Go/NoGo leakage about the next trial |
| Opacity | ≤40% of active bubble |
| Depth band | 4–6 m — separate from the active band, never overlapping |
| Lane angle | ±25–40° azimuth from the active lane |
| Vertical offset | ±10–20° elevation from the active lane |
| Stagger delay | 800–1500 ms offset from active spawn; onsets never coincide |
| Motion signature | **Lateral drift, not approach** |
| Exit path | **Sideways / outward.** Never toward the viewer |
| Determinism | Own presentation seed. **Must not draw from a Research random stream** |

**Two role-coded Active bubbles must never be simultaneously visible at full opacity.** If overlap is ever evaluated, it uses neutral Staging or Ambient actors only.

Experiment: Condition 0 (1 bubble) vs Condition 1 (1: 70% / 2: 25% / 3: 5%). Escalate only if Condition 1 shows no cost.

---

## 10. Performance gates

### 10.1 Tiers

| | Baseline | Production target | Ceiling |
|---|---|---|---|
| GPU time ceiling | **7.7 ms** | **9.5 ms** | **11.0 ms** |
| Translucent objects on screen | 1 | 3 | 5 |
| Added draw calls vs today | 0 | ≤5 | ≤7 |
| Total draw calls per view | ≤700 | ≤700 | ≤700 |
| Total triangles per view | ≤500k | ≤500k | ≤500k |
| Renderer | forward | forward | forward |
| MSAA | 4× | 4× | 4× |

### 10.2 Blocking criteria — a stage fails if any fails

| # | Criterion | Measurement |
|---|---|---|
| **B1** | Sustained 72 FPS across a full session | frame-time trace, not a spot reading |
| **B2** | No meaningful compositor misses | stale-frame / app-drop count |
| **B3** | GPU time within the selected tier ceiling | GPU capture, sustained not peak |
| **B4** | No thermal degradation across the full session | clock/throttle trace, start to end |

### 10.3 DVFS

**A DVFS increase is a warning requiring investigation, not an automatic failure.** Report the DVFS level alongside B1–B4. If B1–B4 all pass with elevated DVFS, **the stage passes** — but record it, because elevated DVFS reduces margin for later stages and raises thermal risk over longer sessions.

### 10.4 Prohibited compensations

Do not reduce render resolution or MSAA to meet a gate. Those are the platform's recommended settings; trading them away hides the regression.

---

## 11. Stages and acceptance tests

Every stage: **one variable only**, then the §1.1 two-layer regression gate.

| Stage | Change | Acceptance test |
|---|---|---|
| **1** | **Flash instrumentation.** No visual change. | **COMPLETE** — see §12.1 |
| **2** | **Lifecycle + ownership refactor** of C1–C4 into the 9-phase machine, at **Stage 2 Compatibility timing** (§4.7). Introduces `LiveDemoPresentation` (§4.9) and early-response semantics (§5). | §11.1 — blocking |
| **3** | **Compound exit** (§4.6) + Hidden frame + deferred destroy. | 72 fps capture, final 15 frames stepped: no alpha rebound, no edge/rim visible after alpha < 0.1, **≥1 full Hidden frame**, no destroy on the same frame as the last alpha write, no depth-sort pop |
| **4** | **A-prime timing** (§4.7). Then A/B vs B-prime. | Observer A/B, 3–5 people, counterbalanced: neither "rushed" nor "monotonous" |
| **5** | **Decelerating approach** (§4.5). | Observer A/B: reads as "arrived", not "stopped". **Velocity provably ≈0 at `ScheduledPhase4End`. Settle endpoint recorded and accepted here.** Peak ≤20 cm/s. Nearest surface ≥2.00 m |
| **6** | **Base material legibility** — view-independent outer contour (Group A). | Target identified within 500 ms at 3.5 m, against 3 HDRI regions, at 3 head orientations |
| **7** | **Go/NoGo identity** — Quiet Core single-mesh (Group B). | §7.3 all three conditions ≥95%; §7.4 luminance within 5% |
| **8** | **Ambient field** — Far first, then Mid, one object at a time. | B1–B4 pass after each object. Comfort no worse than baseline |
| **9** | **Cloud wisps — optional.** Only if Stage 8 leaves ≥3 ms headroom. | B1–B4 pass. Abandon if over tier |
| **10** | **Staging — optional experiment** (§9). | Condition 1 shows no cost to target identification. Explicit keep-or-discard decision recorded |
| **11** | **Final regression.** No new change. | Full two-layer gate + B1–B4 over a full session on device |

**Ordering rules:**

- **1 before 3** — satisfied; fixing an undiagnosed bug produces a fix you cannot trust
- **2 before 3** — ownership and lifecycle structure before exit *form*
- **2 before 4** — the timing complaint is largely an artefact of the collapsed lifecycle
- **3 before 4** — exit form before timing retune
- **6 before 7** — identity cues sit on top of base legibility
- **8 before 9** — wisps are the highest overdraw risk and lowest-confidence benefit
- **10 last, and optional** — staging is the only change that can degrade the task itself

### 11.1 Stage 2 acceptance — blocking

All of the following, validated with **schema-2 identity `(sid, spawnseq)`** (§13):

```
S2-A  max official live Demo presentations = 1     (LiveDemoPresentation)
S2-B  max ActiveTargets = 1
S2-C  no successor PresentationSpawnTime while the predecessor official
      presentation remains visibly active
S2-D  no role-coded Go/NoGo overlap
S2-E  early accepted response does NOT shorten visual lifetime
S2-F  natural expiry and early response converge on the same scheduled exit path
S2-G  no scored event fires outside Phase 4
S2-H  input in Phases 2/3/6/7 provably discarded
S2-I  ActiveTargets registration confined to Phase 4
S2-J  every C1–C4 component labelled retained / deleted / replaced / moved
```

**Do not use `auid` or `miduid` as globally unique identities in this validation.** They are diagnostic only (§13).

---

## 12. Stop conditions

Stop work and report immediately if any occurs:

| # | Condition |
|---|---|
| **S1** | **Canonical Research regression mismatch** (§1.1 Layer B) — blocking, always. **A raw-metadata difference confined to registered nondeterministic fields (§1.2) is NOT automatically a Research failure**; a raw difference outside that register, or any Layer-A integrity failure, IS |
| **S2** | *(retired as a Stage 1 gate — see §12.1)* |
| **S3** | Any B1–B4 criterion fails and cannot be met without reducing resolution or MSAA |
| **S4** | §7.3 greyscale or CVD discrimination cannot reach 95% with a single-mesh Quiet Core |
| **S5** | A change appears to require touching R1–R7 |
| **S6** | Any Demo presentation code path is found reading from or advancing a Research random stream |
| **S7** | The 2.00 m nearest-surface floor would need to be lowered |
| **S8** | Scope drifts beyond the current stage |
| **S9** | **Stage 2 fails if official presentation overlap remains** (S2-A / S2-C) |
| **S10** | **Stage 2 fails if an early accepted response changes visual lifetime** (S2-E) |
| **S11** | **Stage 2 fails if any new code touches R1–R7** |
| **S12** | **Instrumentation ambiguity must be reported, never guessed.** If identity, session scoping or lifetime cannot be established safely, report `LEGACY_LIMITATION` / `UE_ID_RECYCLED` / `OPEN / INCOMPLETE` and stop rather than asserting a conclusion |

### 12.1 Stage 1 acceptance and the retirement of S2

Revision 1 required Stage 1 to identify the flash cause, and made failure to do so (S2) a blocking stop condition. **The flash did not reproduce.**

**Stage 1 acceptance is therefore satisfied by either:**

- **(a)** the cause of a reproducible visual defect is identified and written down; **or**
- **(b)** the previously reported visual defect is **not reproduced under authoritative instrumentation**, *provided that* the result and its limits are explicitly recorded — specifically: what was instrumented, how many cycles under what conditions, what was observed, what was refuted, and what remains unknown.

Stage 1 completed under **(b)**. Recorded: C5 **NOT REPRODUCED IN THE CURRENT INSTRUMENTED BUILD**; the MID-reuse hypothesis **refuted by evidence**; same-frame teardown **confirmed as architectural fragility**; C7 **confirmed**; the cause of the original report **unknown and not invented**.

**Do not reintroduce "flash cause must be found" as a gate while the flash is not reproducible.** A gate that cannot be satisfied blocks all downstream work and invites a fabricated root cause.

**If a flash reappears, reopen it as a NEW evidence-backed defect** using schema-2 instrumentation (§13), with the archived Stage 1 captures as the diff baseline.

**Never commit, push, package, build a distributable, or tag a release without explicit approval.** Report at the end of each stage and wait.

---

## 13. Instrumentation contract — schema 2 (normative)

Every new trace line **must** carry:

```
schema=2 · sid · spawnseq
```

### 13.1 Identity

```
CANONICAL ACTOR IDENTITY  =  (sid, spawnseq)
```

| Field | Rule |
|---|---|
| `sid` | Unique per world/session; stable for that world's whole lifetime; **never reused within one process** |
| `spawnseq` | Monotonic within `sid`; assigned **exactly once** at presentation creation; **never derived from a UObject UniqueID**; not reset until the session ends |
| `auid`, `miduid`, actor name | **DIAGNOSTIC ONLY** |

Neither counter may come from a random stream (protects R5, R7).

### 13.2 Rules

- **`sid` is unique within one process, not globally.** Logs from different processes **must not be merged solely by `sid`**. Cross-process aggregation requires an outer run identifier, source-file identity, or file hash.
- Each actor's identity is assigned once and remains stable through `EndPlay`.
- **Predecessor events cannot inherit successor identity.** Overlap must never relabel.
- Repeated `auid`/`miduid` across **non-overlapping** lifetimes is **`UE_ID_RECYCLED`**.
- **Shared MID is reported only when lifetimes overlap.**
- **Pooling is never inferred from a recycled UniqueID.** It requires positive evidence: an explicit pool-return event, or a schema-2 identity respawning without `EndPlay`.
- The parser **must fail closed** when multiple sessions exist and none is selected.
- Schema-1 evidence is labelled **`LEGACY_INFERRED`**, and unrecoverable conclusions **`LEGACY_LIMITATION`**.

### 13.3 Session boundaries

- **`SESSION_BEGIN` is mandatory for schema 2.** It must carry enough context to distinguish Environment PIE, Void PIE, headless `-game`, and Demo vs Research.
- **`SESSION_END` is expected only on graceful teardown.** It records the teardown reason when available.
- A **missing `SESSION_END` marks the session `OPEN / INCOMPLETE`.**
- **A missing `SESSION_END` alone is not proof of a runtime defect** — a hard process kill closes the log before `EndPlay` runs.
- **Completed actor lifetimes inside an incomplete session remain usable** when explicitly scoped.

### 13.4 Tooling

| Artifact | Location |
|---|---|
| Hardened parser | `Documentation/Stage1/tools/parse_tm_trace_v2.py` |
| Fixtures (A–J) | `Documentation/Stage1/tools/tests/test_parse_v2.py` |
| Tracer source diff | `Documentation/Stage1/tools/TRACER_DIFF.md` |
| Original kit (frozen evidence) | `Documentation/Stage1/` — **never modify** |

Instrumentation stays in place across Stages 2 and 3.

---

## 14. Files and systems likely to be touched

Indicative, to be confirmed by reading the project. Not a licence to modify anything not listed.

| Area | Expected work |
|---|---|
| Active bubble actor / component | 9-phase state machine; confine `ActiveTargets` registration to Phase 4 |
| **New** Demo-only lifecycle component | Owns Phases 2, 3, 6, 7, 8. No access to R1–R7 |
| **New** `LiveDemoPresentation` ownership | §4.9, incl. exactly-once cleanup on all 11 paths |
| C1 exit implementation (350 ms) | **Replaced at Stage 3** |
| C2 MID fade | **Not yet proven** — do not replace speculatively |
| C3 VisualMotion exit state | Reassess at Stage 2; the approach freeze is already correct |
| C4 progress/completion delegates | **Retained** as the phase-transition mechanism |
| Object pool / spawner | Spawn gate on `LiveDemoPresentation`, not `ActiveTargets.Num()`; spawn suppression (§4.4) |
| Shared master material + instances | Group A/B/C split; view-independent outer contour; in-material Quiet Core; **rim parameter mapping or addition (§4.6)** |
| Demo session controller | Lead-in, cadence, spawn suppression, session end |
| Demo summary UI | RT computed from `DemoTrialOnsetTime`; language per §15 |
| Ambient actors (Stage 8+) | New, pooled, never in `ActiveTargets`, never own `LiveDemoPresentation` |
| Staging actors (Stage 10, optional) | New separate class, own seed |
| **Research pipeline** | **DO NOT MODIFY** |

---

## 15. Demo copy constraints

**Permitted:** calming · low-arousal · restorative · gentle · supportive

**Prohibited** in Demo UI, summary screen, and any accompanying material: treatment · therapy · therapeutic efficacy · clinical benefit · medically validated · reduces anxiety · improves attention · measures attention · assesses vigilance · sustained attention test

A 120-second demo cannot demonstrate sustained attention or vigilance. Do not claim it does.
