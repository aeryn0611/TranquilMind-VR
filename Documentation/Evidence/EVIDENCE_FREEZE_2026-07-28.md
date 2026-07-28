# Evidence freeze — 2026-07-28

Repository-organisation and preservation record. No runtime behaviour was
changed, no Stage 2 work was continued, no PIE session was run, no APK was
packaged, and Stage 3 was not begun.

---

## Status matrix (authoritative)

| Stage | Status |
|---|---|
| Stage 1 | **VERIFIED COMPLETE** |
| Pre-Stage-2 Part 1 — Blueprint cleanup | **VERIFIED COMPLETE** |
| Pre-Stage-2 Part 2 — instrumentation hardening | **VERIFIED COMPLETE** |
| Pre-Stage-2 Part 3 — Implementation Spec revision | **VERIFIED COMPLETE** |
| Stage 2 — lifecycle and presentation ownership | **NOT YET VERIFIED COMPLETE** |

### Stage 2 detail

| Check | Result |
|---|---|
| Automated build / tests / captures | **PASS** |
| Research canonical regression | **PASS** |
| Human Environment PIE acceptance | **PENDING** |
| 206-frame phase-boundary cadence interpretation | **PENDING** |
| Overall | **NOT YET VERIFIED COMPLETE** |

Stage 2 must not be described as final, complete or fully verified.

---

## Git result

**Branch:** `backup/tranquilmind-20260728-stage2-pending`, created from
`4c60f0d` with the working tree preserved. No reset, stash or clean was used.
`main` was not moved, merged into, or rebased.

| # | Commit | Subject | Files | Status |
|---|---|---|---:|---|
| 1 | `e7758f6` | chore: preserve audited pre-Stage-2 runtime baseline | 34 | VERIFIED |
| 2 | `8babf30` | docs: freeze Stage 1 and pre-Stage-2 evidence | 24 | VERIFIED |
| 3 | `8f6928d` | fix: remove stale Level Blueprint runtime initialization paths | 2 | VERIFIED |
| 4 | `6f83452` | test: harden TranquilMind trace identity and session parsing | 54 | VERIFIED |
| 5 | `664deeb` | wip: implement Stage 2 lifecycle ownership pending human PIE | 14 | **WIP** |

**Verified tag:** `evidence/tranquilmind-stage1-prestage2-20260728` → `6f83452`
(commit 4). The Stage 2 WIP commit is **not** reachable from the tag; this was
asserted with `git merge-base --is-ancestor`.

No commit was amended, rebased or squashed.

---

## Verification results

| Check | Result |
|---|---|
| Pre-cleanup binary backups unchanged | **PASS** — match `MANIFEST.txt` byte for byte |
| Current maps match approved post-cleanup hashes | **PASS** — staged LFS OIDs equal the Part 1 report values |
| No map, Blueprint or material asset changed during packaging | **PASS** |
| No runtime source changed during packaging | **PASS** |
| Stage 1 / Part 1-3 / Stage 2 reports unchanged | **PASS** — copied, never edited |
| Evidence Pack checksum manifest | **PASS** |
| Compressed archive checksum | **PASS** |
| Git bundle | **PASS** — `git bundle verify`, complete history |
| Privacy review | **PASS** — no secrets, no participant-identifiable data |
| Stage 2 labelled pending | **PASS** |

Cleaned map hashes, confirmed against the Part 1 report:

| File | Bytes | SHA-256 |
|---|---:|---|
| `Content/L_TranquilMind_Environment.umap` | 74796 | `2880939a21f8720c…` |
| `Content/L_TranquilMind_Void.umap` | 59626 | `35e3e742b479126a…` |

Research canonical regression gate: `6ae4fd8c…97eb`, identical across baseline
A, baseline B, post-Stage-1 and Stage 2.

---

## Working-tree classification

183 changed/untracked files at freeze time.

| Group | Count | Meaning | Git |
|---|---:|---|---|
| A | 33 | Audited runtime baseline | committed |
| B | 79 | Verified Stage 1 / pre-Stage-2 docs and tooling | committed |
| C | 2 | Verified Blueprint cleanup maps | committed |
| D | 14 | Stage 2 work in progress | committed as WIP |
| E | 39 | Raw evidence — Evidence Pack only | excluded |
| F | 16 | Build/cache/generated | excluded |
| G | 0 | Unknown or unrelated | — |

**Group G is empty.** Every file's relationship to the evidence chain was
established, so no item was left unclassified.

