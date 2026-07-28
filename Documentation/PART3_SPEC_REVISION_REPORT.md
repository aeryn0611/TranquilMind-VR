# PART 3 — IMPLEMENTATION SPEC REVISION REPORT

**Date:** 2026-07-28
**Scope:** Documentation only. **Stage 2 was not implemented.**

**Status: COMPLETE.**

| Confirmation | Result |
|---|---|
| No runtime source changed | **CONFIRMED** |
| No map, Blueprint or material asset changed | **CONFIRMED** |
| No Stage 2 implementation began | **CONFIRMED** |
| Stage 1 and Part 1/2 evidence untouched | **CONFIRMED** |
| Complete Plan / Decision Table / Literature Review unedited | **CONFIRMED** |
| Committed or packaged | **NO** |

---

## 1. Outputs

| # | Artifact | Path |
|---|---|---|
| 1 | Revised Spec (Revision 2) | `Documentation/TranquilMind_Implementation_Spec.md` |
| 2 | Pre-Part-3 archive + hash | `Documentation/Spec_Archive/` |
| 3 | Section-by-section diff summary | §3 of this report |
| 4 | Cross-document conflict register | `Documentation/PART3_CROSS_DOCUMENT_CONFLICT_REGISTER.md` |
| 5 | This report | `Documentation/PART3_SPEC_REVISION_REPORT.md` |

### Archive integrity

```
Documentation/Spec_Archive/TranquilMind_Implementation_Spec_pre_Part3_20260728.md
  21559 bytes   sha256 696fc36d99a38e8f5cef3592bf0c0a638a56edd202634a220418e555d58a30a1
```

The archive is a **verbatim** pre-edit copy and was not overwritten.
`Documentation/Stage1/TranquilMind_Implementation_Spec.md` is retained **frozen**
as the Stage-1-era evidence snapshot and carries the identical hash. The living
authority is now `Documentation/TranquilMind_Implementation_Spec.md`.

```
Revision 1 (pre)  : 21559 bytes, 413 lines, sha 696fc36d99a38e8f…
Revision 2 (post) : 39259 bytes, 643 lines, sha 1fca08ff18db6601…
Line churn        : +313 / −140
```

---

## 2. Authority rule applied

- The Spec remains the **sole implementation authority**.
- **Stage 1 runtime evidence overrode Revision 1's assumptions** about the
  existing implementation wherever the two disagreed. Four Revision 1 claims
  were withdrawn as factually wrong (§3, items 2.3–2.6).
- Plan / Decision Table / Literature Review were treated as **reasoning
  documents** and **not edited**. Eleven residual conflicts are registered
  instead, with priorities.

---

## 3. Section-by-section diff summary

### §1 Protected Research invariants — **substantially revised**

| | |
|---|---|
| **R5** | reworded: identical seed must yield identical **canonical** output |
| **§1.1** | Replaced the single unconditional *"raw JSONL must be byte-identical"* gate with a **two-layer gate**. **Layer A** raw-artifact integrity: preserve every raw file unchanged, verify schema/field names/field order/types/trial count, record SHA-256, **never rewrite a raw file to make it deterministic**. **Layer B** canonical deterministic regression: versioned projection tool, exclusions only from the register, projections byte-identical, and a mandatory archive set (raw inputs, canonical outputs, tool version+hash, register version, all SHA-256, diff result). Added the rule that **the gate must itself be validated on two pre-change runs before it is trusted.** |
| **§1.2 (new)** | **Nondeterministic-field register v1**, with a stated reason per field. Protected deterministic content enumerated and marked never-excludable (trial construction, Go/NoGo sequence, seeded draws, seqHash, scheduled ITI, scoring outcome, block structure, deterministic physiology gating, all research-semantic fields). Adding an exclusion requires review, a version bump and gate re-validation. |

### §2 Current code baseline — **rewritten from evidence**

