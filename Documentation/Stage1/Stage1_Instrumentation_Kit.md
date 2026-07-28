# TranquilMind — Stage 1 Instrumentation Kit

**Authority:** subordinate to `TranquilMind_Implementation_Spec.md`. This kit implements Spec §11 only.

**Status: NOT EXECUTED.** The UE5 project is not reachable from the session that produced this kit. No cycles have been captured. No findings exist. §7 is a template with blanks, not a report.

**Purpose:** make Stage 1 mechanical. Drop in the tracer, run three natural-expiry cycles in Environment PIE, run the parser, and the report tables fill themselves. Then apply §6's decision rules to the captured evidence.

---

## 0. Stage 1 boundary — what this kit must not do

Log-only. Read-only. Adds no behaviour.

**Must not change:** visual timing · fade curve · MID creation or assignment · motion · actor destruction · visibility order · cadence · target registration · material assets · any Research behaviour (R1–R7).

Three rules that make this non-negotiable in code:

| # | Rule | Why |
|---|---|---|
| **N1** | **Never call `CreateDynamicMaterialInstance`** in tracer code | MID creation is the leading flash hypothesis. Creating one to inspect it would manufacture the artefact being measured |
| **N2** | **Never call any setter** — no `SetScalarParameterValue`, `SetVisibility`, `SetHiddenInGame`, `SetRelativeTransform`, `SetActorLocation`, `Destroy` | Read accessors only |
| **N3** | **Never draw from any random stream** | Protects R5 and R7 |

The tracer reads the *existing* material via `GetMaterial(0)` and casts to `UMaterialInstanceDynamic`. If the cast fails, that is itself a finding — log it, do not fix it.

---

## 1. Prerequisites — read the project before instrumenting

Locate and record these before adding a single log line. **This kit cannot name them; only the project can.**

| Slot | What to find | Record here |
|---|---|---|
| `P1` | Active target actor class (the bubble) | `________` |
| `P2` | Mesh component on P1 | `________` |
| `P3` | VisualMotion component or subobject (C3) | `________` |
| `P4` | Exit driver — whatever ticks the 350 ms fade (C1) | `________` |
| `P5` | Every call site of `CreateDynamicMaterialInstance` (C2) | `________` |
| `P6` | Progress delegate (C4) — declaration + broadcast site | `________` |
| `P7` | Completion delegate (C4) — declaration + broadcast site | `________` |
| `P8` | Spawner / session controller that decides cadence | `________` |
| `P9` | Target array — the actual container (`ActiveTargets`) and its add/remove sites | `________` |
| `P10` | Every `Destroy()` / pool-return call site | `________` |
| `P11` | Exit-controlled scalar parameter names (opacity, rim width, rim intensity, scale, any other) | `________` |
| `P12` | Does an object pool exist at all? Class name, or "none found" | `________` |

`P12` is a Stage 1 question (§6.4), not a prerequisite answer. Record what you *see*; the log decides.

---

## 2. Files in this kit

| File | Role |
|---|---|
`TMStage1Trace.h` / `.cpp` | Log-only tracer. Static functions, no state mutation |
`parse_tm_trace.py` | Turns the raw log into the Spec-required tables |
| this document | Call sites, capture protocol, decision rules, report template |

---

## 3. Log schema

One event per line, pipe-delimited `key=value`, prefixed `TMS1` so a single `grep TMS1` extracts the whole trace.

```
TMS1|f=<GFrameCounter>|rt=<RealTimeSeconds>|wt=<WorldTimeSeconds>|ev=<EVENT>|
     aname=<actor name>|auid=<actor UniqueID>|cyc=<cycle index>|<event-specific pairs>
```

### 3.1 Event vocabulary

| `ev=` | Emit at | Required extra pairs |
|---|---|---|
| `SPAWN` | Actor construction / BeginPlay of P1 | `matiface`, `midvalid`, `miduid`, `midcreated` |
| `MID_CREATE` | **Every** P5 call site, immediately after the call | `miduid`, `caller` |
| `PHASE` | Any existing phase/state transition in P4 or P3 | `from`, `to` |
| `OUTCOME` | Outcome resolution / scoring commit | `outcome`, `rt_ms` |
| `EXIT_BEGIN` | First frame of the 350 ms fade | — |
| `EXIT_70` | Computed 70% point of the fade | `elapsed_ms` |
| `MAT_WRITE` | **Every** scalar write to the MID by P4 | `param`, `value` |
| `VIS_FALSE` | `SetVisibility(false)` on P2 | `caller` |
| `HIDDEN_TRUE` | `SetHiddenInGame(true)` on P2 or actor | `caller` |
| `DELEGATE_PROGRESS` | P6 broadcast | `alpha` or whatever it carries |
| `DELEGATE_COMPLETE` | P7 broadcast | — |
| `DESTROY_REQ` | Every P10 call site, immediately **before** the call | `caller`, `mode` (`destroy` or `pool`) |
| `ENDPLAY` | P1 `EndPlay` override | `reason` |
| `NEXT_SPAWN` | P8 decides to spawn the next target | `prev_auid` |
| `SNAP` | Every frame from `EXIT_BEGIN − 5` to `ENDPLAY + 5` | see §3.2 |

