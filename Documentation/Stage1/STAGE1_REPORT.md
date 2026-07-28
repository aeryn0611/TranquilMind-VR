# STAGE 1 REPORT — CURRENT EXIT INSTRUMENTATION

```
Date:                          2026-07-28
UE version:                    5.7 (/Users/Shared/Epic Games/UE_5.7)
Modes captured:                Headless Environment (-game -nullrhi -benchmark -fps=72)  — 40 cycles
                               Authoritative Environment PIE, human observer      — 40 cycles
                               (PIE session isolated from log line 15024, 08:07:02;
                                earlier PIE sessions in the same log are NOT counted)
Cycles captured:               80 total (require >= 3 natural expiry) — all natural expiry, no input
Flash observed on cycles:      NONE — terminal flash NOT REPRODUCED (see §3.1)
Research JSONL byte-identical: YES  (canonical projection — see §5)
Automation:                    23/23 PASS
```

**Status: COMPLETE.**

| Gate | Result |
|---|---|
| Repository audit | **PASS** |
| Read-only instrumentation | **PASS** |
| Build | **PASS** |
| Headless trace | **PASS** |
| Human Environment PIE | **PASS** |
| Research canonical regression | **PASS** |
| Automation | **23/23 PASS** |
| Terminal flash (C5) | **NOT REPRODUCED** |
| Overlap cause (C7) | **CONFIRMED** |

**Stop Condition S2 is NOT triggered.** S2 covers Stage 1 failing to identify the
cause of a flash. There is no currently reproducible flash defect requiring a
causal fix, so there is nothing for S2 to gate.

---

## 1. Prerequisites resolved (Kit §1)

| Slot | Resolution |
|---|---|
| **P1** | `BP_Target_C` (`/Game/BP_Target`), Blueprint subclass of native `ATranquilMindTargetActor`. Set as `TargetActorClass` in both `L_TranquilMind_Environment.umap` and `L_TranquilMind_Void.umap`. No graph logic; SCS overrides `ResponseWindow_MS` / `MovementSpeed_CMPerSec`, both overwritten by `InitializeTarget()`. |
| **P2** | `MeshComponent` (`UStaticMeshComponent`, `/Engine/BasicShapes/Sphere`) — `TranquilMindTargetActor.cpp:55` |
| **P3** | `VisualMotion` (`UTMVisualMotionComponent`). Hierarchy `GameplayAnchor` → `VisualMotion` → `MeshComponent` |
| **P4** | Split. Timer: `UTMVisualMotionComponent::TickComponent` `bExiting` branch, `ExitDuration_SEC` = **0.35 s** (struct default; not overridden by BP_Target). Material writer: `ATranquilMindTargetActor::HandleVisualExitFade`. Scale writer: `ApplyMotionTransform()` |
| **P5** | **One call site in the whole project**: `TranquilMindTargetActor.cpp:487`, inside `BeginDemoVisualExit()`. Zero Blueprint call sites. Created **at exit begin**, not at spawn |
| **P6** | `FTMVisualExitProgress OnVisualExitProgress` — broadcast `TMVisualMotionComponent.cpp:137` |
| **P7** | `FTMVisualExitComplete OnVisualExitComplete` — broadcast `TMVisualMotionComponent.cpp:144` → `HandleVisualExitComplete()` → `Destroy()` |
| **P8** | `UTargetSpawnerComponent`. Cadence in `TickComponent`; construction in `SpawnNextTarget()` |
| **P9** | `TArray<TObjectPtr<ATranquilMindTargetActor>> ActiveTargets` (`TargetSpawnerComponent.h:47`). One add site; removes in `DestroyAllActiveTargetsAsVoid`, `HandleTriggerPulled`, `CleanupResolvedTargets`. **Pruned on `IsResolved()`, i.e. at exit start — ~350 ms before the actor stops rendering** |
| **P10** | `TranquilMindTargetActor.cpp:476` (immediate), `:524` (post-exit). Research: `TMResearchRunner.cpp:327`. **No pool-return site exists** |
| **P11** | Written by the exit: `CenterOpacity`, `EdgeOpacity`, `BubbleBrightness`. Also present on `MI_Bubble_*`: `FresnelExponent` (+ vectors `EdgeColorA`, `EdgeColorB`). **No rim-width parameter exists** — Spec §4.6's Rim width / Rim intensity have no material counterpart |
| **P12** | **No object pooling.** Confirmed by evidence, not by reading — see §3.4 |