| Entry | Change |
|---|---|
| **C1** | Now: implemented, **visually abrupt**, drives **opacity + relative scale** (not opacity-only), same-frame final write / completion / Destroy / EndPlay, **no Hidden frame** |
| **C2** | Now: fresh MID per fresh actor, **no pool exists**, **no concurrent cross-actor sharing proven**, flash **not reproduced**, **NOT YET PROVEN** as a defect cause. Revision 1's "prime flash suspect … pool reuse" **withdrawn** |
| **C3** | Now: actor approach **frozen at exit start**. Revision 1's "does not stop approach motion at exit" **withdrawn**. Disposition NOT YET PROVEN / reassess |
| **C4** | Now: **verified functional**, **RETAIN** |
| **C5** | Now: **NOT REPRODUCED**; no fictional root cause; same-frame teardown recorded as **architectural fragility**, not a proven flash cause |
| **C6** | Now: **not supported by evidence** (scale *shrinks*; no actor or mesh translation during exit) |
| **C7** | Now: **CONFIRMED** by trace and human PIE; cause is **presentation ownership coupled to `ActiveTargets`**; `visiblepresentations` 2 while `activetargets` 1; **presentation-ownership defect, not scoring-concurrency** |
| **Dispositions** | Added the Stage 1 table: C1 REPLACE in Stage 3 · C2 NOT YET PROVEN · C3 NOT YET PROVEN/reassess · C4 RETAIN |

### §3 Clock semantics — **extended**

Added **`ScheduledPhase4End`** as a first-class, response-independent quantity,
and the rule that Phases 6 and 7 are timed from it, never from response time.
Hard-coded durations replaced with references to §4.7.

### §4 Lifecycle state machine — **substantially revised**

| | |
|---|---|
| Title | *"— A-prime defaults"* removed; the table is now configuration-independent |
| **§4.1** | Added a **`Owns LiveDemoPresentation`** column spanning Phase 1 → Phase 9. Explicit note that **Phase 8 (Hidden) does not exist today** and arrives at Stage 3 |
| §4.3 | Added: `ActiveTargets` is a **scoring-eligibility set, not a presentation set** |
| **§4.5** | **Corrected the impossible combination.** Settle endpoint becomes a **derived output**, nominal ≈3.15–3.25 m, accepted only after Stage 5. Peak ≤20 cm/s kept as a hard ceiling. Explicit prohibition on raising peak speed to preserve 3.00 m, and on claiming 3.00 m + ≤20 cm/s deceleration are simultaneously mandatory |
| **§4.6** | Rim channels renamed **edge/rim analogue**; added a **parameter-availability warning** listing the six parameters that actually exist and stating that **no rim-width or rim-intensity parameter exists** — Stage 3 must map or add them first. Added the measured `CenterOpacity ≈ 0.000995` no-op |
| **§4.7 (new)** | Three named configurations in one table: **Stage 2 Compatibility** (0/0/2500/0/350, cadence 3000, lifetime 2850, gap 150), A-prime, B-prime. Normative statement that Stage 2 changes **structure and ownership only** and must not introduce A-prime timing, the 400 ms compound exit, material changes or motion changes. Zero-duration phases are structurally present |
| §4.8 | Readable-hold A/B explicitly deferred to Stage 4 |
| **§4.9 (new)** | **Presentation ownership.** `ActiveTargets` vs `LiveDemoPresentation` contrasted on contents, start, end and purpose. **The spawn gate must not rely only on `ActiveTargets.Num()`.** Mechanism free, semantics mandatory. **§4.9.1** exactly-once cleanup on all **11** named paths, plus: a stale reference **must never permanently block spawning** |

### §5 Early-response semantics — **new section**

The 8-step required sequence: accept → commit outcome and RT immediately and
exactly once → close input → deregister → discard later input → **visual
presentation continues to `ScheduledPhase4End`** → Persistence and Exit timed
from **scheduled** time → cadence and lifetime independent of reaction speed.
States that **visual lifetime must not vary with reaction time**, and that
natural expiry and early response **converge on the same scheduled exit path**.

### §6 Material parameter ownership *(was §5)* — **minor**

Group A/B parameter names generalised to **role names**; added the measured
list of parameters that actually exist and assigned the mapping to Stage 6.

### §7 Go/NoGo identities *(was §6)* — **minor**

Added the Stage 1 observer results (poor discrimination, boundary hard to see
at distance) as **Stage 6/7 inputs, explicitly not Stage 1 findings**.

### §8 Ambient *(was §7)*, §9 Staging *(was §8)* — **minor**

Added `LiveDemoPresentation` exclusion to both invariant lists.

### §10 Performance gates *(was §9)* — **unchanged**

DVFS-as-warning retained.

### §11 Stages *(was §10)* — **revised**