### 3.2 `SNAP` payload — the per-frame state record

```
axform=<actor world transform>
mxform=<mesh world transform>
vmrel=<VisualMotion relative transform>
apdist=<approach-axis distance, cm>
matiface=<material interface name>
midvalid=<0|1>
miduid=<MID UniqueID, or -1>
sc:<Param>=<value>          one pair per P11 scalar, read-only
meshvis=<0|1>
meshhidden=<0|1>
actorhidden=<0|1>
beingdestroyed=<0|1>
activetargets=<ActiveTargets count>
visiblepresentations=<count of ALL Demo target presentations with a visible mesh>
```

`visiblepresentations` is the field that settles §6.3. It counts every Demo target actor in the world whose mesh is currently visible — not just the one being traced.

---

## 4. Call sites

Insert exactly these. Nothing else. Every insertion is a single log statement.

| # | Location | Statement |
|---|---|---|
| 1 | P1 `BeginPlay` | `FTMStage1Trace::LogSpawn(this, Mesh)` |
| 2 | **Each** P5 site, immediately after the create call | `FTMStage1Trace::LogMidCreate(this, Mesh, TEXT("<call site label>"))` |
| 3 | Each P3/P4 state transition | `FTMStage1Trace::LogPhase(this, TEXT("<from>"), TEXT("<to>"))` |
| 4 | Outcome commit | `FTMStage1Trace::LogOutcome(this, TEXT("<outcome>"), RtMs)` |
| 5 | First frame of fade | `FTMStage1Trace::LogExitBegin(this)` — also starts the SNAP window |
| 6 | Fade tick, when `elapsed >= 0.70 * duration` and not yet logged | `FTMStage1Trace::LogExit70(this, ElapsedMs)` |
| 7 | **Each** scalar write in P4 | `FTMStage1Trace::LogMatWrite(this, ParamName, Value)` — log **after** the existing write, do not replace it |
| 8 | Each `SetVisibility(false)` | `FTMStage1Trace::LogVisFalse(this, TEXT("<call site>"))` |
| 9 | Each `SetHiddenInGame(true)` | `FTMStage1Trace::LogHiddenTrue(this, TEXT("<call site>"))` |
| 10 | P6 broadcast | `FTMStage1Trace::LogProgress(this, Alpha)` |
| 11 | P7 broadcast | `FTMStage1Trace::LogComplete(this)` |
| 12 | **Each** P10 site, immediately **before** the call | `FTMStage1Trace::LogDestroyReq(this, TEXT("<call site>"), TEXT("destroy"/"pool"))` |
| 13 | P1 `EndPlay` | `FTMStage1Trace::LogEndPlay(this, EndPlayReason)` |
| 14 | P8, at the spawn decision | `FTMStage1Trace::LogNextSpawn(World, PrevAuid)` |
| 15 | P1 `Tick`, unconditionally | `FTMStage1Trace::TickSnapshot(this, Mesh, VisualMotion)` — internally no-ops outside the SNAP window |

**Site 7 matters most.** If the fade writes scalars from more than one place, or writes to a MID it did not create, only per-write logging will show it.

**Site 12 must be before the call.** After `Destroy()` the actor may be unusable for logging.

---

## 5. Capture protocol

1. Confirm `TMS1` output goes to the log file, not only the on-screen console. Set the category verbosity to `Log`.
2. Launch **Environment PIE**. Not standalone, not device — Spec §11 specifies desktop PIE because that is where C5 reproduces.
3. Let the Demo run **at least four** natural-expiry cycles. Capture three; the extra guards against a first-cycle warm-up artefact.
4. **Natural expiry only.** No manual input, no early resolution, no pausing, no editor focus change, no viewport resize mid-capture. Any of those invalidates the frame deltas.
5. Note whether the flash is visible on each captured cycle, and on which cycle index. If it appears on cycle 1 only, or from cycle 2 onward, that alone discriminates between hypotheses in §6.1.
6. Save the log. Run:

