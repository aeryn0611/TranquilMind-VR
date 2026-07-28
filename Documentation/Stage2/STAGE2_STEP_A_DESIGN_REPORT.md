# STAGE 2 — STEP A: LIFECYCLE / OWNERSHIP AUDIT AND REFACTOR DESIGN

**Date:** 2026-07-28
**Authority:** `Documentation/TranquilMind_Implementation_Spec.md` (Revision 2)
**Status:** design complete — no code written at the time this file was authored.

Every claim below is read from the current working tree, with file and line
references. Where Stage 1 runtime evidence exists it is cited; where a statement
is a code reading only, it is labelled as such.

---

## 1. Existing lifecycle and resolution states

There is **no explicit state machine.** `ATranquilMindTargetActor` carries its
state in three booleans plus two component flags:

| Carrier | File | Meaning |
|---|---|---|
| `bResolved` | `TranquilMindTargetActor.h:72` | one-way latch; gates all three resolve entry points and every selection loop |
| `bHadResponse` | `:75` | set only by `ResolveAsTriggered` |
| `bStaticResearchStimulus` | `:79` | Research: `Tick` returns immediately, actor never self-scores |
| `UTMVisualMotionComponent::bMotionActive` | `TMVisualMotionComponent.h:206` | presentation rig armed (Environment Demo profile only) |
| `UTMVisualMotionComponent::bExiting` | `:227` | 350 ms exit envelope running |

Observed Demo-Environment sequence, one presentation:

```
SpawnActor                         TargetSpawnerComponent.cpp:746
  BeginPlay  -> trace SPAWN        TranquilMindTargetActor.cpp:118
  InitializeTarget                 :207   material, BeginMotion, TEMP overrides
  spawner pushes Demo overrides    TargetSpawnerComponent.cpp:770-827
  ActiveTargets.Add                :829                    <-- registration at PHASE 1
[ live: actor Tick integrates approach; expiry checked每 tick ]
  Resolve{AsTriggered|AsVoid|ExpiredResponseWindow}         <-- outcome recorded
  FinishResolution                 :479   SetActorTickEnabled(false)
  BeginDemoVisualExit              :520   MID create, bind 2 delegates, BeginVisualExit
[ 350 ms: component tick -> OnVisualExitProgress -> 3 material writes/frame ]
  OnVisualExitComplete -> HandleVisualExitComplete :578 -> Destroy()
  EndPlay -> trace ENDPLAY
```

Mapped onto Spec §4.1:

| Spec phase | Present today? |
|---|---|
| 1 Spawn | implicit, same frame as everything before Phase 4 |
| 2 Entrance | **absent** |
| 3 Readable hold | **absent** |
| 4 Approach + response | the entire pre-resolution lifetime |
| 5 Outcome lock | the interior of `Resolve*()` + `FinishResolution()` |
| 6 Persistence | **absent** |
| 7 Exit | the 350 ms `VisualMotion` envelope |
| 8 Hidden | **absent** (Stage 1 §3.1: zero `VIS_FALSE`, zero `HIDDEN_TRUE`) |
| 9 Destroy | same frame as the final material write (Stage 1: separation `{0}` frames, 118/118 cycles) |

---

## 2. Exact ownership today

| Concern | Owner | Notes |
|---|---|---|
| **Actor lifetime** | **split.** Created by `UTargetSpawnerComponent::SpawnNextTarget()` (Demo) or `UTMResearchRunner` (Research). **Destroyed by the actor itself** — `FinishResolution():511` or `HandleVisualExitComplete():583`. Research destroys via `UTMResearchRunner::DestroyStimulusActor():327` | the spawner never destroys a Demo target |
| **`ActiveTargets`** | `UTargetSpawnerComponent` exclusively (`TargetSpawnerComponent.h:47`) | public `UPROPERTY`, read by tests |
| **Visual exit** | **split three ways.** Timing + scale: `UTMVisualMotionComponent` (`bExiting`, `ExitElapsed_SEC`, `ExitDuration_SEC = 0.35`). Material: `ATranquilMindTargetActor::HandleVisualExitFade`. Start decision: `FinishResolution()` | |
| **Pending spawn** | `UTargetSpawnerComponent` (`bDemoSpawnPendingFrame`, `bSpawnCallInProgress`, `bLoggedPendingSpawnWait`) | `UTMDemoSessionFrameSubsystem` only broadcasts `OnDemoFrameCaptured`; it never spawns |
| **Hard gate** | `ATranquilMindSessionManager` (`TriggerHardGate` / `ResumeFromHardGate`) | spawner reacts through `HandleInterruptStateChanged` and `HandleHardGateResumed` |
| **Session end** | **two independent authorities.** Demo timer (`bDemoEnded`, `TickComponent:257-270`) and the session HFSM (`Phase_III_Cooldown` / `Phase_Terminated`) | both funnel to `DestroyAllActiveTargetsAsVoid()` |
| **`LiveDemoPresentation`** | **does not exist** | this absence is the root of C7 |

