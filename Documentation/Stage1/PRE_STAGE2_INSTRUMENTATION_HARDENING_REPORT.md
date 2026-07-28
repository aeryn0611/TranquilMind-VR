# PRE-STAGE-2 PART 2 — INSTRUMENTATION HARDENING

**Date:** 2026-07-28
**Scope:** Tooling only. No runtime visual behaviour, timing, cadence, spawning,
scoring, target registration, materials, motion, destruction, Research logic or
Research random streams were changed.

**Status: COMPLETE.**

| Gate | Result |
|---|---|
| Parser regression fixtures A–J | **27/27 PASS** |
| Replay — headless Environment (40 cyc) | **PASS** — conclusions reproduced |
| Replay — authoritative PIE (40 cyc) | **PASS** — conclusions reproduced |
| Fresh schema-2 capture | **PASS** |
| Automation | **23/23 PASS** |
| Research canonical regression | **PASS** — sha256 unchanged |
| Stimulus sequence | **PASS** — identical |
| Effective timing and cadence | **PASS** — identical |
| Map / material / Blueprint assets | **PASS** — unchanged |

---

## 1. Original defects

Both were confirmed against real Stage 1 evidence, not hypothesised.

**H1 — mixed sessions in one editor log.** One editor process writes one log
across many PIE runs. Schema 1 had no session marker, so a whole-log parse
silently merged four PIE sessions. During Stage 1 the authoritative capture had
to be isolated **by hand** at log line 15024. A whole-log parse reported 118
cycles where the authoritative session had 40.

**H2 — UObject UniqueID recycling.** `auid` and `miduid` are object-table
indices, reused after GC. In the authoritative PIE capture, 40 spawns produced
only **39 distinct `auid` and 38 distinct `miduid`**, and one `miduid` appeared
under two `auid`. The schema-1 rule (`len(actors) > 1` ⇒ shared) would have
reported *"MID SHARED ACROSS ACTORS — serious"*. It was recycling: lifetimes
`[290364..290374]` and `[294930..295159]` do not overlap.

A third defect surfaced during this work and is fixed here too:

**H3 — conflated identity keys corrupt per-lifetime metrics.** On schema 1,
keying by `auid` merges an actor with whichever later actor UE handed the same
index to. Comparing actor A's `DESTROY_REQ` against actor B's last `MAT_WRITE`
produced a nonsensical **−4900 frame** "teardown separation" on the archived PIE
capture. Such keys are now detected and excluded, labelled `LEGACY_LIMITATION`.

---

## 2. Schema 2 definition

Every trace line now begins `TMS1|schema=2|…` and carries session and
presentation identity:

```
TMS1|schema=2|f=<frame>|rt=<real>|wt=<world>|ev=<EVENT>|sid=<sid>|spawnseq=<seq>
    |aname=<name>|auid=<uid>|cyc=<cycle>|<event-specific pairs>
```

New boundary events:

```
ev=SESSION_BEGIN |sid=N|spawnseq=-1|map=<map>|worldtype=<n>|pie=<0|1>|mode=<Demo|Research|Unknown>|netmode=<n>
ev=SESSION_END   |sid=N|spawnseq=-1|reason=<n>|reasontext=<EEndPlayReason::…>|spawns=<count>
```

`SESSION_BEGIN` distinguishes Environment PIE, Void PIE, headless `-game`, and
Demo vs Research, via `map` + `pie` + `worldtype` + `mode`. The mode string is
**passed in by the spawner**, which has already resolved it — the tracer never
reads settings or protected state itself.

`NEXT_SPAWN` carries `sid` and `spawnseq=-1` (the presentation it announces does
not exist yet), plus `prev_spawnseq` alongside the diagnostic `prev_auid`.

---

## 3. Identity model

```
CANONICAL ACTOR IDENTITY  =  (sid, spawnseq)
```

| Field | Guarantee |
|---|---|
| `sid` | Unique per world/session. Stable for that world's whole lifetime. **Never reused within one editor process** — allocated from a monotonic counter that never resets. |
| `spawnseq` | Monotonic within `sid`. Assigned **exactly once**, at presentation creation. **Never derived from a UObject UniqueID.** Never reset until the session ends. |
| `auid`, `miduid`, `aname` | **DIAGNOSTIC ONLY.** Never treated as globally unique, by tracer or parser. |

