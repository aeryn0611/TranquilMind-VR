# STAGE 2 — LIFECYCLE AND PRESENTATION OWNERSHIP REFACTOR

```
Date:                      2026-07-28
Authority:                 Documentation/TranquilMind_Implementation_Spec.md (Revision 2)
Timing configuration:      Stage 2 Compatibility (§4.7) — 0 / 0 / 2500 / 0 / 350, cadence 3000
Build:                     TranquilMindEditor Mac Development — PASS
Automation:                32/32 PASS  (23 pre-existing + 9 new)
Headless Environment:      40 cycles, Stage 2 acceptance check — PASS
Research canonical gate:   6ae4fd8c…97eb — BYTE-IDENTICAL to the approved baseline
Assets changed:            NONE (map, material, Blueprint hashes and mtimes unchanged)
```

**Status: COMPLETE. Not committed. Not packaged. Stage 3 not started.**

The Step A audit that preceded any code change is a separate artefact:
`Documentation/Stage2/STAGE2_STEP_A_DESIGN_REPORT.md`.

| Gate | Result |
|---|---|
| Step A audit and design, before any edit | **PASS** |
| Editor build | **PASS** |
| Automation, existing + new | **32/32 PASS** |
| Headless Environment capture (40 cycles) | **PASS** |
| S2-A max official live presentations = 1 | **PASS** |
| S2-B max ActiveTargets = 1 | **PASS** |
| S2-C no successor while predecessor alive | **PASS** |
| S2-D no role-coded overlap | **PASS** — 0 frames (baseline: 26) |
| S2-E early response does not shorten visual lifetime | **PASS** |
| S2-F expiry and early response converge | **PASS** |
| S2-G no scored event outside Phase 4 | **PASS** |
| S2-H input in Phases 2/3/6/7 provably discarded | **PASS** |
| S2-I ActiveTargets confined to Phase 4 | **PASS** |
| S2-J every C1–C4 component labelled | **PASS** — §4 below |
| Research regression (Layers A + B) | **PASS** |
| Stop conditions S1, S5, S6, S8, S9, S10, S11 | **not triggered** |

---

## 1. The defect this stage repairs, measured before and after

Stage 1 §3.3 confirmed C7 by two independent methods and traced its causal chain
to a single asymmetry: **the spawn gate read registration, not visibility.**

| Signal | Stage 1 baseline | Stage 2 |
|---|---|---|
| max `visiblepresentations` | **2** | **1** |
| frames with 2 bubbles visible | **26** (headless) / **58** (PIE) | **0** |
| max `activetargets` | 1 | 1 |
| max `livepresentation` | *(did not exist)* | **1** |
| spawn frame-delta multiset | `{181:1, 216:35, 217:3}` | `{206:1, 216:36, 217:2}` |
| outcome distribution | Omission 20, CorrectRejection 19, Void 1 | **identical** |

The anomalous interval is the sharpest evidence. The Phase II boundary latch
(`HandlePhaseChanged()` → `LastSpawnTimestamp_SEC = -1`) still fires, unchanged.
Before Stage 2 the latched spawn was released **one frame after the predecessor
was deregistered**, giving a 181-frame interval and 26 frames of overlap. It now
waits for the predecessor's presentation to actually end:

```
181 frames (baseline, released at deregistration)
+ 25 frames (the measured 350 ms exit the predecessor still had to run)
= 206 frames (Stage 2, released at Phase 9)
```

Arithmetically exact, and the overlap it used to produce is gone. The clear gap
is preserved instead of being consumed.

---

## 2. Files changed

### New

| File | Purpose |
|---|---|
| `Source/TranquilMind/Public/TMDemoLifecycleComponent.h` | Nine-phase presentation state machine (§4.1) and its timing struct |
| `Source/TranquilMind/Private/TMDemoLifecycleComponent.cpp` | Phase advance, scheduled transitions, exactly-once outcome lock |
| `Source/TranquilMind/Private/Tests/TMStage2LifecycleTest.cpp` | 9 new automation tests covering Step F items 1–18 |
| `Documentation/Stage2/tools/stage2_check.py` | Schema-2 Stage 2 acceptance checker |
| `Documentation/Stage2/STAGE2_STEP_A_DESIGN_REPORT.md` | The pre-edit audit and design |
| `Documentation/Stage2/evidence/*` | Captures, check output, raw + canonical Research JSONL |