---

## 3. Every path that can mutate the lifecycle

### 3.1 Add to `ActiveTargets` — 1 site
`SpawnNextTarget():829`, unconditionally after a successful `SpawnActor`.

### 3.2 Remove from `ActiveTargets` — 3 functions, 7 statements

| Site | Condition | Callers |
|---|---|---|
| `CleanupResolvedTargets():863` | `!IsValid \|\| IsResolved()` | `TickComponent:253`, `HandleTriggerPulled:450`, `SpawnNextTarget:591` |
| `DestroyAllActiveTargetsAsVoid():371,374` | unconditional | `EndPlay:222`, `TickComponent:265` (demo end), `TickComponent:282` (cooldown/terminated), `RestartTrial:350`, `RegisterSessionManager:564`, `HandleInterruptStateChanged:880`, `HandlePhaseChanged:898` |
| `HandleTriggerPulled():488,494,499,502` | invalid / already resolved / just resolved | VR pawn trigger, `TranquilMindVRPawn.cpp:133` |

**The load-bearing fact (C7):** `CleanupResolvedTargets()` prunes on
`IsResolved()`, which is true from the resolution instant — roughly **350 ms
before the actor stops rendering.**

### 3.3 Resolve an outcome — 3 entry points, all `bResolved`-guarded

| Function | Trigger | Outcome |
|---|---|---|
| `ResolveAsTriggered():346` | user input via `HandleTriggerPulled` | Hit / Commission |
| `ResolveExpiredResponseWindow():444` | actor `Tick` at `Elapsed >= ResponseWindow_MS` | Omission / CorrectRejection |
| `ResolveAsVoid():393` | `DestroyAllActiveTargetsAsVoid()` — hard gate, phase change, restart, demo end, teardown | Void |

Research never reaches these: `InitializeResearchVisual` sets
`SessionManager = nullptr` and `bStaticResearchStimulus = true`, and `Tick`
early-returns.

### 3.4 Start exit — 1 site
`FinishResolution():499 -> BeginDemoVisualExit():520 -> VisualMotion->BeginVisualExit()`.
`BeginVisualExit()` is idempotent while running; `BeginDemoVisualExit()` is
**not** (it re-creates the MID and re-binds both delegates with `AddUObject`).
Unreachable twice today because `bResolved` is one-way — a latent hazard, not a
live defect.

### 3.5 Destroy an actor — 4 sites
`FinishResolution():511` (no-presentation path), `HandleVisualExitComplete():583`,
`UTMResearchRunner::DestroyStimulusActor():327` (Research), plus engine world
teardown. The first two are mutually exclusive.

### 3.6 Request a next spawn — 4 sites
`TickComponent:299` (the `LastSpawnTimestamp_SEC < 0` branch — fires **every
tick** while latched), `TickComponent:310` (ISI elapsed), `RestartTrial:356`,
`HandleDemoFrameCaptured:935`. `SpawnNextTarget` is `BlueprintCallable`; after
the Part 1 cleanup there are zero Blueprint call sites.

### 3.7 Latch a pending next spawn (`LastSpawnTimestamp_SEC = -1.0f`) — 4 sites
`HandleInterruptStateChanged:881`, `HandlePhaseChanged:890` and `:899`,
`RestartTrial:352`. Stage 1 §3.3 proved the Phase II boundary latch
(`HandlePhaseChanged`) is what created the pending request in the anomalous
181-frame interval.

---

## 4. Existing progress / completion delegates

```
FTMVisualExitProgress OnVisualExitProgress   broadcast TMVisualMotionComponent.cpp:155
FTMVisualExitComplete OnVisualExitComplete   broadcast :173   (bExiting cleared first)
```