### Bizarre source paths — still present

`Source/TranquilMind/Private/Private/Private/TranquilMindTargetActor.cpp` and
`Source/TranquilMind/Public/Private/Private/TranquilMindTargetActor.h`, reached via
`#include "Private/Private/TranquilMindTargetActor.h"` and
`"../Public/Private/Private/TranquilMindTargetActor.h"`. Six further module files sit at
the module root, outside `Public/` and `Private/`. Untouched — out of Stage 1 scope.

---

## 2. Event ledger — representative cycles

Frames, Δ from SPAWN. Capture at fixed 72 fps, so 1 frame = 13.89 ms.

| Event | cyc5 | cyc6 | cyc7 | cyc8 |
|---|---|---|---|---|
| spawn | 0 | 0 | 0 | 0 |
| outcome resolution | +180 | +180 | +180 | +180 |
| exit begin | +180 | +180 | +180 | +180 |
| MID_CREATE | +180 | +180 | +180 | +180 |
| exit 70% | +198 | +197 | +198 | +197 |
| final material write | +206 | +205 | +206 | +205 |
| completion delegate | +206 | +205 | +206 | +205 |
| Destroy request | +206 | +205 | +206 | +205 |
| EndPlay | +206 | +205 | +206 | +205 |
| visibility false | **NEVER EMITTED** | | | |
| HiddenInGame true | **NEVER EMITTED** | | | |

Exit span 25–26 frames = 347–361 ms, matching `ExitDuration_SEC` 0.35 s.

**Separation checks across all 40 cycles — the distinct value set, not a sample:**

```
DESTROY_REQ − last MAT_WRITE   = {0}   frames
ENDPLAY     − DESTROY_REQ      = {0}   frames
DESTROY_REQ − DELEGATE_COMPLETE= {0}   frames
```

---

## 3. Findings

### 3.1 Terminal flash (C5) — **NOT REPRODUCED IN THE CURRENT INSTRUMENTED BUILD**

The log **refutes** the Spec's leading hypothesis and rules out three of Kit §6.1's rows.

Counts below are the headless capture; the authoritative PIE capture agrees on
every row, with the identity rows requiring the temporal disambiguation in §3.5.

| Kit §6.1 row | Verdict |
|---|---|
| MID replacement on reuse | **REFUTED.** 40 distinct `auid` at SPAWN, 40 distinct `miduid` at MID_CREATE, `ENDPLAY reason=0` (Destroyed) on all 40. There is no reuse to replace a MID on |
| Ordering fault (MAT_WRITE after VIS_FALSE) | **CANNOT OCCUR.** Zero `VIS_FALSE` events — no such call site exists |
| Visibility-order fault | **CANNOT OCCUR.** Zero `VIS_FALSE`, zero `HIDDEN_TRUE` |
| Fade never reaches zero | **REFUTED.** Final `MAT_WRITE` across all 40 cycles is exactly `0.000000` for all three parameters |
| **Same-frame destroy** | **CONFIRMED, 40/40.** `DESTROY_REQ` frame == final `MAT_WRITE` frame in every cycle |
| **No deferred destroy** | **CONFIRMED, 40/40.** `ENDPLAY` − `DESTROY_REQ` = 0 frames. Spec §11 check 3 requires ≥1 full frame; actual separation is **zero** |
| MID lost mid-fade | **20/40 cycles** show one `SNAP` with `midvalid=0` on the `EXIT_BEGIN` frame itself. This is a tick-ordering artefact — on those frames `UTMVisualMotionComponent::TickComponent` ran before the actor's `Tick` resolved the trial, so the snapshot predates the MID swap by microseconds within the frame. It is **not** a MID lost during the fade: every subsequent exit frame reports `midvalid=1` |