### Modified

| File | Change |
|---|---|
| `Public/Private/Private/TranquilMindTargetActor.h` | Lifecycle component; weak owning-spawner ref; `IsDemoInputOpen()`; lifecycle callbacks; `FinishResolution(bool bCancellation)`; one-shot exit flag |
| `Private/Private/Private/TranquilMindTargetActor.cpp` | Input gate on `ResolveAsTriggered`; RT measured from `DemoTrialOnsetTime`; outcome lock hands off to the lifecycle; expiry authority; Research `ForceInert()` second layer |
| `Public/TargetSpawnerComponent.h` | `LiveDemoPresentation`; `HasLiveDemoPresentation()`; `RegisterPhase4Target` / `DeregisterPhase4Target` |
| `Private/TargetSpawnerComponent.cpp` | Spawn gate on presentation ownership; ownership claim; lifecycle arming; re-entrancy-safe array loops; cadence re-anchor removed; pending flag cleared on both failure returns; teardown release |
| `Public/TMStage1Trace.h`, `Private/TMStage1Trace.cpp` | **Log-only.** `livepresentation` field on `SNAP` and `NEXT_SPAWN` |
| `Private/Tests/TMVisualExitTest.cpp` | Assertion (6) **deliberately inverted** — see §6 |

**Not modified:** `TMResearchRunner`, `TMSequenceGenerator`, `TMTrialLogger`,
`TMResearchSettings`, `TMResearchConfig`, `TMVisualMotionComponent`,
`TranquilMindSessionManager`, `TMDemoSessionFrame`, the hardened parser
`parse_tm_trace_v2.py` and its 27 fixtures, the frozen `Documentation/Stage1/`
kit, and every asset.

---

## 3. Lifecycle architecture

### 3.1 Why a component, not the actor tick

`FinishResolution()` calls `SetActorTickEnabled(false)` at the outcome lock.
Stage 1 §3.2 measured the consequence: the actor transform is **bit-identical
across every exit frame**. That freeze is a direct product of the call, so
hosting a phase clock on the actor tick would have meant keeping the tick alive
and re-gating travel inside it — putting a proven-correct behaviour at risk for
no benefit, which C3 forbids in Stage 2.

`UTMVisualMotionComponent` was rejected as the host on its own stated contract: a
generic reusable motion rig that "never reads gameplay state". A Demo-trial phase
machine is gameplay state.

`UTMDemoLifecycleComponent` ticks independently of the actor tick, exactly as
`UTMVisualMotionComponent` already does. **`SetActorTickEnabled(false)` is
untouched.**

### 3.2 The nine phases at Compatibility timing

| # | Phase | Enters at | Duration | Input | In `ActiveTargets` | Owns presentation |
|---|---|---|---|---|---|---|
| 1 | Spawn | `t0` | 0 | no | no | **yes, from creation** |
| 2 | Entrance | `t0` | **0** | discarded | no | yes |
| 3 | Readable hold | `t0` | **0** | discarded | no | yes |
| 4 | Approach + response | `t0` (= `DemoTrialOnsetTime`) | **2500 ms** | **accepted** | **yes** | yes |
| 5 | Outcome lock | resolution instant | instant | closed | no | yes |
| 6 | Persistence | `ScheduledPhase4End` | **0** | discarded | no | yes |
| 7 | Exit | `ScheduledPhase4End` | **350 ms**, existing envelope | discarded | no | yes |
| 8 | Hidden | +350 ms | **0 — same frame** | — | no | yes |
| 9 | Destroy | same frame | — | — | no | **released here** |

`ScheduledPhase4End = t0 + Entrance + Hold + ScoredResponse`. **Fixed,
independent of any response.**

Phase durations are data (`FTMDemoLifecycleTiming`), defaulting to the §4.7
Compatibility column. The spawner always arms `0 / 0 / 2500 / 0 / 350`. **No
A-prime timing is introduced anywhere.**