Stage 1 marked **COMPLETE**. Stage 2 restated as **lifecycle + ownership at
Stage 2 Compatibility timing**. Stage 3 acceptance gains **≥1 full Hidden
frame**. Stage 5 gains *"settle endpoint recorded and accepted here"*. Added
ordering rules **2 before 3** and **3 before 4**. **§11.1 (new)**: ten blocking
Stage 2 acceptance criteria S2-A…S2-J, validated with `(sid, spawnseq)`, with
an explicit prohibition on using `auid`/`miduid` as global identities.

### §12 Stop conditions — **revised**

S1 rewritten for the two-layer gate, including that a raw difference confined to
registered nondeterministic metadata is **not** automatically a Research
failure. **S2 retired** with a pointer to §12.1. Added **S9** (Stage 2 fails if
official presentation overlap remains), **S10** (fails if early response changes
visual lifetime), **S11** (fails if new code touches R1–R7), **S12**
(instrumentation ambiguity must be reported, never guessed).
**§12.1 (new)** defines the two-branch Stage 1 acceptance, records that Stage 1
completed under branch (b), and forbids reintroducing "flash cause must be
found" while the flash is not reproducible — with instructions to reopen it as a
new evidence-backed defect if it returns.

### §13 Instrumentation contract — **new normative section**

Schema 2 mandatory fields; canonical identity `(sid, spawnseq)`; `auid`,
`miduid`, actor name diagnostic only; the ten identity rules including
`sid` uniqueness being **per process, not global** and the cross-process
aggregation requirement; `UE_ID_RECYCLED`; shared-MID only on overlap; pooling
never inferred from a recycled UID; parser fails closed; `LEGACY_INFERRED` /
`LEGACY_LIMITATION`. Session-boundary rules including **a missing `SESSION_END`
alone is not proof of a runtime defect** and completed lifetimes inside an
incomplete session remain usable when explicitly scoped. Tooling table.

*(Revision 1 §11 "Stage 1 instrumentation requirements" was consumed: its five
checks are discharged and recorded in the Stage 1 report; §12.1 and §13 carry
forward what remains normative.)*

### §14 Files likely touched *(was §13)* — **revised**

Added `LiveDemoPresentation` ownership row; C1 marked replaced at Stage 3; C2
marked *do not replace speculatively*; spawner row now says gate on
`LiveDemoPresentation`, **not** `ActiveTargets.Num()`; material row gains the
rim-parameter mapping obligation.

### §15 Demo copy constraints *(was §14)* — **unchanged**

---

## 4. Cross-document conflict register

Eleven conflicts recorded in
`Documentation/PART3_CROSS_DOCUMENT_CONFLICT_REGISTER.md`, each with source
document + section, outdated wording, revised authority, why, and whether
archival annotation is needed.

| Priority | Conflicts |
|---|---|
| **High — would cause wrong or wasted work** | C-01 unsatisfiable raw byte-identical gate · C-07 overlap framed as a cadence number · C-02 impossible 3.50→3.00 m / ≤20 cm/s deceleration |
| **Medium — work with no defect behind it** | C-03 approach-continues-through-exit · C-04 pooling/MID-reuse hypothesis · C-08 Stage 1 requires a flash cause · C-09 A-prime as current default · C-06 DVFS rise as automatic failure |
| **Low — wording only** | C-05 "opacity-only" · C-11 "too short" |
| **Open design question** | C-10 exit easing direction (NN/g ease-in vs Spec ease-out) — already flagged by the LitRev against itself |

**Eight of eleven are recommended for dated archival annotation.** None was
applied — the three documents were not edited.

The single most consequential is **C-07**: the reasoning documents attribute
overlap to lifetime/cadence arithmetic, which implies the fix is a timing
number. It is not. Measured steady-state cadence already leaves a clear gap;
overlap arises from a pending spawn released the frame the predecessor is
deregistered. **No cadence value fixes that** — only the ownership rule does.

---

## 5. Confirmations

### 5.1 No runtime source changed

```
git status --porcelain -- Source/   →  identical to the pre-Part-3 state
```
The nine modified and fourteen untracked `Source/` entries are unchanged from
Part 2. No file under `Source/` was opened for writing in Part 3.

### 5.2 No map, Blueprint or material asset changed