Both counters are plain diagnostic integers. **No RNG of any kind is used**, so
R5 (seeded determinism) and R7 (Research random streams) cannot be perturbed —
confirmed empirically by the byte-identical Research regression in §9.

Per-actor state (`Cycle`, `SpawnSeq`, `Sid`, `bExit70Logged`, `SnapWindowEnd`)
lives in a per-actor record. Every event for a presentation carries the same
`sid`/`spawnseq`/`cyc` until `EndPlay`, so **overlap cannot cause a
predecessor's remaining events to acquire the successor's identity** — verified
in §7 and fixture C.

Session scoping: `ActorTraces` is cleared at both `SESSION_BEGIN` and
`SESSION_END`, so identities never leak between worlds.

---

## 4. Files changed

| File | Change | +/− |
|---|---|---|
| `Source/TranquilMind/Public/TMStage1Trace.h` | schema 2 API + per-actor identity | +60 / −4 |
| `Source/TranquilMind/Private/TMStage1Trace.cpp` | session lifecycle, identity emission | +110 / −10 |
| `Source/TranquilMind/Private/TargetSpawnerComponent.cpp` | 2 call sites: `LogSessionBegin` (BeginPlay), `LogSessionEnd` (EndPlay) | +14 / −0 |

Exact diff: `Documentation/Stage1/tools/TRACER_DIFF.md`.

**Every added statement is a log emission or a diagnostic-counter update.** No
setter, no transform write, no material call, no `Destroy`, no random draw —
Kit §0 N1–N3 still hold.

### Original tooling preserved

The archived kit is part of the Stage 1 evidence chain and is **unmodified**,
verified by checksum after all Part 2 work:

```
Documentation/Stage1/TMStage1Trace.h            OK
Documentation/Stage1/TMStage1Trace.cpp          OK
Documentation/Stage1/parse_tm_trace.py          OK
Documentation/Stage1/Stage1_Instrumentation_Kit.md  OK
```

The hardened parser is a **new versioned tool**:
`Documentation/Stage1/tools/parse_tm_trace_v2.py`.

---

## 5. Parser CLI

```bash
# enumerate sessions — always safe
python3 Documentation/Stage1/tools/parse_tm_trace_v2.py <log> --list-sessions

# analyse exactly one session
python3 Documentation/Stage1/tools/parse_tm_trace_v2.py <log> --session <sid> --out <report.md>

# single-session logs may omit --session; multi-session logs FAIL CLOSED (exit 3)
python3 Documentation/Stage1/tools/parse_tm_trace_v2.py <log> --out <report.md>
```

Behaviour:

1. Lists every session with schema, map, mode, world type, event count, actor
   count, frame range and end reason.
2. **Fails closed** (exit 3) rather than merging when several sessions exist.
3. Uses `(sid, spawnseq)` as the actor key on schema 2.
4. Still reads schema 1 archived evidence, labelling identity `LEGACY_INFERRED`,
   inferring session boundaries from world-time resets, never claiming global
   uniqueness for `auid`/`miduid`, and reporting unrecoverable conclusions as
   `LEGACY_LIMITATION`.

### Lifetime-aware identity rules

Lifetime = `SPAWN → ENDPLAY`; absent `ENDPLAY` ⇒ **OPEN / INCOMPLETE**, bounded
by the last frame seen.

- `SHARED_MID` / `SHARED_ACTOR_STATE` are reported **only** when two distinct
  identities use the same value **and their frame ranges overlap**.
- Otherwise → **`UE_ID_RECYCLED`**.
- **Pooling is never inferred from a recycled UID.** It requires positive
  evidence: `DESTROY_REQ mode=pool`, or a schema-2 identity respawning without
  `EndPlay`.

---

## 6. Regression fixtures and results

`Documentation/Stage1/tools/tests/test_parse_v2.py` — **27/27 checks pass**.

