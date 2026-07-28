# TranquilMind — Implementation Spec

> **When this file conflicts with any earlier review, table, or plan, this Implementation Spec wins.**

**This is the only implementation authority.** It contains no literature review, no citations, and no competing candidate values except where an A/B test is explicitly named.

**Target:** TranquilMind Demo Presentation Pipeline. Unreal Engine 5, Meta Quest 2 (72 Hz) baseline, desktop Environment PIE for instrumentation.

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
| **R5** | Research reproducibility | Seeded determinism — identical seed must yield identical output |
| **R6** | Research physiology integration | Slow-physiology gating logic, sampling, and its coupling to trial state |
| **R7** | Research random streams | No Demo presentation code may draw from, advance, or reseed a Research stream |

### 1.1 Regression gate — runs at the end of EVERY stage

```
1. Run the Research Pipeline with a fixed seed.
2. Diff the produced JSONL against the pre-change baseline.
3. It MUST be byte-identical.
4. If it is not: revert the stage. Do not proceed. Report.
```

This gate applies to every stage without exception, **including stages that appear purely visual.** Capture the baseline JSONL before Stage 1 begins and keep it for the duration of the work.

---

## 2. Current code baseline — this is a brownfield refactor

The project already contains a partial, failing implementation. **Read it before changing it.**

| # | Already present | Observed |
|---|---|---|
| **C1** | 350 ms visual exit | Too short, opacity-only |
| **C2** | MID-based fade (dynamic material instance driving opacity) | **Prime flash suspect** — a MID replaced or re-acquired on pool reuse can render one frame at parent defaults |
| **C3** | VisualMotion exit state | Does not stop approach motion at exit → "still appears to approach while fading" |
| **C4** | Progress and completion delegates | Likely reusable as the phase-transition mechanism |
| **C5** | Terminal flash | Reproducible in **desktop Environment PIE**. Cause unresolved |
| **C6** | Continued apparent approach during fade | Explained by C3 |
| **C7** | Visual overlap with the next bubble | Explained by absence of lifetime/cadence separation |

**Stage 2 is a refactor of C1–C4, not a greenfield build.** After Stage 1, assign each of C1–C4 exactly one disposition and record it: **retained · deleted · replaced · moved into the new lifecycle component.**

Do not assign dispositions before Stage 1 completes. Rewriting C1–C4 first destroys the evidence needed to diagnose C5.

---

## 3. Clock semantics

| Name | Definition | Value |
|---|---|---|
| **PresentationSpawnTime** | Actor creation / start of Phase 2 Entrance | *t* = 0 |
| **DemoTrialOnsetTime** | **Start of Phase 4**, after Entrance and Readable Hold | *t* = 600 ms |
| **ReactionTime** | `accepted_input_time − DemoTrialOnsetTime` | 0–2500 ms |
| **Cadence** | `PresentationSpawnTime(n+1) − PresentationSpawnTime(n)` | 4200 ms |

Rules:

- **Demo summary reaction time uses `DemoTrialOnsetTime` as zero.** Never PresentationSpawnTime — that would inflate every RT by 600 ms.
- **Outcome logging occurs only from Phase 4 onward.** No outcome, partial outcome, or provisional state is written during Phases 2, 3, 6 or 7.
- **Research timestamps remain unchanged.** The Research Pipeline keeps its own clock, onset definition and timestamp fields (R2, R4).
- Demo RT and Research RT are **not interchangeable.** Do not compare or pool them.

---

## 4. Lifecycle state machine — A-prime defaults

### 4.1 Phase table

| # | Phase | Start | End | Duration | Input | Scored | Visible | In target array | Motion |
|---|---|---|---|---|---|---|---|---|---|
| 1 | Spawn | 0 | 0 | 0 | none | no | **no** | **no** | 0 |
| 2 | **Entrance** | 0 | 400 | **400** | **IGNORED** | no | yes (alpha 0→1) | **no** | 0 |
| 3 | **Readable hold** | 400 | 600 | **200** | **IGNORED** | no | yes (alpha 1) | **no** | 0 |
| 4 | **Approach + response** | 600 | 3100 | **2500** | **ACCEPTED** | **yes** | yes (alpha 1) | **YES** | decelerating |
| 5 | Outcome lock | 3100 | 3100 | 0 | closed | commit | yes | **no** | frozen |
| 6 | **Persistence** | 3100 | 3400 | **300** | **IGNORED** | no | yes (alpha 1) | **no** | 0 |
| 7 | **Exit** | 3400 | 3800 | **400** | **IGNORED** | no | yes (alpha 1→0) | **no** | recede only |
| 8 | Hidden | 3800 | +1 frame | 1 frame | none | no | **no** (alpha clamped 0) | **no** | 0 |
| 9 | Destroy / pool return | ≥1 frame later | — | — | none | no | no | no | cleared |
| — | **Clear gap** | 3800 | 4200 | **400** | — | — | **nothing visible** | — | — |