### 3.3 Phase 8 (Hidden) — what was and was not done

Hidden **exists as a state and is traversed**, and the trace records the
`p7_exit → p8_hidden → p9_destroy` transitions. In Stage 2 it is entered and left
**within one frame**: no `SetVisibility(false)`, no `SetHiddenInGame(true)`, no
deferred destroy. Verified in the capture:

```
VIS_FALSE = 0        HIDDEN_TRUE = 0
DESTROY_REQ on the final MAT_WRITE frame: 40/40   (Stage 1 baseline: 40/40)
```

Giving Hidden a real frame is Spec §4.6 and belongs to Stage 3. Doing it here
would have changed the rendered frame sequence away from the Stage 1 baseline,
which Stage 2 forbids.

### 3.4 Scored resolution vs cancellation — the one deliberate asymmetry

Spec §5 governs *accepted input before `ScheduledPhase4End`* and natural expiry.
It does not govern session cancellation.

- **Scored** (Hit / Commission / Omission / CorrectRejection) → outcome locks
  immediately; the presentation continues to `ScheduledPhase4End`, then
  Persistence → Exit. This is §5, and it is what S2-E / S2-F test.
- **Void** (hard gate, phase change, restart, demo end, teardown) → a
  **cancellation**: the presentation ends promptly, exactly as it does today.

This keeps every existing cancellation behaviour bit-identical and confines the
scheduled-exit change to precisely the paths Stage 2 must change.

### 3.5 Measured phase timeline, schema 2

Early accepted response at RT = 500 ms, identity `(sid=9, spawnseq=0)`:

```
wt=100.000000  SPAWN
wt=100.000000  PHASE p1_spawn -> p2_entrance -> p3_hold -> p4_response   (zero-duration)
wt=100.500000  OUTCOME outcome=Hit                     <- RT = 500 ms, committed once
wt=100.500000  PHASE  -> p5_outcomelock                <- input closes, deregistered
wt=102.500000  PHASE  -> p6_persistence                <- SCHEDULED, +2500 ms. NOT +500 ms
wt=102.500000  PHASE  -> p7_exit
wt=102.847221  PHASE  -> p8_hidden -> p9_destroy
wt=102.847221  DESTROY_REQ, ENDPLAY                    <- visual lifetime 2847.2 ms
```

**Visual lifetime with a 500 ms response: 2847.2 ms. Natural expiry in the
headless capture: 2847.2–2875.0 ms. Identical.** Before Stage 2 this presentation
would have died at 850 ms.

With non-zero durations injected by the input-gating test (Entrance 400,
Hold 200, Persistence 300), every state shows its real dwell:

```
wt=300.000000  p1_spawn -> p2_entrance
wt=300.402771  -> p3_hold          (+402.8 ms  Entrance)
wt=300.611115  -> p4_response      (+208.3 ms  Readable Hold)
wt=303.111115  OUTCOME Omission    (+2500.0 ms Scored Response)
wt=303.111115  -> p5_outcomelock -> p6_persistence
wt=303.402771  -> p7_exit          (+291.7 ms  Persistence)
```

The zero-duration phases are genuinely implemented states, not comments.

---

## 4. Ownership model

```
ActiveTargets          TArray on UTargetSpawnerComponent
                       SCORING ELIGIBILITY. Phase 4 only. Max 1.
                       Registered at Phase 4 entry, deregistered at the outcome
                       lock — both driven by the lifecycle, not by a tick sweep.

LiveDemoPresentation   TWeakObjectPtr<ATranquilMindTargetActor> on the spawner
                       OFFICIAL PRESENTATION. Claimed at creation (Phase 1),
                       released at the end of Phase 9. Max 1.
```

The spawn gate is `HasLiveDemoPresentation()`, replacing
`if (ActiveTargets.Num() > 0) return;`.

**Why a weak pointer.** Release becomes self-clearing: `Destroy()` marks the
actor garbage, so an actor that dies without running its normal path counts as
released. Spec §4.9.1's anti-deadlock assertion — *"a gate that can deadlock the
Demo is worse than the overlap it prevents"* — is satisfied **by construction**
rather than by a cleanup call that could be missed. Test 15 proves it: an
out-of-band `Destroy()` releases ownership and spawning continues.