Final `git status` is clean: every Group A–D file is committed and every
Group E/F file is covered by `.gitignore`. Nothing was deleted, reset or stashed
to achieve this.

---

## Intentionally excluded from Git

Preserved in the external Evidence Pack instead:

- Complete raw Unreal capture logs (Stage 1 headless and PIE, schema-2 replays,
  Stage 2 automation and headless) — ~24 MB.
- Raw Research JSONL. Two identical runs differ in 11 fields (session GUID, wall
  clock, engine-uptime timestamps, sub-frame ITI rounding), so Spec §1.1
  "byte-identical" is only satisfiable against the canonical projection. The
  canonical outputs **are** committed.
- The two >500 KB generated frame-by-frame table files.
- Pre-cleanup binary map/Blueprint backups.
- The Phase 1 packaging/automation verification run.
- Packaged and archived Android build output, including three ~370 MB `libUnreal.so`
  symbol payloads (~866 MB total).
- Python bytecode caches and one empty scratch file.

---

## Deviations from the preferred commit plan

1. **`TranquilMindTargetActor` (.h/.cpp) and `TargetSpawnerComponent` (.h/.cpp)
   are in commit 5, not commit 1.** The preferred plan puts the audited runtime
   baseline in commit 1, but these four files exist in the working tree **only**
   in their post-Stage-2 state. Placing them in commit 1 would have required
   reverting Stage 2 work, which this task forbids. They are therefore committed
   as WIP with the rest of Stage 2. `TMVisualExitTest.cpp` moved to commit 5 for
   the same reason.
2. **`TMStage1Trace.h/.cpp` are in commit 4 and carry one Stage 2 field.** The
   live schema-2 tracer includes the Stage 2 `livepresentation` field on `SNAP`
   and `NEXT_SPAWN`. It is log-only and changes no runtime behaviour, and it
   cannot be separated without reverting work. Declared rather than hidden.
3. **`.gitignore` is in commit 1.** Not in the preferred plan, but the ignore
   rules must land before later commits for them to be safe.
4. **`Content/Environment/**` is in commit 1, not commit 3.** These are runtime
   assets required by the Demo baseline, so they belong with the baseline rather
   than with the Blueprint-cleanup fix. Commit 3 stays exactly the two maps.
5. **This summary is committed after the tag.** It is a packaging record, not
   verified evidence, so it is deliberately outside
   `evidence/tranquilmind-stage1-prestage2-20260728`.

---

## Known non-blocking findings

- `git diff --cached --check` reports pre-existing trailing whitespace in
  `Docs/Research/Quest_Bubble_Rendering_Research.md:223` and in fenced code
  blocks of `Documentation/Stage1/tools/TRACER_DIFF.md`. All occurrences are
  inside verbatim quoted source. Left unmodified — normalising them would
  corrupt quoted evidence.
- A stale zero-byte `.git/index.lock` dated three weeks before this session
  blocked the first `git add`. No git process was running and nothing held it;
  it was removed after verification.
- `.gitattributes` already routed `*.uasset` / `*.umap` through Git LFS. That
  setup was preserved and **no retroactive LFS migration was performed**.
- A Git bundle stores LFS **pointers**, not LFS **blobs**. A clone from the
  bundle alone yields pointer text for every `.uasset` / `.umap`. Real bytes are
  in the private remote or in the Evidence Pack's binary recovery section.

---

## Remaining Stage 2 acceptance tasks

1. Environment PIE, Demo mode, **≥ 90 seconds**, human observer.
2. Natural-expiry and early-response observations (early response must **not**
   shorten the visible lifetime).
3. Rapid double-trigger observation — exactly one scored outcome.
4. Human confirmation of no two-bubble overlap.
5. Resolve the 206-frame phase-boundary interval against the exact wording of
   Implementation Spec Revision 2. Stage 1 measured 181 frames, Stage 2 measures
   206, ordinary Stage 2 intervals are 216–217; 206 frames at 72 Hz is ≈2861 ms.
   The overlap is fixed, but **no checker-only reclassification may be used to
   hide the discrepancy**.
6. Stage 3 is **not authorised**.

---

## External Evidence Pack

A full Verified Evidence Pack, its compressed archive and the Git bundle were
created **outside** this repository as siblings of the repository directory.
They contain the raw evidence excluded above, per-file SHA-256 manifests, the
privacy review and the full freeze report (`00_MANIFEST/GIT_FREEZE_REPORT.md`).

Absolute paths are deliberately omitted from this tracked summary.