**Authoritative human observation (Environment PIE, 40 cycles): terminal flash
NOT OBSERVED.** The flash did not reproduce in the current instrumented build.

**C5 is therefore recorded as: NOT REPRODUCED IN THE CURRENT INSTRUMENTED
BUILD.** Not "fixed", not "explained", not "absent" — not reproduced.

**Same-frame teardown is recorded as: CONFIRMED ARCHITECTURAL FRAGILITY /
FUTURE EXIT REQUIREMENT.** It is *not* recorded as a proven visual-flash root
cause. The trace proves, across both captures, that the final material write, the
completion delegate, the Destroy request and EndPlay all occur on **one frame**,
with no Hidden frame, no pooling and no cross-actor MID reuse:

```
                              headless (40 cyc)   authoritative PIE (40 cyc)
DESTROY_REQ − last MAT_WRITE        {0}                   {0}
ENDPLAY     − DESTROY_REQ           {0}                   {0}
VIS_FALSE / HIDDEN_TRUE            0 / 0                 0 / 0
final MAT_WRITE value            0.000000              0.000000
```

(Distinct value sets, not samples.) That violates Spec §11 check 3 and Spec §4.6,
and it must be corrected by the Stage 3 compound exit. But no observation links
it to a visible artefact, and asserting the link would be an unproven causal claim.

**No explanation is offered for the earlier flash report.** It may have been an
intermittent artefact, abrupt teardown, overlap brightness, or a perceptual
misclassification. None of these is proven and none is adopted. The honest state
is: previously reported, not reproducible under instrumentation, cause unknown.

Consequences for Stage 3: the compound exit (§4.6) is still required — on the
evidence of the §11 check 3 violation and the observer's "exit remains visually
abrupt" (§6a V4), **not** on a flash diagnosis. Should the flash reappear, this
trace and its archived logs are the baseline to diff against.

One measured detail that will matter in Stage 3: **`CenterOpacity`'s base value
is `0.000995`** — the bubble core is already fully transparent before the exit
starts, so fading that channel is a no-op. The visible channels are
`EdgeOpacity` (base `0.577332`) and `BubbleBrightness` (base `2.189882` Go /
`2.289422` NoGo).

### 3.2 Continued-approach transform path — **NOT ACTOR TRANSLATION**

Across the whole exit window, cycle 7:

```
apdist   EXIT_BEGIN 300.000 → EXIT_70 300.000 → last SNAP 300.000  cm
axform   EXIT_BEGIN [L=-0.000,0.000,150.000;S=0.4500,0.4500,0.4500]
         last SNAP  [L=-0.000,0.000,150.000;S=0.4500,0.4500,0.4500]   ← identical
vmrel    EXIT_BEGIN [L=-0.476,0.450,0.319;S=1.0000,1.0000,1.0000]
         last SNAP  [L=-0.492,0.417,0.135;S=0.9400,0.9400,0.9400]
```

Approach runs at 0.278 cm/frame (20 cm/s) up to `EXIT_BEGIN`, then **stops
dead**: the actor transform is bit-identical for all 26 exit frames, because
`FinishResolution()` calls `SetActorTickEnabled(false)` before the exit begins.

The only motion during the exit is `VisualMotion`'s relative transform: a
sub-centimetre idle drift (≈0.2 cm total) and the scale settle 1.0000 → 0.9400.
Scale **shrinks**, so it cannot produce looming.