Bound in `BeginDemoVisualExit():546-549` with `AddUObject`; never explicitly
unbound (the actor dies with them). Stage 1 §6 C4: **verified functional** — 118
completions, one per cycle, each preceded by a monotone progress series
terminating at exactly 1.0.

---

## 5. Current early-response path

```
ATranquilMindVRPawn::HandleTriggerPressed         TranquilMindVRPawn.cpp:114
  -> UTargetSpawnerComponent::HandleTriggerPulled  TargetSpawnerComponent.cpp:377
       Research guard -> runner, return
       phase gate: 1B_Baseline / II_CoreTraining only
       interrupt gate: SysAbort always; HardGate only when !bDebugHardwareMode
       debounce DEBOUNCE_MIN_MS -> ReportSpamInput, return
       CleanupResolvedTargets()
       if ActiveTargets.Num() <= 0 -> ReportSpamInput, return
       TEMP DEBUG: resolve ALL active targets, remove each, Reset()
       LastSpawnTimestamp_SEC = TriggerTimestamp_SEC          <-- see D2
  -> ResolveAsTriggered  RT = (Trigger - SpawnTimestamp) * 1000
  -> FinishResolution -> BeginDemoVisualExit    <-- exit begins AT RESPONSE TIME
```

Two Stage-2-blocking defects, neither observable in the Stage 1 captures because
**those captures had no input at all** (80/80 cycles natural expiry):

**D1 — early response shortens the visual lifetime.**
Total visual lifetime becomes `RT + 350 ms` instead of `2500 + 350 = 2850 ms`.
Directly violates Spec §5 steps 6–8 and trips **S2-E / stop condition S10**.

**D2 — cadence becomes a function of reaction time.**
`LastSpawnTimestamp_SEC = TriggerTimestamp_SEC` (`:505`) re-anchors the ISI to
the response instant, so the successor spawns at `RT + 3000 ms` after the
predecessor's spawn. Spec §3 defines Cadence as
`PresentationSpawnTime(n+1) − PresentationSpawnTime(n)`, and §5 step 8 requires
it to be independent of reaction speed.

**Not a defect (recorded so it is not "fixed" by accident):** the
`ActiveTargets.Num() <= 0 -> ReportSpamInput` branch. `ReportSpamInput` is the
anti-spam watchdog; it is not scoring and not a commission error, so §4.2 is
satisfied. After the refactor `ActiveTargets` is exactly "input is open", so the
existing branch keeps its current meaning and its current behaviour.

---

## 6. Current natural-expiry path

`ATranquilMindTargetActor::Tick:174-184` compares
`(World->GetTimeSeconds() - SpawnTimestamp_SEC) * 1000 >= ResponseWindow_MS`
and calls `ResolveExpiredResponseWindow()`. Measured effective window
**2500 ms** (180–181 frames at 72 fps; the 181 is one frame of quantisation —
Blueprint-cleanup report §6). The clock is anchored at `SpawnTimestamp_SEC`,
which under Compatibility timing is simultaneously `PresentationSpawnTime` and
`DemoTrialOnsetTime` (Entrance + Hold = 0).

---

## 7. Current hard-gate and restart cleanup

```
HandleInterruptStateChanged(HardGate_GazeLost|SysAbort, true)
    -> DestroyAllActiveTargetsAsVoid();  LastSpawnTimestamp_SEC = -1
HandleHardGateResumed() -> RestartTrial()
    -> DestroyAllActiveTargetsAsVoid();  LastSpawnTimestamp_SEC = -1
    -> if (CanSpawnTargetsNow()) SpawnNextTarget()
RegisterSessionManager() -> if hard gate already active -> DestroyAllActiveTargetsAsVoid()
```

**Defect H — the function's name is false on the Environment profile.**
`DestroyAllActiveTargetsAsVoid()` does not destroy: `ResolveAsVoid()` routes
through `FinishResolution()` into `BeginDemoVisualExit()`, so the target starts a
**350 ms exit** and is merely dropped from the array. `RestartTrial()` then calls
`SpawnNextTarget()` immediately, which sees `ActiveTargets.Num() == 0` and spawns
into the predecessor's live exit. This is C7 on the hard-gate-resume path, and
there it is **deterministic**, not a race.

Any target that was already exiting is simply not in the array at all — nothing
in the project tracks it.