```
python3 parse_tm_trace.py <log file> --out stage1_report_tables.md
```

7. Run the fixed-seed Research regression. Diff JSONL against the pre-Stage-1 baseline. **Byte-identical required.** If it is not, the tracer is touching protected state — revert and find out why before continuing.
8. **Remove no instrumentation.** It is needed for Stages 2 and 3.

---

## 6. Decision rules — log signature → finding

Apply these to captured data. Do not pre-fill an answer.

### 6.1 Why does the terminal flash occur?

| Signature in the log | Indicated cause |
|---|---|
| `MID_CREATE` appears **once per cycle**, and `miduid` in `SNAP` **changes between cycles**, and the flash frame shows `sc:Opacity` back at its parent default | **MID replacement on reuse.** Leading hypothesis. A fresh MID renders one frame at parent defaults before the first scalar write |
| Last `MAT_WRITE` frame is **after** `VIS_FALSE` frame | **Ordering fault.** A write lands on an already-hidden component, then visibility returns for one frame |
| `VIS_FALSE` and `HIDDEN_TRUE` are on **different frames**, and a `SNAP` between them shows `meshvis=1` with `sc:Opacity` non-zero | **Visibility-order fault** |
| `DESTROY_REQ` frame **equals** the final `MAT_WRITE` frame | **Same-frame destroy.** The last write may not commit before teardown |
| `ENDPLAY` frame **minus** `VIS_FALSE` frame `== 0` | **No deferred destroy.** Spec §4.6 requires ≥1 full frame |
| Final `sc:Opacity` **> 0** at the last `SNAP` before `ENDPLAY` | **Fade never reaches zero.** Not a flash from reset — a flash from truncation |
| `midvalid=0` at any `SNAP` inside the exit window | **MID lost mid-fade.** Component fell back to the base material |
| Flash present on **cycle 1 only** | Warm-up / first-compile artefact, not the exit logic |
| Flash present from **cycle 2 onward** but not cycle 1 | **Reuse-path fault.** Strongly implicates pooling or MID reuse |

More than one may be true. Report all that match.

### 6.2 Which transform path creates the apparent continued approach?

Compare across the exit window:

| Signature | Indicated path |
|---|---|
| `apdist` decreases after `EXIT_BEGIN`, and `axform` translation changes | **Actor-level motion.** Approach is driven on the actor and not stopped at exit |
| `apdist` decreases, `axform` static, `vmrel` translation changes | **VisualMotion relative transform.** C3 keeps ticking through the fade |
| `apdist` decreases, `axform` and `vmrel` static, `mxform` changes | **Mesh-component-level motion** — a third path not in the C1–C7 catalogue. Report as a new finding |
| `apdist` static, but the object still *reads* as approaching | **Not a transform.** Scale or rim growth is producing apparent looming. Check `sc:` scale/rim values across the window |

Report the numeric `apdist` at `EXIT_BEGIN`, at `EXIT_70`, and at the last `SNAP`.

### 6.3 Why does visual overlap occur despite the earlier timing calculation?

The earlier calculation assumed visual lifetime ends at fade end. Candidates:

| Signature | Indicated cause |
|---|---|
| `NEXT_SPAWN` frame **<** `ENDPLAY` frame of the previous actor | **Lifetime exceeds cadence.** The arithmetic was right, the assumption about when the actor stops existing was wrong |
| `visiblepresentations >= 2` on any `SNAP` | **Confirmed overlap**, with the exact frame count |
| `visiblepresentations >= 2` while the previous actor's `sc:Opacity > 0.3` | **Overlap with a substantially visible predecessor** — worse than a faint tail |
| `NEXT_SPAWN` frame **>** `ENDPLAY` frame, yet overlap is still observed | The next actor becomes visible **before** its own `SPAWN` log — check whether it spawns visible at full opacity rather than alpha 0 |
| `activetargets >= 2` on any `SNAP` | **Worse than visual overlap** — two actors registered as targets simultaneously. This would be a live scoring defect, not a presentation defect. Escalate immediately |

Report the maximum `visiblepresentations` observed and the frame span for which it exceeded 1.

### 6.4 Does actor pooling actually exist in the current path?