**Visual lifetime 3800 ms. Cadence 4200 ms. Clear gap 400 ms. NO OVERLAP.**

Scored window is 2500 ms = 66% of visual lifetime.

### 4.2 Input rule — explicit

Input during Entrance, Readable Hold, Persistence and Exit is **discarded at the point of receipt**:

- **Not buffered**
- **Not queued**
- **Not replayed into Phase 4**
- **Not scored**
- **Does not become a commission error**

Rationale: buffering pre-onset input would record a response against a target that was not yet answerable, producing reaction times negative relative to `DemoTrialOnsetTime`.

### 4.3 Target-array rule — explicit

The actor is registered in the target array **only during Phase 4.** Registered at Phase 4 entry, deregistered at Outcome lock.

Do not register-then-filter. Do not use an `bIsActive` flag on a permanently registered actor.

### 4.4 Spawn suppression

```
if (PresentationSpawnTime + VisualLifetime > SessionEnd) → do not spawn
```

Prevents the final bubble being truncated mid-exit.

### 4.5 Spatial parameters

| Parameter | Value |
|---|---|
| Spawn centre distance | 3.50 m |
| Settle centre distance | 3.00 m |
| **Hard floor, nearest surface — clamped constant** | **≥2.00 m** |
| Visual radius | 0.225 m (unscaled 0.50 m × scale 0.45) |
| Peak approach speed | 20 cm/s |
| Approach curve | decelerating, asymptotic to ~0 at Outcome lock. Not braked |
| Velocity at Exit start | must be 0 |

Assert the 2.00 m nearest-surface floor in code as a clamped constant, not a convention.

### 4.6 Exit composition — 400 ms

| Channel | From → To | Curve | Completes at |
|---|---|---|---|
| Opacity | 1.0 → 0.0 | **ease-out** | 100% |
| Position | settle → +8–12 cm **away** | ease-out | 100% |
| Scale | 1.0 → 0.97 | ease-out | 100% |
| Rim width | current → 0 | ease-out | **70%** |
| Rim intensity | current → 0 | ease-out | **70%** |

Rim must reach zero before the body does. Then Hidden (alpha hard-clamped 0, visibility off), then Destroy on a later frame.

### 4.7 Named A/B test — cadence

| | Default | Alternative |
|---|---|---|
| Name | **A-prime** | **B-prime** |
| Entrance / Hold / Response / Persist / Exit | 400 / 200 / **2500** / 300 / 400 | 500 / 300 / **3000** / 400 / 500 |
| Visual lifetime | 3800 | 4700 |
| Cadence | **4200** | **5000** |
| Clear gap | 400 | 300 |
| Visually complete trials in 120 s | 28 | 23–24 |

Build A-prime. Test B-prime as the named alternative at Stage 4. Pass criterion: neither "rushed" nor "monotonous".

### 4.8 Named A/B test — readable hold

Toggle Readable Hold to 0 ms and compare. If no perceptual difference, remove the phase.

---

## 5. Material parameter ownership

One shared master material. Four instances: Go, NoGo, Selected, Ambient-lightweight.

| Group | Owner | Parameters | Update rule |
|---|---|---|---|
| **A — shared base legibility** | identical across all Active instances | `OuterContourWidth`, `OuterContourColour` (**neutral/achromatic**), `OuterContourIntensity`, `CoreOpacity`, `FresnelPower`, `FresnelIntensity`, `TargetLuminance` | Set once at Entrance start. **Never per frame** |
| **B — Go/NoGo identity** | differs per instance | `InnerRimColour`, `InternalStructure`, `InternalMotionDirection`, `InternalMotionAmplitude` | Set once at Entrance start. **Never per frame** |
| **C — exit only** | exit driver | `ExitOpacityScalar`, `ExitRimScalar`, `ExitScaleScalar` | Written only during Phase 7 |
| **D — forbidden per-frame** | — | everything in Groups A and B | Per-frame writes are prohibited |