| # | Fixture | Proves |
|---|---|---|
| A | one session, distinct identities | baseline: 1 session, 2 actors, no sharing |
| B | multiple PIE sessions in one log | 2 sessions detected; **exit 3 without `--session`**; identical `auid`+`miduid` across sessions **not merged** |
| C | overlap, actor *n* emits after *n+1* spawns | predecessor keeps its own identity; **no relabelling**; overlap classed as presentation-ownership, not escalation |
| D | `auid` recycled after earlier actor ended | 2 identities despite identical `auid`; **`UE_ID_RECYCLED`**, not `SHARED_ACTOR_STATE` |
| E | `miduid` recycled after earlier actor ended | **`UE_ID_RECYCLED`**, not `SHARED_MID` |
| F | genuine **concurrent** MID sharing | **`SHARED_MID` — SERIOUS** still detected |
| G | missing `EndPlay` | lifetime marked incomplete but bounded; incomplete ≠ pooling |
| H | forced teardown | `SESSION_END` carries the reason; identity uncorrupted |
| I | schema 1 archived log | parsed; identity `LEGACY_INFERRED`; report carries the legacy notice; pooling answer carries `LEGACY_LIMITATION`; world-time reset **splits** sessions |
| J | schema 2 log | `sid` read from log, identity `SCHEMA2`, key is `(sid, spawnseq)` |

The four required properties are each proved: sessions never silently merged (B,
I), overlap never relabels the predecessor (C), recycled IDs never classified as
sharing (D, E), genuine concurrent sharing still detected (F).

---

## 7. Replay of the Stage 1 evidence

Both archived captures replayed **read-only**; neither file was modified.

| Stage 1 conclusion | Headless (40 cyc) | Authoritative PIE (40 cyc) |
|---|---|---|
| Terminal flash not reproduced | not a log question — parser explicitly declines | same |
| Visual overlap confirmed | **CONFIRMED** | **CONFIRMED** |
| `visiblepresentations` max = 2 | **2** | **2** |
| `activetargets` max = 1 | **1** | **1** |
| No proven pooling | **NO** (+ `LEGACY_LIMITATION`) | **NO** (+ `LEGACY_LIMITATION`) |
| No proven concurrent MID sharing | none detected | **`UE_ID_RECYCLED`** |
| Actor approach frozen during exit | **ACTOR TRANSFORM FROZEN** (40/40) | **ACTOR TRANSFORM FROZEN** (37/37 usable) |
| Same-frame write / Destroy / EndPlay | `DESTROY_REQ−MAT_WRITE={0}`, `ENDPLAY−DESTROY_REQ={0}` | same |
| UID recycling classed as `UE_ID_RECYCLED` | n/a (none) | **yes** |

The parser **independently rediscovered** the recycled MID that had to be found
by hand in Stage 1:

```
MID sharing | UE_ID_RECYCLED | 52112: auid51872 [290364..290374],
                                     auid47112 [294930..295159]
```

Reports: `Documentation/Stage1/tools/replay/*_v2.md`.

### `LEGACY_LIMITATION` recorded rather than guessed

1. **Pooling, both captures.** On schema 1, absence of pooling is asserted only
   from the absence of `mode=pool` events — never from `auid` uniqueness, which
   is exactly the unsound inference H2 describes.
2. **One conflated identity key, PIE capture.** `auid` recycling merged two
   presentations under one key. That key is excluded from exit and approach
   metrics and reported explicitly. This is what removed the bogus −4900 frame
   separation (H3); the remaining 37 exits give `{0}`, matching Stage 1.

Both are consequences of schema 1 having no stable identity. Neither is
recoverable from the archived logs, and neither was guessed. Schema 2 captures
do not have them.

---

## 8. Fresh schema-2 capture

Headless Environment, fixed 72 fps, `OperatingMode=Demo` — tooling validation
only, no behaviour tuned.

```
total TMS1 lines            : 12786
lines missing schema=2      : 0
distinct sid                : ['1']            <- stable across the session
lines missing sid/spawnseq  : 0
SPAWN count                 : 40
spawnseq monotonic 0..N-1   : True   (0,1,2,3,4,5,…)
spawnseq assigned once each : True
identities                  : 40
identities w/ mixed cyc     : 0                <- per-actor attribution holds
identities w/ mixed auid    : 0
identity frame-span overlaps: 1                <- overlap is real and preserved
overlapping pairs share seq : False            <- NO cross-cycle relabelling
```

