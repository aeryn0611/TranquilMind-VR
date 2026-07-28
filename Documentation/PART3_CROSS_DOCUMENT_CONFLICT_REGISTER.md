# PART 3 — CROSS-DOCUMENT CONFLICT REGISTER

**Date:** 2026-07-28
**Authority:** `Documentation/TranquilMind_Implementation_Spec.md` Revision 2.

The Complete Plan, Design Decision Table and Literature Review are **reasoning
documents, not authority**. Per the Part 3 authority rule they were **not
edited**. Every residual conflict with the revised Spec is listed here instead.

Source documents (in the connected project, `Academic Portfolio 2026/TranquilMind_Research/`):

| Short name | File |
|---|---|
| **Plan** | `TranquilMind_Complete_Visual_Timing_Plan.md` (635 lines) |
| **Table** | `TranquilMind_Design_Decision_Table.md` (214 lines) |
| **LitRev** | `TranquilMind_Literature_Review.md` (362 lines) |

"Needs archival annotation" = the document should carry a dated note pointing at
Spec Revision 2, so a future reader does not act on the superseded wording. **No
such annotation was added in Part 3.**

---

## C-01 — Raw byte-identical Research gate

| | |
|---|---|
| **Source** | Plan §"blocking gate" (line 30); Table D1 Validation (line 30), D-series Validation (line 159) |
| **Outdated wording** | Plan: *"diff the JSONL against the pre-change baseline. **It must be byte-identical.** No exceptions…"* · Table: *"Research Pipeline JSONL diff must be byte-identical"*, *"It must be byte-identical. This is a **blocking** gate for every change in this table."* |
| **Revised authority** | Spec §1.1 — **two-layer gate**. Layer A raw-artifact integrity (preserve, verify schema/fields/order/types/count, record SHA-256, never rewrite a raw file). Layer B canonical deterministic regression, byte-identical **after** a versioned projection excluding only registered nondeterministic fields (§1.2). |
| **Why** | Measured: two independent **pre-change** runs already differ (GUID, wall clock, uptime timestamps, frame-quantised values). The old requirement was unsatisfiable before any change was made. |
| **Needs archival annotation** | **Yes — high priority.** This wording, taken literally, fails every stage forever. |

---

## C-02 — 3.50 → 3.00 m with ≤20 cm/s monotonic deceleration

| | |
|---|---|
| **Source** | Plan §spatial (lines 248, 250); Table D-geometry (lines 111, 113), D-motion (lines 139, 141); LitRev (lines 208, 223) |
| **Outdated wording** | Plan: *"Settle centre distance 3.00 m [H]"*, *"Peak speed 20 cm/s [H]"* · Table: *"Current: settles at 300 cm centre"*, *"Recommended: settle at 280–300 cm"*, *"Keep 20 cm/s peak. Switch … to Motion Model 2 — decelerating, easing to near-zero velocity by the end of the response window at 2.8 m"* · LitRev: *"Terminal centre distance … 3.00 m"*, *"Model 2 — decelerating, settling ~2.8–3.0 m"* |
| **Revised authority** | Spec §4.5 — spawn 3.50 m fixed; peak **≤20 cm/s** hard ceiling; monotonic deceleration; velocity ≈0 at `ScheduledPhase4End`; **settle endpoint is an OUTPUT derived from the curve**, initial nominal **≈3.15–3.25 m**, accepted only after Stage 5 validation; nearest-surface floor ≥2.00 m. |
| **Why** | Arithmetically inconsistent. 50 cm in 2.5 s needs a *mean* of 20 cm/s; a monotonically decelerating curve ending at ≈0 needs a peak well above that (≈40 cm/s for a linear ramp-down). The 3.00 m endpoint in the current build comes from **constant** 20 cm/s, which is not deceleration and does not end at ≈0. The Table's 2.8 m figure is further from the achievable endpoint than 3.00 m, not closer. |
| **Needs archival annotation** | **Yes.** Three documents carry the impossible combination. |

---

## C-03 — Actor approach continues through the exit