**Primary legibility cue is a view-independent outer contour, not Fresnel.** Fresnel is a secondary form cue and must never be the sole contour. Do not solve far-boundary legibility by increasing `CoreOpacity` or by increasing target scale.

---

## 6. Go / NoGo identities

### 6.1 Redundant cue stack — priority order

| Priority | Cue | Go | NoGo |
|---|---|---|---|
| **1** | Internal motion direction | **outward** / opening | **inward** / settling |
| **2** | Internal structure | dispersed interior | present diffuse central core |
| **3** | Hue (never load-bearing) | cool cyan inner rim | pale lavender / opal inner rim |

**Do not use red.** Silhouette, outer contour and apparent size are **identical** between Go and NoGo.

### 6.2 Quiet Core — single-mesh, in-material, mandatory default

Implement the core **inside the shared master material** using local-position-driven radial masking — sphere mask / radial gradient on object-local position, or an equivalent single-layer technique.

**Do NOT add a second translucent sphere mesh by default.**

A second core mesh may be evaluated later **only if all three hold:**

1. Single-layer discrimination has **failed** the §6.3 test
2. Measured Quest GPU and overdraw headroom **allows** it (capture stays inside the selected tier)
3. Stereo consistency is **confirmed on device** — identical in both eyes, correct disparity. Not verified in PIE

All three, not any one. If discrimination fails but headroom does not allow a second mesh, strengthen **internal motion** (priority 1), do not add geometry.

### 6.3 Discrimination acceptance test

| Condition | Pass |
|---|---|
| Full colour, 3.5 m | ≥95% correct |
| **Full-frame greyscale** (colour removed) | **≥95% correct** |
| **CVD simulation filter** | **≥95% correct** |

Greyscale failure means cues 1 and 2 are not working and the design is still colour-dependent.

### 6.4 Salience matching

Go and NoGo matched within **5%** on rendered-frame mean and peak luminance, apparent size, contrast against background, and onset salience. Measure against three HDRI regions.

### 6.5 Motion amplitude and period

**Tune on device, not in PIE.** PIE and HMD differ enough that PIE tuning is misleading. Amplitude must be large enough that internal motion resolves at 3.5 m / 8.58°; period slow enough that it does not read as pulsing.

---

## 7. Ambient budget

### 7.1 Performance-test starting point (measurement baseline, NOT the design)

| Layer | Count |
|---|---|
| Far ambient | **0** |
| Mid ambient | **0** |
| Cloud wisps | **0** |

### 7.2 Intended production target (after measured approval at each step)

| Layer | Distance | Target | Material | Condition |
|---|---|---|---|---|
| **Far ambient** | 8–15 m | **up to 3** | unlit **opaque or masked** billboard | GPU capture within tier after each object |
| **Mid ambient** | 4–7 m | **up to 2** | unlit lightweight translucent, single-layer | GPU capture within tier after each object |
| **Cloud wisps** | 6–12 m | **0 by default** | unlit translucent card | **up to 2, only if ≥3 ms headroom remains after Mid** |

**An empty ambient field is not the final visual design.** Zero is the instrumentation baseline. The production target is 3 Far + 2 Mid, wisps as a stretch goal.

Add **one object at a time** with a GPU capture between each. Far ambient before Mid.

### 7.3 Ambient invariants

- No ambient actor is ever registered in the target array
- No ambient actor accepts input
- No ambient actor draws from a Research random stream
- Ambient actors are pooled; pooled state must be fully reset on reuse

---

## 8. Staging rules — optional experiment only

**Default: 1 visible approaching bubble, 100%. Staging is not enabled by default.**

If the Stage 10 experiment runs, all of the following are mandatory:

| Rule | Requirement |
|---|---|
| Actor type | **Separate class.** Not an Active bubble with a flag |
| Target array | **Never registered** |
| Scoring | No code path from a staging actor reaches scoring |
| Role identity | **None.** Neutral colour. No Go/NoGo leakage about the next trial |
| Opacity | ≤40% of active bubble |
| Depth band | 4–6 m — separate from the active band (2.8–3.5 m), never overlapping |
| Lane angle | ±25–40° azimuth from the active lane |
| Vertical offset | ±10–20° elevation from the active lane |
| Stagger delay | 800–1500 ms offset from active spawn; onsets never coincide |
| Motion signature | **Lateral drift, not approach** |
| Exit path | **Sideways / outward.** Never toward the viewer, never on the active lane |
| Determinism | Own presentation seed. **Must not draw from a Research random stream** |

**Two role-coded Active bubbles must never be simultaneously visible at full opacity.** If overlap is ever evaluated, it uses neutral Staging or Ambient actors only.

Experiment: Condition 0 (1 bubble) vs Condition 1 (1: 70% / 2: 25% / 3: 5%). Escalate only if Condition 1 shows no cost.

---

## 9. Performance gates

### 9.1 Tiers

| | Baseline | Production target | Ceiling |
|---|---|---|---|
| GPU time ceiling | **7.7 ms** | **9.5 ms** | **11.0 ms** |
| Translucent objects on screen | 1 | 3 | 5 |
| Added draw calls vs today | 0 | ≤5 | ≤7 |
| Total draw calls per view | ≤700 | ≤700 | ≤700 |
| Total triangles per view | ≤500k | ≤500k | ≤500k |
| Renderer | forward | forward | forward |
| MSAA | 4× | 4× | 4× |

### 9.2 Blocking criteria — a stage fails if any fails

| # | Criterion | Measurement |
|---|---|---|
| **B1** | Sustained 72 FPS across a full 120 s session | frame-time trace, not a spot reading |
| **B2** | No meaningful compositor misses | stale-frame / app-drop count |
| **B3** | GPU time within the selected tier ceiling | GPU capture, sustained not peak |
| **B4** | No thermal degradation across the full session | clock/throttle trace, start to end |

### 9.3 DVFS

**A DVFS increase is a warning requiring investigation, not an automatic failure.** Report the DVFS level alongside B1–B4. If B1–B4 all pass with elevated DVFS, the stage passes — but record it, because elevated DVFS reduces margin for later stages and raises thermal risk over longer sessions.

### 9.4 Prohibited compensations

Do not reduce render resolution or MSAA to meet a gate. Those are the platform's recommended settings; trading them away hides the regression.

---

## 10. Stages and acceptance tests

Every stage: **one variable only**, then §1.1 regression gate.

| Stage | Change | Acceptance test |
|---|---|---|
| **1** | **Flash instrumentation.** No visual change. | All five §11 checks logged; **flash cause identified and written down** |
| **2** | **Lifecycle refactor** of C1–C4 into the 9-phase machine. **Keep current timing values.** | No scored event fires outside Phase 4; input in Phases 2/3/6/7 provably discarded; target-array registration confined to Phase 4; every C1–C4 component labelled retained/deleted/replaced/moved |
| **3** | **Compound exit** (§4.6) + hidden frame + deferred destroy. Fixes the Stage 1 cause. | 72 fps capture, final 15 frames stepped: no flash, no alpha rebound, no rim visible after alpha < 0.1, no destroy on the same frame as the last alpha write, no depth-sort pop |
| **4** | **A-prime timing** (§4.1). Then A/B vs B-prime. | Observer A/B, 3–5 people, counterbalanced: neither "rushed" nor "monotonous" |
| **5** | **Decelerating approach** (§4.5), Model 2. | Observer A/B: reads as "arrived", not "stopped". Velocity provably 0 at Exit start |
| **6** | **Base material legibility** — view-independent outer contour (Group A). | Target identified within 500 ms at 3.5 m, against 3 HDRI regions, at 3 head orientations |
| **7** | **Go/NoGo identity** — Quiet Core single-mesh (Group B). | §6.3 all three conditions ≥95%; §6.4 luminance within 5% |
| **8** | **Ambient field** — Far (opaque/masked) first, then Mid, one object at a time. | B1–B4 pass after each object. Comfort no worse than baseline |
| **9** | **Cloud wisps — optional.** Only if Stage 8 leaves ≥3 ms headroom. | B1–B4 pass. Abandon if over tier |
| **10** | **Staging — optional experiment** (§8). | Condition 1 shows no cost to target identification. Explicit keep-or-discard decision recorded |
| **11** | **Final regression.** No new change. | Full JSONL byte-diff + B1–B4 over a full 120 s session on device |