**The gate delays a spawn; it never cancels one.** It deliberately does not touch
`LastSpawnTimestamp_SEC`, so a latched first-spawn request stays armed and the
tick loop retries every frame.

### Exactly-once cleanup — all 11 paths

| Path | Mechanism | Test |
|---|---|---|
| natural expiry | Phase 9 destroy → weak pointer invalid | headless capture, 39/40 |
| early accepted response | same; presentation still runs to schedule | 1, 5 |
| hard-gate entry | Void → prompt exit → Phase 9 destroy | 11 |
| hard-gate resume | gate refuses until the predecessor is gone; latched retry spawns after | 11, 12 |
| session restart | as above | 12 |
| session end | `DestroyAllActiveTargetsAsVoid()` + explicit `Reset()` | 13 |
| world teardown | explicit `Reset()` in `EndPlay` + weak invalidation | 13 |
| pending-spawn cancellation | never claimed (no actor constructed) | 16 |
| failed spawn | never claimed; pending flag now cleared on **both** failure returns | 14 |
| actor destroyed unexpectedly | weak pointer invalid → released | 15 |
| map transition | world dies → weak invalid; `EndPlay` also resets | 13 |

### Re-entrancy hazards introduced and closed

Moving deregistration into the outcome lock means `ResolveAsVoid()` /
`ResolveAsTriggered()` now mutate `ActiveTargets` from **inside** loops that were
iterating it with `RemoveAtSwap`. Both loops
(`DestroyAllActiveTargetsAsVoid()`, `HandleTriggerPulled()`) were rewritten to
snapshot the array and clear the member first. `AdvancePhases()` carries a
`TGuardValue` re-entrancy guard for the same reason, and `BeginDemoVisualExit()`
was made explicitly one-shot.

---

## 5. Input semantics

Only Phase 4 accepts input. Enforced at two independent levels:

1. **Spawner** — `ActiveTargets` now means exactly "a presentation is in Phase 4
   and input is open", so the existing empty-array branch discards the input at
   the point of receipt.
2. **Actor** — `ResolveAsTriggered()` refuses any input when
   `DemoLifecycle->IsInputOpen()` is false. Defence in depth: a stale array entry
   cannot score outside Phase 4 (S2-G).

Verified for Phases 2, 3, 6 and 7 through **both** the spawner path and a direct
`ResolveAsTriggered()` call: no trial recorded, no Hit, **no commission**, and
nothing buffered or replayed into the subsequent Phase 4.

`ReportSpamInput` is **retained unchanged** from the Stage 1 baseline. It is the
anti-spam watchdog — it records no trial outcome and produces no commission
error — so §4.2's prohibition is satisfied without altering existing behaviour.

### Two early-response defects fixed

Neither was observable in the Stage 1 captures, because all 80 baseline cycles
were natural expiry with **no input at all**.

**D1 — early response shortened the visual lifetime.** The exit began at response
time, so lifetime was `RT + 350 ms` instead of 2850 ms. Fixed by §3.4 above.

**D2 — cadence was a function of reaction time.** `HandleTriggerPulled` ended
with `LastSpawnTimestamp_SEC = TriggerTimestamp_SEC`, re-anchoring the ISI to the
response instant and making the successor spawn at `RT + 3000 ms`. That line is
removed; cadence is anchored solely at `PresentationSpawnTime` (§3, §5 step 8).

Additionally, Demo RT is now measured from `DemoTrialOnsetTime` rather than
`PresentationSpawnTime` (Spec §3). Under Compatibility timing the two coincide,
so this is bit-identical today and stays correct once Stage 4 gives Entrance and
Readable Hold real durations.

---

## 6. C1–C4 dispositions