```
L_TranquilMind_Environment.umap   2880939a21f8720c…   unchanged
L_TranquilMind_Void.umap          35e3e742b479126a…   unchanged
BP_TranquilMindRuntime.uasset     70731fb2fec395f7…   unchanged
Content/Environment/Materials/*                        untouched
```

### 5.3 No Stage 2 implementation began

No lifecycle component, no `LiveDemoPresentation`, no phase machine, no spawn-gate
change was written. §4.9 and §5 are **specifications of required behaviour**, not
implementations.

### 5.4 Stage 1 and Part 1/2 evidence untouched

```
Documentation/Stage1/TMStage1Trace.h                 OK
Documentation/Stage1/TMStage1Trace.cpp               OK
Documentation/Stage1/parse_tm_trace.py               OK
Documentation/Stage1/Stage1_Instrumentation_Kit.md   OK
Documentation/Stage1/STAGE1_REPORT.md                unmodified
Documentation/Stage1/PRE_STAGE2_BLUEPRINT_CLEANUP_REPORT.md          unmodified
Documentation/Stage1/PRE_STAGE2_INSTRUMENTATION_HARDENING_REPORT.md  unmodified
Documentation/Stage1/evidence/*  (schema-1 + schema-2 traces)         unmodified
Documentation/Stage1/tools/tests/*  (fixtures)                        unmodified
Backups/PreStage2_BlueprintCleanup_20260728/*                         verified OK
```

### 5.5 Reasoning documents unedited

Opened **read-only**. Modification times all predate this session's first action
(≈06:30Z) by roughly 45 minutes, and Part 3 by over four hours:

```
TranquilMind_Complete_Visual_Timing_Plan.md   2026-07-28T05:42:54Z
TranquilMind_Design_Decision_Table.md         2026-07-28T05:40:18Z
TranquilMind_Literature_Review.md             2026-07-28T05:44:57Z
```

---

## 6. Judgement calls made, and why

1. **Where the living Spec lives.** The Stage 1 folder holds a verbatim
   Spec copy as *evidence of what the Spec said during Stage 1*. Editing it
   would corrupt that record. Revision 2 therefore lives at
   `Documentation/TranquilMind_Implementation_Spec.md`; the Stage 1 copy stays
   frozen; the pre-Part-3 archive is a third, independent copy.

2. **Nominal settle figure.** §4.5 gives ≈3.15–3.25 m as an explicitly
   provisional planning figure, per instruction, and states plainly that the
   endpoint is an output of the curve accepted at Stage 5. No curve was
   selected — that is Stage 5's job, and picking one here would smuggle a design
   decision into a documentation task.

3. **Rim parameters.** §4.6 keeps the two rim channels as *requirements* but
   renames them "edge/rim analogue" and states outright that no such parameter
   exists, with an obligation on Stage 3 to map or add them. The alternative —
   silently renaming them to `EdgeOpacity` — would have invented a design
   decision and hidden the gap.

4. **Old §11 consumed rather than deleted.** Revision 1 §11 ("Stage 1
   instrumentation requirements") was a forward-looking checklist that is now
   discharged. Its normative residue lives in §12.1 and §13; the checklist
   itself is preserved in the archived copy and in the Stage 1 report.

---

## 7. Limitations

| # | Limitation |
|---|---|
| L1 | **The reasoning documents remain internally inconsistent with the Spec.** By instruction they were not edited. Until annotated, a reader consulting the Plan or Decision Table directly can still act on superseded numbers — most dangerously C-01 and C-07. |
| L2 | **The settle endpoint is unresolved by design.** ≈3.15–3.25 m is provisional; the real value depends on a curve not yet chosen. Any downstream document quoting a settle distance before Stage 5 is quoting a placeholder. |
| L3 | **The rim-channel mapping is unresolved.** Stage 3 cannot begin its material work until §4.6's mapping-or-addition decision is made. |
| L4 | **Register v1 was derived from two runs on one machine.** It is sufficient to make the gate pass reproducibly here, but a different host or frame cadence could surface a further nondeterministic field. The register is versioned precisely so that adding one is a reviewed act, not an ad-hoc exclusion. |
| L5 | **C-10 (exit easing) is left open.** Stage 1 evidence cannot settle it; it is a design question for Stage 3. |

---

```
PART 3 COMPLETE.
Stage 2 NOT started.  No runtime code changed.  No asset changed.
Reasoning documents not edited.  Nothing committed.  Nothing packaged.
```