---

## 8. Current pending-spawn handshake

State: `bDemoSpawnPendingFrame` (one coalesced request), `bSpawnCallInProgress`
(`TGuardValue` reentrancy guard, `:623`), `bLoggedPendingSpawnWait`.
Flow: `SpawnNextTarget` resolves frame availability **before**
`ChooseNextStimulusType()` so a deferred attempt cannot consume a stimulus draw;
on `!bFrameReady` it records one request and constructs nothing. The subsystem's
`OnDemoFrameCaptured` (valid pose **or** bounded-wait timeout) reaches
`HandleDemoFrameCaptured():908`, which clears the flag **before** spawning.
Covered by 4 automation tests.

**Gap P1 — the consume path has no live-presentation gate.**
`HandleDemoFrameCaptured -> SpawnNextTarget` relies on the same
`ActiveTargets.Num() > 0` check.

**Gap P2 — the flag leaks on the two early-return failure paths.**
`TargetActorClass == nullptr` (`:606-616`) and `!IsValid(SpawnedTarget)`
(`:755-763`) both return with `bDemoSpawnPendingFrame` still `true`, while the
frame is already captured — so no further `OnDemoFrameCaptured` will ever fire to
consume it. Harmless today (the flag is only read in `HandleDemoFrameCaptured`)
but it is stale state, and Stage 2 test 14 asks about exactly this.

---

## 9. Current teardown behaviour

`UTargetSpawnerComponent::EndPlay:182-234` clears `bDemoSpawnPendingFrame`,
removes the `OnDemoFrameCaptured` subscription, shuts down the Research runner,
unbinds four `SessionManager` delegates, calls `DestroyAllActiveTargetsAsVoid()`,
clears the tracer provider and emits `SESSION_END`.

**Defect T — teardown starts an exit it can never finish.**
`DestroyAllActiveTargetsAsVoid()` at `EndPlay` voids the live target, which binds
two delegates and starts a 350 ms envelope inside a world that is being torn
down. The Void outcome recorded there is the documented force-stop artefact
("cycle 40 resolves at 103 frames as Void", Blueprint-cleanup report §6) and must
be **preserved**; the exit start is what must not happen.

`ATranquilMindTargetActor::EndPlay:143` emits the trace line and calls `Super`.
Nothing else.

---

## 10. Paths that can resolve or destroy twice

| Question | Answer | Evidence |
|---|---|---|
| Resolve twice? | **No.** All three entry points early-return on `bResolved`, which is one-way | code; and `TranquilMind.Demo.VisualExit.Lifecycle` assertion (3) |
| Destroy twice? | **No.** The two Demo destroy sites are mutually exclusive; `OnVisualExitComplete` fires once (`bExiting` cleared before the broadcast); UE `Destroy()` is itself idempotent | `TMVisualMotionComponent.cpp:166-173` |
| Re-enter the exit? | **Latent only.** `BeginDemoVisualExit()` is not idempotent (re-creates the MID, re-binds delegates) but is unreachable twice behind `bResolved` | `TranquilMindTargetActor.cpp:520-552` |
| Resolve re-entering the spawner? | **No.** `RecordTrialOutcome` (`TranquilMindSessionManager.cpp:322-379`) only mutates counters and broadcasts nothing | verified by reading every `Broadcast` site in the session manager |

**New hazard introduced by the planned refactor, and mitigated in the design:**
once deregistration moves into `FinishResolution()`, `DestroyAllActiveTargetsAsVoid()`
and `HandleTriggerPulled()` would mutate `ActiveTargets` **re-entrantly while
iterating it with `RemoveAtSwap`**. Both loops are therefore rewritten to take a
snapshot and clear the member array first.

---

## 11. The proposed refactor — smallest change that satisfies §4.1, §4.9 and §5

### 11.1 Why a new component rather than the actor tick

The phase clock must run **after** `FinishResolution()` calls
`SetActorTickEnabled(false)`. Stage 1 §3.2 proved the actor-level approach freeze
during exit is a *consequence* of that call — the actor transform is bit-identical
across all 26 exit frames. Keeping the actor tick alive to host the phase clock
would require re-gating travel inside `Tick()` and would put the proven freeze at
risk, which C3 forbids in Stage 2.