| | Disposition | Why |
|---|---|---|
| **C1** — 350 ms exit | **RETAINED, ownership MOVED** | The visual form is untouched: same `VisualMotion` envelope, same `ExitDuration_SEC = 0.35`, same three material writes per tick, same scale settle. **Measured identical** — exit span 25–26 frames (347–361 ms) against the Stage 1 baseline's 25–26 frames; exit-start values `CenterOpacity 0.000995`, `EdgeOpacity 0.577332`, `BubbleBrightness 2.289422` byte-identical; final writes exactly `0.000000`. Only the *decision of when it starts* moved, from `FinishResolution()` to the lifecycle's scheduled Phase 7 entry. The Stage 3 replacement (§4.6 compound exit) is **not** implemented |
| **C2** — MID fade | **RETAINED, untouched** | Not relocated: lifecycle ownership did not require it. The MID is still created once at exit begin in `BeginDemoVisualExit()`, still parented to the live `MI_Bubble_*`, still writes the same three scalars. Not redesigned and not diagnosed further, per the Stage 1 disposition (NOT YET PROVEN) |
| **C3** — VisualMotion exit state | **REASSESSED, RETAINED unchanged** | Structural reassessment concluded the current arrangement is correct and load-bearing: the actor-level approach freeze is a product of `SetActorTickEnabled(false)`, which Stage 1 §3.2 proved gives a bit-identical actor transform across every exit frame. **That call is deliberately untouched**, and it is why the phase clock had to live on a component (§3.1). Spec §4.1 independently requires motion frozen from the outcome lock onward, so the existing behaviour already satisfies the new state machine. No change to the motion curve or apparent motion |
| **C4** — progress/completion delegates | **RETAINED, promoted to a lifecycle transition** | `OnVisualExitProgress` still drives the material fade unchanged. `OnVisualExitComplete` is now the **Phase 7 → 8 transition trigger**: it calls `NotifyExitEnvelopeComplete()` instead of `Destroy()` directly, and the lifecycle takes Phases 8 and 9. This is exactly the "future lifecycle phase-transition mechanism" role Stage 1 §6 recommended |

Nothing in C1–C4 was **deleted**.

---

## 7. Test results

**32/32 PASS** (23 pre-existing + 9 new). No test was disabled or skipped.

| Step F item | Test |
|---|---|
| 1 Input accepted only in Phase 4 | `Stage2.EarlyResponseDoesNotShortenLifetime`, `Stage2.InputDiscardedOutsidePhase4` |
| 2 Input discarded in Phases 2, 3, 6, 7 | `Stage2.InputDiscardedOutsidePhase4` |
| 3 First accepted response exactly once | `Stage2.EarlyResponseDoesNotShortenLifetime` |
| 4 Later responses cannot alter outcome | `Stage2.EarlyResponseDoesNotShortenLifetime`, `VisualExit.Lifecycle` (3) |
| 5 Early response does not shorten visual lifetime | `Stage2.EarlyResponseDoesNotShortenLifetime` |
| 6 Expiry and early response share the scheduled exit | `Stage2.ExpiryAndEarlyResponseConverge` |
| 7 `ActiveTargets` only during Phase 4 | `Stage2.InputDiscardedOutsidePhase4`, `VisualExit.Lifecycle` (2) |
| 8 Max `ActiveTargets` = 1 | `Stage2.OwnershipCardinality` |
| 9 Max `LiveDemoPresentation` = 1 | `Stage2.OwnershipCardinality` |
| 10 No successor before predecessor ownership ends | `Stage2.OwnershipCardinality`, `VisualExit.Lifecycle` (6) |
| 11 Hard gate clears ownership exactly once | `Stage2.HardGateAndRestartOwnership` |
| 12 Restart clears ownership and pending spawn safely | `Stage2.HardGateAndRestartOwnership` |
| 13 Session end / teardown leave no stale ownership | `Stage2.TeardownReleasesOwnership` |
| 14 Failed spawn does not permanently block | `Stage2.FailedSpawnDoesNotBlock` |
| 15 Unexpected `EndPlay` releases ownership | `Stage2.TeardownReleasesOwnership` |
| 16 Pending-spawn exactly-once still correct | the 4 pre-existing `Demo.PendingSpawn.*` tests, unchanged and passing |
| 17 Demo touches no Research random stream | `Stage2.DemoDoesNotTouchResearchStream` |
| 18 Research negative controls intact | `Research.Stage2.LifecycleNeverArms` + the 11 pre-existing `Research.*` tests |