**Ordering rules:**

- **1 before 3** — fixing an undiagnosed bug produces a fix you cannot trust
- **2 before 4** — the timing complaint is largely an artefact of the collapsed lifecycle
- **6 before 7** — identity cues sit on top of base legibility
- **8 before 9** — wisps are the highest overdraw risk and lowest-confidence benefit
- **10 last, and optional** — staging is the only change that can degrade the task itself

---

## 11. Stage 1 instrumentation requirements

Log all five in desktop Environment PIE. Do not change visual behaviour in this stage.

| # | Check | Log |
|---|---|---|
| **1** | **Material parameter reset** | Every scalar/vector param on the MID at: exit start, 70% of exit, exit end, Hidden, Destroy. Look for any param reverting to parent default on MID release or re-acquire |
| **2** | **Visibility order** | Exact frame index of: last alpha write; `SetVisibility(false)`; `SetHiddenInGame`. Wrong order = one frame of default material rendered |
| **3** | **Deferred Destroy** | Frame index of `Destroy()` / pool return relative to the visibility toggle. **Assert ≥1 full frame separation** |
| **4** | **MID replacement** | Whether `CreateDynamicMaterialInstance` occurs on reuse. A fresh MID starts at parent defaults — one frame of that is a full-opacity flash. **Leading hypothesis** |
| **5** | **Motion-state reset** | Whether velocity / target-position state is cleared at Destroy or carried into pooled reuse |

**Do not tune exit timing before these five are logged and the cause identified.** Tuning first masks the bug.

---

## 12. Stop conditions

Stop work and report immediately if any of these occurs:

| # | Condition |
|---|---|
| **S1** | Research JSONL diff is not byte-identical after any stage |
| **S2** | Stage 1 cannot identify the flash cause from the five checks |
| **S3** | Any B1–B4 criterion fails and cannot be met without reducing resolution or MSAA |
| **S4** | §6.3 greyscale or CVD discrimination cannot reach 95% with a single-mesh Quiet Core |
| **S5** | A change appears to require touching R1–R7 |
| **S6** | Any Demo presentation code path is found reading from or advancing a Research random stream |
| **S7** | The 2.00 m nearest-surface floor would need to be lowered |
| **S8** | Scope drifts beyond the current stage |

**Never commit, push, package, build a distributable, or tag a release without explicit approval.** Report at the end of each stage and wait.

---

## 13. Files and systems likely to be touched

Indicative, to be confirmed by reading the project. Not a licence to modify anything not listed.

| Area | Expected work |
|---|---|
| Active bubble actor / component | Add the 9-phase state machine; confine target-array registration to Phase 4 |
| **New** Demo-only lifecycle component | Owns Phases 2, 3, 6, 7, 8. No access to R1–R7 |
| C1 exit implementation (350 ms) | Likely **replaced** |
| C2 MID fade | Likely **replaced or fixed** per Stage 1 finding |
| C3 VisualMotion exit state | Likely **moved** into the lifecycle component; must freeze at Outcome lock |
| C4 progress/completion delegates | Likely **retained** as the phase-transition mechanism |
| Object pool / spawner | MID and motion-state reset on reuse; spawn suppression (§4.4) |
| Shared master material + instances | Group A/B/C parameter split; view-independent outer contour; in-material Quiet Core |
| Demo session controller | Lead-in, cadence, spawn suppression, session end |
| Demo summary UI | RT computed from `DemoTrialOnsetTime`; language per §14 |
| Ambient actors (Stage 8+) | New, pooled, never in target array |
| Staging actors (Stage 10, optional) | New separate class, own seed |
| **Research pipeline** | **DO NOT MODIFY** |

---

## 14. Demo copy constraints

**Permitted:** calming · low-arousal · restorative · gentle · supportive

**Prohibited** in Demo UI, summary screen, and any accompanying material: treatment · therapy · therapeutic efficacy · clinical benefit · medically validated · reduces anxiety · improves attention · measures attention · assesses vigilance · sustained attention test

A 120-second demo cannot demonstrate sustained attention or vigilance. Do not claim it does.