Session boundaries, from a separate graceful-shutdown run:

```
ev=SESSION_BEGIN|sid=1|map=L_TranquilMind_Environment|worldtype=1|pie=0|mode=Demo|netmode=0
ev=SESSION_END  |sid=1|reason=4|reasontext=EEndPlayReason::Quit|spawns=0
```

**Real multi-session validation.** The automation run creates a fresh world per
test and produced **10 distinct sids in one process** — genuine multi-world data,
not a synthetic fixture. `--list-sessions` enumerates all ten with per-session
counts and end reasons, and the parser fails closed without `--session`:

```
ERROR: 10 sessions found in this log; refusing to merge them.
```

Note `reasontext=SupersededByNewWorld` on sessions closed by the next world
opening — the tracer never leaves two sessions open.

Artefacts: `Documentation/Stage1/tools/replay/schema2_fresh_environment.log`,
`schema2_session_boundary.log`.

---

## 9. Non-behavioural regression gates

| Gate | Required | Result |
|---|---|---|
| Automation | 23/23 | **23 pass, 0 fail** |
| Research canonical regression | sha256 unchanged | **`6ae4fd8c2e630806b089615c612b03d1cf41b013c5b566da9393960b21ac97eb`** — identical |
| Target stimulus sequence | unchanged | **identical** (`0101…01`, 40 spawns) |
| Effective timing and cadence | unchanged | **identical** — deltas `{181:1, 216:35, 217:3}` |
| Map assets | no change | Environment `2880939a…`, Void `35e3e742…` — same as Part 1 post-cleanup |
| Material assets | no change | none touched |
| Blueprint assets | no change | `BP_TranquilMindRuntime` `70731fb2…` unchanged |

The 181-frame Phase II anomaly documented in Stage 1 §3.3 is preserved exactly,
which is the sharpest available evidence that no timing changed.

---

## 10. Confirmation: runtime visual behaviour unchanged

1. **Stimulus sequence identical** — 40 spawns, same Go/NoGo order.
2. **Cadence identical** — same spawn-delta multiset, including the anomalous
   181-frame interval.
3. **Trace volume identical in structure** — 40 SPAWN / 40 EXIT_BEGIN /
   40 MID_CREATE / 40 EXIT_70 / 40 DELEGATE_COMPLETE / 40 DESTROY_REQ /
   40 ENDPLAY, matching the pre-hardening capture. (Total line count rises by 1:
   the new `SESSION_BEGIN`.)
4. **Research byte-identical** — same canonical sha256, so R1–R7 are untouched.
5. **Automation 23/23**, including `Demo.VisualExit.Lifecycle`.
6. **No asset of any kind modified.**
7. **Source diff is log-only** — every added line emits a log string or updates a
   diagnostic counter.

---

## 11. Remaining limitations

| # | Limitation |
|---|---|
| L1 | **`sid` is unique per process, not globally.** Two separate editor/game processes each start at `sid=1`. Concatenating logs from different processes would collide. Real UE logs are per-process, so this does not arise in practice — but do not merge logs from separate runs by hand. |
| L2 | **Schema-1 session segmentation is a heuristic.** It infers boundaries from world-time resets (>1 s backwards step). Labelled `LEGACY_INFERRED` everywhere. Schema-2 logs use the authoritative `sid`. |
| L3 | **Schema-1 conflated keys cannot be un-merged.** Where `auid` recycling merged two presentations, the parser excludes the key rather than guessing. The archived PIE capture has exactly one such key. |
| L4 | **`SESSION_END` requires a graceful teardown.** A hard kill (SIGALRM/SIGKILL) closes the log before `EndPlay`, so the session is reported `open/none`. This is a property of process termination, not of the tracer. |
| L5 | **The flash question remains outside tooling.** No log field can establish whether a flash is visible; the parser says so explicitly rather than implying an answer. |
| L6 | **Pooling detection is evidence-based and therefore conservative.** With no pool in the codebase there is nothing to validate the positive path against beyond fixture G. |

---

```
PART 2 COMPLETE.  PART 3 NOT STARTED.
Implementation Spec not revised.  Stage 2 not started.
Nothing committed.  Nothing packaged.
Stage 1 report, archived traces, kit, maps and backups all unmodified.
```