### One pre-existing test was deliberately inverted

`TranquilMind.Demo.VisualExit.Lifecycle` assertion (6) previously asserted that a
successor target **could** be created while the predecessor was still visually
fading. That is defect C7 written as an expectation. Spec §4.9 requires the
opposite, so the assertion now requires the spawn to be **refused**, and a new
assertion (6b) proves the successor spawns as soon as the predecessor ends — the
gate delays, never cancels. The stimulus-sequence assertion (7) is retained and
still passes, proving the refused attempt consumed no stimulus draw.

This inversion is the single clearest expression of what Stage 2 changed.

---

## 8. Schema-2 trace results

Headless Environment, 40 cycles, `sid=1`, identity `(sid, spawnseq)`. Full output
in `Documentation/Stage2/evidence/stage2_check_headless.txt`.

```
S2-A  max official live presentations = 1        PASS  max livepresentation = 1
S2-A  never 2 official presentations             PASS  0 samples >= 2
S2-B  max ActiveTargets = 1                      PASS  max activetargets = 1
S2-D  no role-coded target overlap                PASS  0 samples with visiblepresentations >= 2
ID    no identity carries two cycle numbers      PASS  0
ID    no identity carries two auid               PASS  0
S2-C  no predecessor event relabelled            PASS
S2-C  no successor overlaps a predecessor        PASS  0 overlapping identity frame-span pairs
S2-C  no spawn interval < visual lifetime        PASS  min 2861.1 ms vs max lifetime 2875.0 ms
PHASE strictly ordered, no repeats               PASS  0 out of order
PHASE all nine phases traversed                  PASS  40/40 identities
S2-G  no scored event outside Phase 4 / lock     PASS  0 violations
      at most one outcome per presentation       PASS  0 identities with >1 OUTCOME
      at most one destroy per presentation       PASS  0 identities with >1 DESTROY_REQ
TIMING steady-state cadence 3000 ms              PASS  median 3000.0, modal delta 216 (36/39)
TIMING response window 2500 ms                   PASS  n=39 min 2500.0 max 2513.9
TIMING visual lifetime 2850 ms                   PASS  n=39 min 2847.2 max 2875.0
STAGE3 no visibility write introduced            PASS  VIS_FALSE=0 HIDDEN_TRUE=0
```

Across **all 32 automation worlds** (every hard-gate, restart, teardown, failed
spawn and early-response path), `max visiblepresentations = 1`,
`max activetargets = 1`, `max livepresentation = 1`. No session ever reached 2.

### Capture coverage and one honest limitation

| Required capture | How it was obtained |
|---|---|
| Short headless Environment | `-game -nullrhi -benchmark -fps=72`, 40 cycles, `sid=1` |
| Natural expiry | the same capture — 39 scored cycles, no input |
| Early response | **instrumented automation worlds** (`sid=9`, `sid=11`), full schema-2 phase timeline quoted in §3.5 |
| Hard gate / resume | **instrumented automation worlds** (`sid=19`); the pre-existing `SpawnContainment` tests additionally drive the real gaze hard-gate cycle |
| Session teardown | `SESSION_END` with real reasons — `EEndPlayReason::Destroyed` (`sid=4`, `sid=17`), `SupersededByNewWorld` elsewhere |

**Limitation, stated rather than papered over:** the early-response and
hard-gate captures come from instrumented automation worlds, not from a headless
`-game` run. A headless `-unattended` run has no input path and no gaze source,
so it can neither pull a trigger nor trip the gaze hard gate. Adding a debug exec
command to inject input would have been new runtime code outside the Stage 2
scope. The automation worlds run the real spawner, the real actor, the real
lifecycle and the real tracer, and their traces carry full schema-2
`(sid, spawnseq)` identity — but their `GFrameCounter` barely advances, so
frame-delta statistics from them are not meaningful and are not used. World-time
(`wt`) values from them are meaningful and are what §3.5 quotes.

---

## 9. Research regression result

**Layer A — raw-artifact integrity: PASS**