| Signature | Finding |
|---|---|
| `auid` **repeats** across cycles | Pooling exists — same object reused |
| `auid` **increments** each cycle, and `ENDPLAY` has `reason=Destroyed` | **No pooling.** Fresh spawn each cycle |
| `DESTROY_REQ` has `mode=pool` but `ENDPLAY` still fires | A pool exists but is being bypassed — report as a defect |
| No `ENDPLAY` at all, and `auid` repeats | Pooling with no teardown. Every reset must then be explicit — and any missing reset is a flash candidate |

This answer determines whether the Stage 3 fix needs a pool-reset path at all. Do not assume from `P12`.

### 6.5 Is any MID or motion state reused across actors?

| Signature | Finding |
|---|---|
| Same `miduid` under two different `auid` values | **MID shared across actors.** Serious — one actor's fade writes affect another |
| `miduid` stable within a cycle, different across cycles, `auid` also different | Normal per-actor MID |
| `miduid` **stable across cycles** with repeating `auid` | Pooled actor reusing its MID. Then the flash is **not** MID replacement — look at §6.1's ordering rows instead |
| `vmrel` at the first `SNAP` of cycle *n+1* **≠** its value at cycle *n*'s `SPAWN` | **Motion state carried across reuse.** Directly explains a mid-flight-looking first frame |
| `sc:` values at cycle *n+1* `SPAWN` **≠** parent defaults | **Material state carried across reuse** |

---

## 7. Stage 1 report template — DO NOT PRE-FILL

Fill only from captured data. Leave anything not observed blank and say so.

```
STAGE 1 REPORT — CURRENT EXIT INSTRUMENTATION
Date:                        ____
UE version:                  ____
Mode:                        Environment PIE
Cycles captured:             ____   (require >= 3 natural expiry)
Flash observed on cycles:    ____
Research JSONL byte-identical: YES / NO

PREREQUISITES RESOLVED (§1)
P1..P12: ____

EVENT LEDGER  (one table per cycle, frame index + delta from SPAWN)
  spawn                  f=____  Δ=0
  outcome resolution     f=____  Δ=____
  exit begin             f=____  Δ=____
  exit 70%               f=____  Δ=____
  final material write   f=____  Δ=____
  visibility false       f=____  Δ=____
  HiddenInGame true      f=____  Δ=____
  completion delegate    f=____  Δ=____
  Destroy request        f=____  Δ=____
  EndPlay                f=____  Δ=____
  next target spawn      f=____  Δ=____

IDENTITY LEDGER
  cycle : auid : miduid : matiface : midcreated : CreateDynamicMaterialInstance called again?
  ____

EXIT-WINDOW SNAPSHOTS
  per frame: axform / mxform / vmrel / apdist / scalars / meshvis / meshhidden /
             actorhidden / beingdestroyed / activetargets / visiblepresentations
  ____

MAX visiblepresentations observed: ____   frames above 1: ____
MAX activetargets observed:        ____   (>1 = escalate immediately)

FINDINGS  (each must cite the frames and fields that prove it)
1. Terminal flash cause:              ____  evidence: ____
2. Continued-approach transform path: ____  evidence: ____
3. Overlap cause:                     ____  evidence: ____
4. Pooling exists:                    ____  evidence: ____
5. MID / motion state reuse:          ____  evidence: ____

UNRESOLVED AFTER STAGE 1
  ____   (if the flash cause is not determined, this triggers stop condition S2)

DISPOSITIONS
  C1 350 ms exit        : retained / deleted / replaced / moved / NOT YET PROVEN
  C2 MID fade           : ____
  C3 VisualMotion exit  : ____
  C4 delegates          : ____
  Assign ONLY where instrumentation proves it. "NOT YET PROVEN" is a valid answer.

STAGE 2 NOT STARTED.  Instrumentation left in place.  Not committed.  Not packaged.
```

---

## 8. What I could not do, and what is needed from you

| Blocker | Consequence |
|---|---|
| **The UE5 project is not in the connected folder.** Only documents plus an unrelated web demo (`eartech-flow-demo`) | I cannot read P1–P12, so call sites are described by role, not by name |
| **No Unreal Editor in this environment** | I cannot run Environment PIE, cannot produce frame indices, cannot capture cycles |
| **No Research Pipeline access** | I cannot run the fixed-seed regression or diff the JSONL |

To move Stage 1 from kit to report, either:

**(a)** connect the folder containing the `.uproject` and I will locate P1–P12, wire the call sites precisely, and hand back a build-ready patch — you run PIE and paste the log back; or

**(b)** hand this kit plus `TranquilMind_Implementation_Spec.md` to an agent with repository and editor access.

Either way, **the report must be written from a real log.** An unfilled template is a correct Stage 1 state. An invented one is not.