| | |
|---|---|
| **Source** | Plan C6 (line 88); Table D-exit Rationale (line 71); LitRev symptom table (line 301) |
| **Outdated wording** | Plan: *"C6 Observed: continued apparent approach during fade — Explained by C3"* · Table: *"Approach motion continuing through the fade produces 'still coming while leaving'"* · LitRev: *"'Still looks like it's approaching while fading' → Approach motion continues through the fade → Freeze approach at outcome lock"* |
| **Revised authority** | Spec §2 C3/C6 — **withdrawn.** Actor-level approach translation is **frozen at exit start**; the actor transform is bit-identical across every exit frame, because `FinishResolution()` disables the actor tick before the exit begins. Only ≈0.2 cm relative drift and a scale *contraction* (1.0 → 0.94) remain. |
| **Why** | Measured across 80 instrumented cycles in both headless and authoritative PIE. The recommended remedy ("freeze approach at outcome lock") is already implemented. |
| **Needs archival annotation** | **Yes.** Acting on this would be work with no defect behind it. |

---

## C-04 — Pooling / MID-reuse as the leading flash hypothesis

| | |
|---|---|
| **Source** | Plan C2 (line 84); Table D-exit "Flash cause" (line 72) |
| **Outdated wording** | Plan: *"C2 MID-based fade … **Prime suspect for the flash** — a MID replaced or re-acquired on pool reuse can render one frame at parent defaults"* · Table: *"Leading hypothesis is MID replacement on pool reuse rendering one frame at parent defaults."* |
| **Revised authority** | Spec §2 C2 — **withdrawn and refuted.** **No object pool exists** anywhere in the project, so there is no reuse path. A fresh MID is created at exit start on a fresh actor each cycle. No **concurrent** cross-actor MID sharing was proven; the one repeated `miduid` was `UE_ID_RECYCLED` (non-overlapping lifetimes). |
| **Why** | 80 cycles: distinct MID per presentation, `ENDPLAY reason=Destroyed` on every cycle, zero pool-return events. |
| **Needs archival annotation** | **Yes.** |

---

## C-05 — "Opacity-only" description of the 350 ms exit

| | |
|---|---|
| **Source** | Plan C1 (line 83) and disposition example (line 99); Table D-exit Current (line 67) and Rationale (line 71) |
| **Outdated wording** | Plan: *"C1 350 ms visual exit — Implemented. Too short and opacity-only"*, *"C1 (350 ms opacity-only) is the clearest case"* · Table: *"350 ms MID-based opacity fade"*, *"Opacity-only fade leaves the object's *behaviour* unchanged — only its alpha."* |
| **Revised authority** | Spec §2 C1 — the exit drives **opacity *plus* relative scale (1.0 → 0.94)**. It is not opacity-only. |
| **Why** | Measured: `vmrel` scale goes 1.0000 → 0.9400 across the exit window in every cycle. |
| **Needs archival annotation** | **Yes**, though the practical impact is small — C1 is replaced at Stage 3 regardless. The Table's *rationale* is what is wrong, not its conclusion. |

---

## C-06 — DVFS rise treated as automatic failure

| | |
|---|---|
| **Source** | Table D20 (line 178) |
| **Outdated wording** | *"**Stop rule:** if DVFS rises above low, the tier has failed."* |
| **Revised authority** | Spec §10.3 — **a DVFS increase is a warning requiring investigation, not an automatic failure.** If B1–B4 all pass with elevated DVFS, **the stage passes**; the level is recorded because it reduces margin and raises thermal risk. |
| **Why** | The Spec has held the softer rule since Revision 1 (old §9.3); the Table was never reconciled. Note the two documents also agree on the second half — neither permits compensating by cutting resolution or MSAA. |
| **Needs archival annotation** | **Yes.** As written the Table can fail a stage the Spec passes. |

---

## C-07 — Overlap attributed to lifetime/cadence arithmetic

| | |
|---|---|
| **Source** | Table D-exit Rationale (line 71); Table D-cadence (lines 41–42); Plan (lines 155, 203) |
| **Outdated wording** | Table: *"No lifetime/cadence separation produces the overlap."* · *"4200 ms is the shortest cadence that accommodates A-prime's 3800 ms visual lifetime with a clear 400 ms gap and **no overlap**"* · Plan: *"400 ms clear gap … No overlap."* |
| **Revised authority** | Spec §2 C7 and §4.9 — overlap is a **presentation-ownership defect**: `ActiveTargets` deregistration happens at resolution while the actor keeps rendering for the whole exit, and the spawn gate reads registration rather than visibility. |
| **Why** | Measured: steady-state cadence already leaves a clear gap (2850 ms lifetime vs 3000 ms cadence; 35 of 39 intervals showed no overlap). Overlap occurred when a *pending* spawn request was released the frame the predecessor was deregistered. **A larger cadence does not fix this** — the same release path exists at any cadence. |
| **Needs archival annotation** | **Yes — high priority.** This is the conflict most likely to cause wasted work, because it implies the fix is a timing number when it is an ownership rule. |