```
A1  raw JSONL preserved unchanged, never rewritten or normalised
A2  schema version, field names, field order, field types, record count
      checked against Documentation/Stage1/baseline/prechange_run_A.raw.jsonl
      -> MATCH (53 records, identical key order and types)
A3  raw sha256 940fcf5913e0e99d8595de2450c4a4d0af914fd541a4e1d8a3b9187cb2705b55
```

**Layer B — canonical deterministic regression: PASS**

```
tool              Documentation/Stage1/baseline/canon_jsonl.py   (unchanged)
register          v1 (2026-07-28), unchanged — no exclusion added

baseline_A canon   6ae4fd8c2e630806b089615c612b03d1cf41b013c5b566da9393960b21ac97eb
baseline_B canon   6ae4fd8c…97eb   (gate validation — projections agree)
post-Stage-1       6ae4fd8c…97eb
STAGE 2 candidate  6ae4fd8c…97eb   <- BYTE-IDENTICAL
```

Seed 20260708, 50 trials. **Stop condition S1 not triggered.** No protected
Research semantic field changed, so no revert is required.

**S6 / S11 not triggered.** No Stage 2 code path reads from, advances or reseeds
a Research stream. The Demo spawn path is behind the operating-mode guard;
`InitializeResearchVisual()` calls `DemoLifecycle->ForceInert()` as an explicit
second layer; `ArmDemoPresentationLifecycle()` refuses on a Research stimulus
even when called directly; and `UTMDemoLifecycleComponent` contains no RNG of any
kind. Test 17 additionally proves that refused spawns, discarded input and early
responses perturb no stimulus draw.

**Assets: none changed.**

```
L_TranquilMind_Environment.umap  2880939a21f8720c…  mtime 04:41  (Part 1 value)
L_TranquilMind_Void.umap         35e3e742b479126a…  mtime 04:47  (Part 1 value)
BP_TranquilMindRuntime.uasset    70731fb2fec395f7…  mtime 2026-05-18
BP_Target.uasset                 unchanged,         mtime 2026-07-27
M_Bubble_Master + 4 instances    unchanged,         mtime 2026-07-22
```

All mtimes predate the start of Stage 2 work (~10:50 today).

---

## 10. Known issues deliberately unchanged

| # | Item | Why it was left alone |
|---|---|---|
| K1 | **Abrupt 350 ms exit; destroy on the final material-write frame; no Hidden frame** | C1 is `REPLACE in Stage 3`. Changing it here would move the rendered frame sequence off the Stage 1 baseline and make any Stage 2 behavioural difference unattributable |
| K2 | **Spec §4.4 spawn suppression** (`spawn + lifetime > SessionEnd`) not implemented | Not in the Stage 2 scope list. It would change the last-trial count and perturb the documented "cycle 40 Void" baseline artefact. Belongs with the Demo session-controller work |
| K3 | **Teardown starts an exit it can never finish.** `EndPlay` → `DestroyAllActiveTargetsAsVoid()` voids the live target, which begins a 350 ms envelope in a dying world | Fixing it would alter the documented force-stop artefact (Blueprint-cleanup report §6) that the headless baseline depends on. It leaks no ownership — `EndPlay` resets explicitly — so it is waste, not a defect. Recorded for the session-controller work |
| K4 | **Phase II boundary latch** (`HandlePhaseChanged()` → `LastSpawnTimestamp_SEC = -1`) still produces one non-modal spawn interval | Pre-existing and out of scope. Stage 2 made it *safer*, not worse: 181 → 206 frames, and the overlap it used to cause is gone |
| K5 | **Go/NoGo discrimination poor; bubble boundary hard to see at distance** | Stages 7 and 6. Observer findings V1/V2, explicitly not Stage 2 |
| K6 | **`ReportSpamInput` on input with no open Phase 4** | Retained exactly as-is. It is a watchdog, not scoring, and changing it would be an unrequested behaviour change |
| K7 | **Duplicated source paths** `Private/Private/Private/…` | Housekeeping, out of scope |
| K8 | **C2 MID fade and C5 terminal flash** | C2 remains NOT YET PROVEN; C5 remains NOT REPRODUCED. Neither was diagnosed further, per instruction |
| K9 | **Void-map early responses now hold to the schedule** | A consequence of applying §5 uniformly on the Demo path. The Void map is a debug map with no exit envelope, so a scored early response there now holds the target frozen to `ScheduledPhase4End` before an immediate destroy. Headless Void captures are unaffected (no input), and the only visible delta is extra `PHASE` log lines |