`UTMVisualMotionComponent` already ticks across the whole presentation, but its
header states an explicit architecture contract: it is a **generic, reusable**
motion rig that "never reads gameplay state". Loading a Demo-trial phase machine
into it violates that contract.

**Decision: a new Demo-only component**, `UTMDemoLifecycleComponent`, matching
Spec §14's "New Demo-only lifecycle component — owns Phases 2, 3, 6, 7, 8. No
access to R1–R7". It ticks independently of the actor tick, exactly as
`UTMVisualMotionComponent` does, so `SetActorTickEnabled(false)` and the proven
approach freeze are untouched.

### 11.2 Phase timeline at Compatibility timing

All offsets from `PresentationSpawnTime` (`t0`). Durations are data
(`FTMDemoLifecycleTiming`), defaulting to the §4.7 Compatibility column.

| Phase | Enters at | Duration | Input | In `ActiveTargets` | Owns `LiveDemoPresentation` |
|---|---|---|---|---|---|
| 1 Spawn | `t0` | 0 | no | no | **yes, from creation** |
| 2 Entrance | `t0` | **0** | discarded | no | yes |
| 3 Readable hold | `t0` | **0** | discarded | no | yes |
| 4 Approach + response | `t0` (= `DemoTrialOnsetTime`) | **2500 ms** | **accepted** | **yes** | yes |
| 5 Outcome lock | resolution instant (early) **or** `ScheduledPhase4End` | instant | closed | no | yes |
| 6 Persistence | `ScheduledPhase4End` | **0** | discarded | no | yes |
| 7 Exit | `ScheduledPhase4End` | **350 ms**, existing envelope | discarded | no | yes |
| 8 Hidden | `ScheduledPhase4End + 350` | **0 — same frame** | — | no | yes |
| 9 Destroy | same frame | — | — | no | **released here** |

`ScheduledPhase4End = t0 + Entrance + Hold + ScoredResponse = t0 + 2500 ms`,
**fixed, independent of any response.**

Phase 8 is entered and left inside one call chain: no `SetVisibility(false)`, no
`SetHiddenInGame(true)`, no deferred destroy. **The rendered frame sequence is
byte-for-byte what it is today** — Stage 3 is what gives Hidden a real frame.
This is what "Hidden may exist structurally" means here.

### 11.3 Scored resolution vs cancellation — the one deliberate asymmetry

Spec §5 governs *"valid input accepted before `ScheduledPhase4End`"* and natural
expiry. It does not govern session cancellation.

- **Scored resolutions** (Hit, Commission, Omission, CorrectRejection) →
  outcome locks immediately, presentation continues to `ScheduledPhase4End`,
  then Persistence → Exit. This is §5, and it is what S2-E / S2-F test.
- **Void** (hard gate, phase change, restart, demo end, world teardown) → is a
  **cancellation**. Outcome locks and the presentation ends promptly, exactly as
  today: exit envelope if armed, immediate destroy if not.

This keeps every existing cancellation behaviour bit-identical, keeps the Void
map and the `VisualExit.InertWithoutPresentation` test unchanged, and confines
the scheduled-exit change to precisely the paths Stage 2 is required to change.

### 11.4 Ownership split

```
ActiveTargets          TArray on UTargetSpawnerComponent — Phase-4 scoring eligibility
LiveDemoPresentation   TWeakObjectPtr<ATranquilMindTargetActor> on UTargetSpawnerComponent
```

- **Claimed** immediately after a successful `SpawnActor` (Phase 1), before
  `InitializeTarget`.
- **Released** at end of Phase 9. A weak pointer makes this automatic and
  **self-clearing**: `Destroy()` marks the object garbage, so `IsValid()` is
  false from that instant. An actor that dies without running its normal path
  therefore counts as released — Spec §4.9.1's anti-deadlock assertion is
  satisfied *by construction*, not by a cleanup call that might be missed.
- **Spawn gate** becomes `HasLiveDemoPresentation()`
  (`IsValid() && !IsActorBeingDestroyed()`), replacing
  `if (ActiveTargets.Num() > 0) return;` at `SpawnNextTarget():599`.
- `ActiveTargets` registration moves to **Phase 4 entry**, deregistration to
  **Outcome lock inside `FinishResolution()`** — so removal is immediate on all
  three resolve paths, including early response (§5 step 4).
  `CleanupResolvedTargets()` is retained as a safety net and is no longer
  load-bearing.