---

## C-08 — Stage 1 acceptance requires a flash cause

| | |
|---|---|
| **Source** | Table stage list (line 196) |
| **Outdated wording** | *"1 · Flash instrumentation. No visual change. … Acceptance: **Flash cause identified and written down**"* |
| **Revised authority** | Spec §12.1 — acceptance is satisfied by **(a)** identifying the cause of a *reproducible* defect **or (b)** the defect **not reproducing** under authoritative instrumentation, provided the result and its limits are recorded. Stage 1 completed under **(b)**. Old stop condition **S2 is retired**. |
| **Why** | The flash did not reproduce across 40 authoritative PIE cycles with a human observer. A gate that cannot be satisfied blocks all downstream work and invites a fabricated root cause. |
| **Needs archival annotation** | **Yes.** |

---

## C-09 — A-prime presented as the current default

| | |
|---|---|
| **Source** | Plan (lines 140–141, 150, 197–205, 216); Table D-cadence (line 40), phase recommendation (line 84); LitRev (line 112, 312) |
| **Outdated wording** | LitRev: *"**A-prime — CURRENT DEFAULT** … 2500 ms … 4200 ms"* · Plan: *"**Default: Candidate A-prime at 4200 ms cadence.**"*, *"Total visual lifetime 3800 ms; cadence 4200 ms"* · Table: *"Recommended: 4200 ms (Candidate A-prime)"* |
| **Revised authority** | Spec §4.7 — three named configurations. **Stage 2 uses the Stage 2 Compatibility configuration** (Entrance 0 / Hold 0 / Response 2500 / Persistence 0 / Exit 350 / cadence 3000 / lifetime 2850). **A-prime is Stage 4 only.** |
| **Why** | Not a contradiction of intent — A-prime remains the Stage 4 target. The conflict is one of **scope and sequencing**: the reasoning documents describe A-prime as already-current, which would pull Stage 4 timing into Stage 2 and confound the lifecycle refactor with a timing change. Stage 2 must change structure and ownership only. |
| **Needs archival annotation** | **Yes** — annotate as "Stage 4 target, not Stage 2". |

---

## C-10 — Exit easing direction (pre-existing, unresolved)

| | |
|---|---|
| **Source** | LitRev (lines 282, 324) vs Spec §4.6 |
| **Outdated wording** | LitRev: NN/g — *"entrances should be longer than exits (e.g. 300 ms in, 200–250 ms out), with ease-out for entrances and **ease-in for exits**"*; and LitRev's own caveat: *"No peer-reviewed source on XR exit-animation duration was found. The 400 ms recommendation is reasoned, not evidenced, and departs from the only quantitative guidance located."* |
| **Revised authority** | Spec §4.6 — 400 ms compound exit on **ease-out**, longer than the 400 ms entrance is short. |
| **Why** | **Not resolved by Stage 1 evidence and deliberately left open.** The LitRev already flags it against itself. Recorded here so Stage 3 makes the departure knowingly rather than by omission. |
| **Needs archival annotation** | **No** — the LitRev already states the tension honestly. Carry into Stage 3 as an open design question. |

---

## C-11 — Old exit-duration framing ("too short")

| | |
|---|---|
| **Source** | Plan C1 (line 83); Table D-exit (line 68 band discussion) |
| **Outdated wording** | *"Too short and opacity-only"* — implying duration is the primary fault. |
| **Revised authority** | Spec §2 C1 and §11 Stage 3 — the exit is replaced because it is **abrupt, has no Hidden frame, and tears down on the same frame as the final material write**. Duration is a contributing factor, not the diagnosis. |
| **Why** | Measured: teardown separation `{0}` frames in every cycle; zero `VIS_FALSE`, zero `HIDDEN_TRUE`. The Table's own rationale (line 71) already argues duration is not the fault — it simply retains "too short" phrasing elsewhere. |
| **Needs archival annotation** | Optional — low impact, since the recommended remedy is unchanged. |

---

## Summary

| Priority | Conflicts |
|---|---|
| **High — would cause wrong or wasted work** | C-01 (unsatisfiable gate), C-07 (overlap framed as a cadence number), C-02 (impossible motion combination) |
| **Medium — would cause work with no defect behind it** | C-03, C-04, C-08, C-09, C-06 |
| **Low — wording only, conclusion unchanged** | C-05, C-11 |
| **Open design question, not a defect** | C-10 |

**No reasoning document was edited.** Eight of eleven conflicts are recommended
for dated archival annotation pointing at Spec Revision 2.