---

## 11. Human PIE validation — exact steps

Automated validation is complete and passing. This check exists to confirm the
one thing a log cannot: what the eye sees.

### Setup

1. Close any running PIE session.
2. In the Unreal Editor, open **`L_TranquilMind_Environment`**.
3. Confirm the operating mode is Demo. Either leave
   `Config/DefaultGame.ini` at `OperatingMode=Demo` (its current value), or set
   the console variable before pressing Play:

```
tranquilmind.OperatingMode 0
```

4. Press **Play (PIE)**. Let it run **at least 90 seconds** — the session needs
   ~12 s to reach Phase II, and the interesting boundary is the Phase II
   transition, which is where the overlap used to appear.
5. Stop PIE manually.

### What to check — and only this

| # | Check | Expected |
|---|---|---|
| **H1** | **No two-bubble role-coded overlap, at any point** | **PASS = never two bubbles visible at once.** This is the Stage 2 acceptance check. Watch especially around the Phase II transition (~12 s in) and after any gaze hard-gate resume |
| **H2** | Visual appearance otherwise unchanged | Same bubble size, same colours, same approach speed, same spawn distance, same idle float |
| **H3** | Cadence still feels like one bubble every ~3 s | Rhythm unchanged |
| **H4** | Press the trigger / Space **early**, well before a bubble would expire | **The bubble must NOT disappear immediately.** It should freeze in place and remain visible until its normal disappearance time, then fade out as usual. This is the §5 change, and it is the most visible difference from before |
| **H5** | Press the trigger repeatedly during that frozen period | Nothing should happen — no second scoring, no early disappearance |
| **H6** | Blueprint runtime errors | **0 `Accessed None`, 0 other Blueprint runtime errors** in the Output Log |

### Explicitly NOT a Stage 2 failure

Do not report these as Stage 2 problems:

- **The exit still looks abrupt.** C1 is unchanged by design; the compound exit
  is **Stage 3**.
- **Go / NoGo are hard to tell apart.** Observer finding V1 — **Stage 7**.
- **The bubble boundary is hard to see at distance.** Observer finding V2 —
  **Stage 6**.
- **A `Resolved as Void` / `Compliant=NO` tail after stopping PIE manually.**
  That is the documented forced-teardown artefact, present identically in the
  pre-Stage-2 baseline.

### If H1 fails

Stop and report. Overlap remaining would trip stop condition **S9**. Capture the
Output Log and run:

```bash
python3 Documentation/Stage2/tools/stage2_check.py <log> --list-sessions
```

then re-run with `--session <sid>` for the PIE session. An editor log contains
several PIE sessions and the tool **fails closed** rather than merging them.

---

## 12. Artefacts

```
Documentation/Stage2/
  STAGE2_STEP_A_DESIGN_REPORT.md          pre-edit audit and design
  STAGE2_LIFECYCLE_REFACTOR_REPORT.md     this report
  tools/
    stage2_check.py                       schema-2 Stage 2 acceptance checker
  evidence/
    stage2_headless_environment_capture.log   40 cycles   0abb9f71c2954fcc…
    stage2_check_headless.txt                 checker output (PASS)  2b85af0b93f4ebc5…
    stage2_automation_capture.log             32-test trace          3df30df3161a0b20…
    stage2_automation_sessions.txt            session enumeration    855e69d799a4477d…
    stage2_research.raw.jsonl                 raw, preserved         940fcf5913e0e99d…
    stage2_research.canon.jsonl               canonical projection   6ae4fd8c…97eb
```

`Documentation/Stage1/` — kit, archived traces, parser and fixtures — is
**unmodified**. Instrumentation stays in place for Stage 3.

```
STAGE 2 COMPLETE.  STAGE 3 NOT STARTED.
Overlap fixed and measured.  Research byte-identical.  No assets touched.
Not committed.  Not packaged.
```