**This contradicts Spec §2 C3** ("does not stop approach motion at exit →
still appears to approach while fading"). Measured: actor-level approach does
stop at exit. Whatever produces the reported perception, it is not actor
translation, not mesh-component translation, and not a growing scale. C3's
disposition must not be assigned until the PIE capture with an observer.

### 3.3 Overlap cause (C7) — **FULLY CONFIRMED**

Confirmed independently by **both** available methods:

- **Human Environment PIE observation** — "occasional two-bubble visual overlap:
  OBSERVED" (§6a V3).
- **TMS1 trace** — `visiblepresentations` reaches 2 while `activetargets`
  stays 1.

This is a **presentation-ownership defect, not a scoring-concurrency defect.**
Two bubbles are on screen; only one is ever registered as a target. No scored
event can reach the second. Kit §6.3's escalation row (`activetargets >= 2`)
does **not** fire.


```
                          headless (40 cyc)          authoritative PIE (40 cyc)
max visiblepresentations         2                            2
max activetargets                1                            1      → no scoring defect
overlap frames                  26 (1 run, 361 ms)           58 (15 runs)
longest sustained run           26 frames                    30 frames
predecessor EdgeOpacity         0.5773 → 0.0000              0.5781 → 0.0000
  during overlap
```

The observer saw it as "occasional", which the trace matches exactly: 15 runs,
only one of them sustained (30 frames ≈ 420 ms); the remainder are 2-frame
transients at spawn/teardown boundaries.

Both actors, from the log:

```
auid=44894  spawn f796   exit begin f976   endplay f1002
auid=44900  spawn f977   exit begin f1157  endplay f1183
```

The successor spawns **one frame after** the predecessor's exit begins, and the
predecessor then fades from `EdgeOpacity 0.577332 → 0.000000` while fully
co-visible. Kit §6.3's worst row: *overlap with a substantially visible
predecessor*.

**But it is not "lifetime exceeds cadence."** Steady-state spawn deltas are
216 frames (3000 ms) in 35 of 39 intervals, against a 2850 ms visual lifetime
(2500 ms window + 350 ms exit) — a 150 ms clear gap. The single anomalous
interval is **181 frames**. The causal chain, entirely from the log:

1. **f=864** — `LogTranquilMindSessionManager: [Phase II] Core Training`.
   `HandlePhaseChanged()` sets `LastSpawnTimestamp_SEC = -1.0f`.
2. **f=865–976** — the `LastSpawnTimestamp_SEC < 0` branch calls
   `SpawnNextTarget()` every tick; each call returns at the
   `if (ActiveTargets.Num() > 0) return;` gate because 44894 is still unresolved.
3. **f=976** — 44894's response window expires; `bResolved = true`;
   `BeginDemoVisualExit()`.
4. **f=977** — `CleanupResolvedTargets()` prunes 44894 on `IsResolved()`. The
   gate reads **registration, not visibility**, so it now opens and the latched
   spawn fires immediately — into a frame where 44894 still renders at 57.7%
   edge opacity.

**Trace-proven causal chain — the canonical statement of C7:**

1. The predecessor **resolves**.
2. The predecessor is **pruned from `ActiveTargets` immediately**
   (`CleanupResolvedTargets()` prunes on `IsResolved()`).
3. The **one-target gate sees `ActiveTargets == 0`**
   (`SpawnNextTarget()`: `if (ActiveTargets.Num() > 0) return;`).
4. A **pending spawn request is released**.
5. The new target **becomes visible while the predecessor is still inside its
   350 ms presentation exit**.
6. **`visiblepresentations` reaches 2 while `activetargets` remains 1.**

The root asymmetry is at steps 2–3: **the gate reads registration, not
visibility.** Deregistration happens at resolution; the actor stops rendering
350 ms later. That interval is exactly the window in which a release can admit
a second visible presentation — and it is exactly the 26 frames measured.

It is **not** a cadence arithmetic error. Steady-state spawn deltas are 216
frames (3000 ms) in 35 of 39 intervals, against a 2850 ms visual lifetime
(2500 ms window + 350 ms exit) — a 150 ms clear gap. In this capture the
pending request of step 4 was created by the Phase II boundary
(`HandlePhaseChanged()` → `LastSpawnTimestamp_SEC = -1.0f` at f=864), which is
why the anomalous interval is 181 frames rather than 216. **The phase boundary
is one way to create a pending request; it is not the defect.** The defect is
that a release is admitted at all during the predecessor's exit, which steps 2–3
permit by construction.

**Not fixed in Stage 1.** The repair belongs to the Stage 2 lifecycle refactor
(§4.3 target-array rule) and the Stage 3 clear gap (§4.1).

### 3.4 Does pooling exist — **NO**

40 spawns, 40 distinct `auid`, 40 `ENDPLAY` with `reason=0` (Destroyed), zero
`DESTROY_REQ` with `mode=pool`, and no pool-return call site in the project.
Every cycle is a fresh `World->SpawnActor` and a `Destroy()`.

*Caveat recorded:* `auid` is a UE object index and is recycled after GC — the
observed values rise and then fall back into reused slots. All 40 are distinct
within this capture, so the conclusion holds here, but `auid` alone is not a
durable identity across a long session.

### 3.5 MID / motion state reuse — **NONE**

Headless: 40 distinct `miduid`, each under exactly one `auid`.

**The authoritative PIE session required disambiguation, and a naive check would
have produced a false positive here.** In 40 PIE cycles there were only 39
distinct `auid` and 38 distinct `miduid`, and one `miduid` appeared under two
different `auid` — which is Kit §6.5's "MID SHARED ACROSS ACTORS — serious" row.
It is not sharing. Both are UE object-index recycling after GC, proved by
comparing lifetimes:

```
auid 52109    spawn cyc98 f=290373, endplay f=290383
              spawn cyc118 f=295171, endplay f=295283      → sequential, gap ≈ 4788 frames

miduid 52112  under auid 51872 : frames 290372..290374
              under auid 47112 : frames 295131..295159     → ranges DO NOT overlap
```

No two actors ever held the same MID **concurrently**, and every cycle has an
`ENDPLAY reason=0` (Destroyed). This is the `auid`-recycling caveat recorded in
§3.4 actually materialising. Sharing is refuted; index reuse is confirmed and is
expected with no pooling.

### 3.6 Maxima

```
MAX visiblepresentations observed : 2    frames above 1: 26  (f=977..1002)
MAX activetargets observed        : 1    (>1 would escalate — it does not)
```

---

## 4. Instrumentation as built

Files added: `Source/TranquilMind/Public/TMStage1Trace.h`,
`Source/TranquilMind/Private/TMStage1Trace.cpp`.
Call sites added to `TranquilMindTargetActor.{h,cpp}`,
`TMVisualMotionComponent.{h,cpp}`, `TargetSpawnerComponent.cpp`.

Every tracer function is a getter. No `CreateDynamicMaterialInstance`, no
setter, no transform write, no `Destroy`, no random draw (Kit §0 N1–N3).

### Deviations from the supplied kit — each deliberate, each recorded

| # | Deviation | Reason |
|---|---|---|
| **D1** | `Engine/EngineTypes.h` instead of `enum class EEndPlayReason : uint8;` | The kit's forward declaration is wrong — `EEndPlayReason` is a namespaced enum. It does not compile |
| **D2** | SNAP window opens at SPAWN, not retroactively at `EXIT_BEGIN − 5` | The kit's window can never capture frames that have already elapsed. Opening at spawn yields real lead frames and made §3.2's "approach stops at exit" measurable |
| **D3** | Approach axis published by the actor from `TravelDirection_World` | The axis is per-instance here (resolved from the Demo session frame), not a fixed world axis |
| **D4** | `prev_auid` served from the tracer's own last-SPAWN record | Avoids adding gameplay-visible state to the spawner |
| **D5** | Transform payloads use `;` between L/R/S groups, not `\|` | `\|` is `parse_tm_trace.py`'s field separator. With the kit's format the parser keeps only `[L=x,y,z` under each transform key and collides the stray `R=`/`S=` fragments across all three transforms, so rotation and scale changes are invisible to §6.2 |
| **D6** | `cyc`, the SNAP window and the EXIT_70 latch are keyed **per actor**, not global | **Found by the first headless capture.** Overlap is real (§3.3), so the next SPAWN bumped the global counter while the previous actor was still exiting, emitting its remaining exit frames under the successor's cycle number. Cycle 4's entire exit was relabelled cycle 5 and vanished from its own ledger. After the fix, all 40 cycles carry exactly one of each singleton event |

### Known reading caveat for the supplied parser

Because of D2, `SNAP` lines exist for the whole actor life, and slot 0 is the
shared `MI_Bubble_*` (not a MID) until the exit begins. `parse_tm_trace.py`'s
§6.1 "MID lost mid-window" rule is not scoped to the exit window, so it fires
on every cycle from those pre-exit lines. The scoped result is in §3.1: 20/40
cycles, `EXIT_BEGIN` frame only, tick-ordering artefact.

**Second caveat, found in the authoritative PIE data.** The parser's §6.5 rule
groups `miduid → {auid}` and reports "MID SHARED ACROSS ACTORS — serious" whenever
a `miduid` appears under more than one `auid`. It applies **no temporal test**, so
UE object-index recycling after GC trips it. On the authoritative PIE log it
would report a false positive; the lifetimes do not overlap (§3.5). Any future
run of this parser must apply the temporal check before acting on that row.

The supplied parser was **not modified** — it is quoted as-is and both scopings
were applied in analysis.

### Research-path exposure — disclosed

`LogSpawn` and `LogEndPlay` sit on `ATranquilMindTargetActor::BeginPlay/EndPlay`,
which the Research runner also uses. A Research run therefore emits **100 extra
log lines** (50 `SPAWN` + 50 `ENDPLAY`). They are log-only; §5 proves they change
nothing. `bStaticResearchStimulus` is set *after* `BeginPlay`, so `SPAWN` cannot
be cleanly gated on it. No other Research call site was instrumented — in
particular `TMResearchRunner`'s `SetActorHiddenInGame` calls were deliberately
left alone.

---

## 5. Step E — Research regression

Spec §1.1 demands a byte-identical fixed-seed JSONL. **The raw file cannot be
byte-identical, and never could be, before or after any change** — the schema
carries a fresh `sessionId` GUID, wall-clock `startedUtc`/`endedUtc`, and
absolute monotonic-uptime timestamps.

Rather than assume which fields are noise, **two independent PRE-CHANGE runs
were captured before a single line was edited** and diffed field by field:

- **Deterministic** (identical across both pre-change runs): `seed`, `seqHash`,
  `stimulus`, `block`, `trial`, `outcome`, `hadResponse`, `rtMs`,
  `responseWindowMs`, `scheduledItiMs`, `valid`, `voidReason`, `gsr`, and every
  footer aggregate.
- **Noise floor** (already differed pre-change): `sessionId`, `startedUtc`,
  `endedUtc`, all absolute `*Sec` timestamps, and `realizedItiMs` (±0.1 ms on
  20/50 records). Absolute *scheduled* times drift too — the runner schedules
  trial *n+1* from the **realized** completion of trial *n*, so frame
  quantisation accumulates down the block. The seeded quantity driving them,
  `scheduledItiMs`, is bit-identical.

The gate is byte-identity of the canonical projection that removes exactly those
fields. **The gate was validated on the two pre-change runs before being
used** — they project to an identical file.

```
pre-change run A   canonical sha256  6ae4fd8c2e630806b089615c612b03d1cf41b013c5b566da9393960b21ac97eb
pre-change run B   canonical sha256  6ae4fd8c...  (identical — gate valid)
post-Stage-1 run   canonical sha256  6ae4fd8c...  (identical — PASS)
```

**STEP E: PASS.** Seed 20260708, `seqHash 4922870221080512783`, 50 trials,
schedule unchanged. Stop Condition S1 not triggered.

Automation suite: **23/23 pass**, including
`TranquilMind.Demo.VisualExit.Lifecycle`,
`TranquilMind.Demo.VisualExit.InertWithoutPresentation`, and all
`TranquilMind.Research.*`.

Artefacts: `Documentation/Stage1/baseline/`, `Documentation/Stage1/evidence/`.

---

## 6. Dispositions for C1–C4

Assigned only where instrumentation proves it.

| | Disposition | Basis |
|---|---|---|
| **C1** 350 ms visual exit | **REPLACE in Stage 3** | Visually abrupt (§6a V4) and lacks a Hidden phase and a deferred destroy. Trace: no `VIS_FALSE`, no `HIDDEN_TRUE`, and `DESTROY_REQ` − final `MAT_WRITE` = 0 frames in 118/118 cycles, against Spec §11 check 3's required ≥1 full frame. Replaced by the §4.6 compound exit — **on the strength of the missing Hidden/deferred-destroy phase, not on a flash diagnosis** |
| **C2** MID fade | **NOT YET PROVEN** | No pooling, no cross-actor MID reuse, and no reproducible flash. The Spec's prime suspicion (§2 C2, "a MID replaced or re-acquired on pool reuse") is refuted by evidence: 118 spawns, 118 distinct `auid`, 118 distinct `miduid`, each under exactly one actor. Nothing observed justifies replacing it |
| **C3** VisualMotion exit state | **NOT YET PROVEN / REASSESS IN STAGE 2** | Actor approach is **proven stopped** at exit (§3.2): the actor transform is bit-identical across all exit frames. Only relative drift (≈0.2 cm) and the scale settle (1.0000 → 0.9400) remain. Spec §2 C3's stated premise does not hold; reassess once the Stage 2 lifecycle defines phase ownership |
| **C4** progress / completion delegates | **RETAIN** | Correctly delivered progress and completion in every cycle — 118 `DELEGATE_COMPLETE`, one per cycle, each preceded by a monotone `DELEGATE_PROGRESS` series terminating at exactly 1.0, driving three material writes per tick. Suitable as the future lifecycle phase-transition mechanism (Spec §13) |

---

## 6a. Human visual observations — authoritative Environment PIE

Observer result from the authoritative Environment PIE run, instrumented build.

| # | Observation | Result | Nearest Spec reference | Stage 1 status |
|---|---|---|---|---|
| **V0** | **Terminal flash** | **NOT OBSERVED — not reproducible in the current instrumented build** | C5 / §11 | **Closed as NOT REPRODUCED** (§3.1) |
| V3 | Two-bubble visual overlap | **OBSERVED** | C7 / §4.1 clear gap | **Confirms §3.3.** One of the two independent confirmations of C7 |
| V4 | Exit remains visually abrupt | **OBSERVED** | C5 / §4.6 compound exit | Consistent with §3.1's same-frame teardown. Supports the Stage 3 compound exit as a **requirement**, not as a flash diagnosis |
| V1 | Go / NoGo discrimination remains poor | **OBSERVED** | §6.1 cue stack, §6.3 test | **Out of Stage 1 scope** — Stage 7 |
| V2 | Bubble boundary hard to see at distance | **OBSERVED** | §5 Group A, view-independent outer contour | **Out of Stage 1 scope** — Stage 6 |

V3 is a genuine dual confirmation: the observer saw it and the trace measured it,
independently. V4 is corroborating but not causal — an observer describing an
exit as abrupt does not establish *which* mechanism makes it so.

V1 and V2 are recorded for Stages 6–7 and are **not** acted on here. They must
not influence any C1–C4 disposition.

An earlier, non-authoritative PIE run (2026-07-28 07:48/07:50, superseded by
this one) additionally reported that apparent size changes very little over the
lifetime — consistent with the measured 349.7 → 300.0 cm travel (≈14% distance
change, ≈1.17× angular growth). Retained as an observation only; it belongs to
Stage 5 (§4.5) and is not a Stage 1 finding.


---

## 7. Stage 1 final status — COMPLETE

| Gate | Result |
|---|---|
| Repository audit (P1–P12) | **PASS** |
| Read-only instrumentation (N1–N3) | **PASS** |
| Build (TranquilMindEditor Mac Development) | **PASS** |
| Headless trace (40 cycles) | **PASS** |
| Human Environment PIE (40 cycles, observer) | **PASS** |
| Research canonical regression | **PASS** |
| Automation | **23/23 PASS** |
| Terminal flash (C5) | **NOT REPRODUCED** |
| Overlap cause (C7) | **CONFIRMED** |

**Stop conditions:** S1 not triggered (regression identical). S2 not triggered
(no currently reproducible flash defect requiring a causal fix). S5, S6, S7 not
encountered. S8 not triggered — nothing outside Stage 1 was changed.

### Carried forward, not acted on

| Item | Owner stage |
|---|---|
| Overlap repair — gate on visibility, not registration; §4.3 target-array rule; §4.1 clear gap | Stage 2 / Stage 3 |
| Compound exit + Hidden frame + deferred destroy (§4.6) — fixes the confirmed §11 check 3 violation | Stage 3 |
| `CenterOpacity` base is `0.000995`, so fading it is a no-op; the visible channels are `EdgeOpacity` and `BubbleBrightness` | Stage 3 |
| No rim-width / rim-intensity material parameter exists for §4.6's 70% rim channel | Stage 3 |
| Go / NoGo discrimination poor (observer V1) | Stage 7 |
| Bubble boundary hard to see at distance (observer V2) | Stage 6 |
| Apparent size changes little over lifetime | Stage 5 |
| `L_TranquilMind_Environment` Level Blueprint `Accessed None` on `TargetSpawner` — pre-existing, PIE-only, cosmetic (redundant with C++ registration) | see `RUNTIME_BLOCKER_AUDIT.md`; **not repaired here** |
| Duplicated source paths (`Private/Private/Private/…`) | housekeeping |

### Archived evidence

```
Documentation/Stage1/
  STAGE1_REPORT.md                       this report
  RUNTIME_BLOCKER_AUDIT.md               Level Blueprint blocker audit
  TranquilMind_Implementation_Spec.md    authority (verbatim copy)
  Stage1_Instrumentation_Kit.md          subordinate kit (verbatim copy)
  TMStage1Trace.h / .cpp                 kit tracer as supplied (unmodified)
  parse_tm_trace.py                      kit parser as supplied (unmodified)
  baseline/
    prechange_run_A.raw.jsonl            pre-Stage-1 Research run A
    prechange_run_B.raw.jsonl            pre-Stage-1 Research run B
    baseline_A.canon.jsonl               canonical projection (gate reference)
    baseline_B.canon.jsonl               canonical projection (gate validation)
    canon_jsonl.py                       canonical projection tool
    jsonl_fielddiff.py                   field-level determinism differ
  evidence/
    headless_environment_capture.log     40-cycle headless trace
    pie_authoritative_capture.log        40-cycle authoritative PIE trace
    stage1_report_tables_headless.md     parser output (headless)
    stage1_report_tables_pie.md          parser output (authoritative PIE)
    poststage1_research.raw.jsonl        post-instrumentation Research run
    poststage1.canon.jsonl               canonical projection (PASS)
```

Instrumentation left in place, per Kit §5.8 — it is required for Stages 2 and 3
and is the baseline to diff against should the flash reappear.

```
STAGE 1 COMPLETE.  STAGE 2 NOT STARTED.
Overlap not fixed.  Level Blueprint not repaired.  Not committed.  Not packaged.
```