**Exactly-once cleanup, all 11 paths:**

| Path | Mechanism |
|---|---|
| natural expiry | Phase 9 destroy → weak pointer invalid |
| early accepted response | same — presentation still runs to schedule |
| hard-gate entry | Void → prompt exit → Phase 9 destroy |
| hard-gate resume | gate refuses until the predecessor is gone; the `LastSpawnTimestamp_SEC < 0` retry (already fires every tick) spawns within ≤350 ms |
| session restart | same as hard-gate resume |
| session end | `DestroyAllActiveTargetsAsVoid()` + explicit `Reset()` |
| world teardown | explicit `Reset()` in `EndPlay`, plus weak-pointer invalidation |
| pending-spawn cancellation | ownership never claimed (no actor was constructed) |
| failed spawn | ownership never claimed; pending flag now cleared on both failure returns (fixes Gap P2) |
| actor destroyed unexpectedly | weak pointer invalid → released |
| map transition | world dies → weak pointer invalid; `EndPlay` also resets |

### 11.5 Cadence

Delete `LastSpawnTimestamp_SEC = TriggerTimestamp_SEC` (`HandleTriggerPulled:505`).
Cadence is then anchored solely at `PresentationSpawnTime` in
`SpawnNextTarget():753`, giving a fixed 3000 ms independent of reaction speed
(§3, §5 step 8). The comment's original intent — "let the eye see it disappear" —
is now served by the presentation running its full scheduled lifetime.

### 11.6 Instrumentation

- Phase transitions emit the existing `FTMStage1Trace::LogPhase(Actor, From, To)`
  — no tracer change needed for those, and every line already carries
  `(sid, spawnseq)`.
- One tracer addition, log-only: a `LiveDemoPresentation` provider mirroring the
  existing `SetActiveTargetsProvider` pattern, so `SNAP` and `NEXT_SPAWN` can
  record `livepresentation` alongside `activetargets` and `visiblepresentations`.
  This is what validates **S2-A** directly rather than by inference.
- The hardened parser `parse_tm_trace_v2.py` and its 27 fixtures are **not
  modified.** Stage 2 validation uses a new
  `Documentation/Stage2/tools/stage2_check.py`.

### 11.7 Deliberately NOT done in Stage 2

| Item | Why |
|---|---|
| Spec §4.4 spawn suppression (`spawn + lifetime > SessionEnd`) | not in the Stage 2 scope list; it would change the last-trial count and perturb the Stage 1 headless baseline (the documented "cycle 40 Void" artefact). Deferred to the Demo session-controller work |
| Stage 3 compound exit, Hidden frame, deferred destroy | Stage 3 |
| A-prime timing | Stage 4 |
| Motion curve, spawn distance, scale, materials, Go/NoGo identity | Stages 5/6/7 |
| Repairing the duplicated `Private/Private/Private/` source paths | housekeeping, out of scope |

### 11.8 Files to change

| File | Change |
|---|---|
| **NEW** `Public/TMDemoLifecycleComponent.h`, `Private/TMDemoLifecycleComponent.cpp` | the 9-phase machine |
| `Public/Private/Private/TranquilMindTargetActor.h` / `Private/Private/Private/TranquilMindTargetActor.cpp` | host the lifecycle component; owning-spawner weak ref; input-open guard on `ResolveAsTriggered`; deregistration at outcome lock; exit start moved to scheduled Phase 7 |
| `Public/TargetSpawnerComponent.h` / `Private/TargetSpawnerComponent.cpp` | `LiveDemoPresentation` + gate; Phase-4 register/deregister API; re-entrancy-safe array loops; remove the RT cadence re-anchor; clear the pending flag on both failure returns |
| `Public/TMStage1Trace.h` / `Private/TMStage1Trace.cpp` | log-only live-presentation provider |
| **NEW** `Private/Tests/TMStage2LifecycleTest.cpp` | the 18 required Step F tests |
| `Private/Tests/TMVisualExitTest.cpp` | assertion (6) inverts — spawning during a predecessor's exit must now be **refused**. This is the C7 fix and is recorded prominently |

**Research pipeline: not touched.** No file under `TMResearchRunner`,
`TMSequenceGenerator`, `TMTrialLogger`, `TMResearchSettings` or
`TMResearchConfig` is modified, and no new code path reaches a Research random
stream.
