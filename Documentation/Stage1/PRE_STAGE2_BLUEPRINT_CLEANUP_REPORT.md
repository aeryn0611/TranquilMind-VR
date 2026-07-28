# PRE-STAGE-2 PART 1 — LEVEL BLUEPRINT DEAD-PATH CLEANUP

**Date:** 2026-07-28
**Scope:** Option B — delete the redundant Level Blueprint call chains only.
**Status: COMPLETE.** All gates pass, no items outstanding.

The edit was applied manually in the Unreal Editor (the deletion is not
scriptable — see §2). Every measured behaviour is identical to the pre-cleanup
baseline.

| Gate | Result |
|---|---|
| Backups intact | **PASS** |
| Edited map hashes changed | **PASS** |
| Headless Environment | **PASS** — identical to baseline |
| Headless Void | **PASS** — identical to baseline |
| Automation | **23/23 PASS** |
| Research canonical regression | **PASS** — sha256 unchanged |
| Stimulus sequence + spawn timing | **PASS** — bit-identical |
| **Environment PIE** | **PASS** — 0 `Accessed None`, 0 other Blueprint runtime errors |
| **Void PIE** | **PASS** — 0 `Accessed None`, 0 other Blueprint runtime errors |

**PART 1 OVERALL: COMPLETE.**

Stage 1 findings are untouched. No runtime code and no C++ registration path was
modified. The overlap defect is untouched and remains deferred to Stage 2.

---

## 1. Backups and integrity record

Created **outside** the active `Content/` directory, before any other action:

```
Backups/PreStage2_BlueprintCleanup_20260728/
  L_TranquilMind_Environment.umap
  L_TranquilMind_Void.umap
  BP_TranquilMindRuntime.uasset
  MANIFEST.txt
```

Copied with `cp -p` (timestamps preserved).

| File | Bytes | mtime (UTC) | SHA-256 |
|---|---:|---|---|
| `Content/L_TranquilMind_Environment.umap` | 84197 | 2026-07-22T10:20Z | `04257e189c61c04dfe7fa988bdf0bfa0ffe654ca4960bd0f7546485dcdabb41e` |
| `Content/L_TranquilMind_Void.umap` | 75365 | 2026-06-08T08:36Z | `c13080f435c36015e9062ae43c74e917a1d59e97ce44d36dfc0a2138a4691ecd` |
| `Content/BP_TranquilMindRuntime.uasset` | 24775 | 2026-05-18T07:24Z | `70731fb2fec395f76a92fef00aebf5cd116c79868f61193cc3a6b5165ba4d048` |

Both `.umap` files are **untracked in git** — this manifest and the backup copy
are the only recovery path. `BP_TranquilMindRuntime.uasset` is tracked
(`9d437a6`) and is included for completeness; it is **not** a target of this edit.

Verify at any time:

```bash
shasum -a 256 Content/L_TranquilMind_Environment.umap Content/L_TranquilMind_Void.umap
```

---

## 2. Why the edit had to be manual

UE's Python API does not expose Blueprint graph nodes. Verified directly against
UE 5.7 rather than assumed:

- `unreal.load_object(...)` **does** return the `LevelScriptBlueprint`.
- `unreal.BlueprintEditorLibrary.find_event_graph(bp)` **does** return the
  `EdGraph` object.
- That `EdGraph` exposes **no node collection and no node accessors** — its full
  Python surface is generic `UObject` methods (`get_editor_property`, `modify`,
  `rename`, …). There is no `nodes` property.
- `Blueprint.ubergraph_pages` and `Blueprint.new_variables` are **not exposed**
  (`Failed to find property 'ubergraph_pages' … on 'LevelScriptBlueprint'`).
- `BlueprintEditorLibrary`'s only removal functions are **graph-level**:
  `remove_graph`, `remove_function_graph`, `remove_unused_nodes`,
  `remove_unused_variables`. None can target a specific node chain.

`remove_unused_nodes` is **not** a substitute: the target nodes are connected to
`ReceiveBeginPlay` and are therefore "used". Calling it would be an untargeted
change to the graph, which Option B forbids.

Direct binary editing of the `.umap` was rejected as unsafe and unverifiable.

**Conclusion: the deletion is a GUI operation.** Reporting this rather than
approximating it, because an untargeted or partial graph edit on an untracked
binary asset is exactly the failure mode the backups exist to guard against.

---

## 3. Node inventory — what to delete

Extracted from both map binaries. **Both maps carry the same wiring**, so Void
has the same latent stale reference; it has simply never been PIE'd (see
`RUNTIME_BLOCKER_AUDIT.md`).

Symbols present in **both** `L_TranquilMind_Environment.umap` and
`L_TranquilMind_Void.umap`:

```
ReceiveBeginPlay            ResumeHardGate_Event        ResumeStaircase_Event
RegisterSessionManager      ApplyStaircaseParams
OnHardGateResumed           OnStaircaseParamsUpdated
K2Node_Literal              ← the hard level-actor reference (the stale one)
K2Node_VariableGet          ← reads TargetSpawner off it
K2Node_CallFunction         K2Node_AddDelegate          K2Node_CreateDelegate
K2Node_CustomEvent ×3       K2Node_Event                K2Node_InputKey / InputKeyEvent ×2
```

The chain to remove, per Option B:

```
K2Node_Literal (BP_TranquilMindRuntime_C_1)   ← stale hard actor reference
        └─> K2Node_VariableGet (TargetSpawner)
                ├─> CallFunction RegisterSessionManager
                └─> CallFunction ApplyStaircaseParams
```

**Do not** repair the reference, add `IsValid`, add `GetActorOfClass`, or touch
the C++ path — all four are explicitly out of scope. Delete the dead chain only.

Leave alone: `ResumeHardGate_Event`, `ResumeStaircase_Event`, the
`InputKey`/`InputKeyEvent` nodes, and anything not fed by the stale literal.

---

## 4. Editor steps

1. Close PIE. Open `L_TranquilMind_Environment`.
2. **Blueprints → Open Level Blueprint.**
3. In `EventGraph`, locate the `BP_TranquilMindRuntime` blue reference node
   (`K2Node_Literal`) and the `TargetSpawner` getter fed by it.
4. Select **only**: that reference node, the `TargetSpawner` getter, and the
   `RegisterSessionManager` and `ApplyStaircaseParams` call nodes.
5. Delete. Re-link the `ReceiveBeginPlay` exec pin to whatever followed the
   deleted calls, so the remaining `BeginPlay` chain stays connected.
6. **Compile.** Expect zero errors and zero warnings. Save the level.
7. Repeat 1–6 for `L_TranquilMind_Void`.

If the compile reports anything unexpected, stop and restore from
`Backups/PreStage2_BlueprintCleanup_20260728/` rather than improvising.

---

## 5. Pre-cleanup validation baseline

Captured **before** any edit, so the post-edit run is a like-for-like comparison.
Headless, fixed 72 fps, `OperatingMode=Demo`.

| Signal | Environment | Void |
|---|---:|---:|
| `TMS1` lines | 12785 | 200 |
| `SPAWN` events | 40 | 40 |
| `Spawned ONE target` | 40 | 40 |
| `LogScript` lines | 0 | 0 |
| `Accessed None` | 0 | 0 |

The Environment/Void `TMS1` asymmetry is expected and not a defect: Void does not
match `UseEnvironmentDemoVisualProfile()`, so `VisualMotion` stays inert, no exit
runs, and only `SPAWN` / `OUTCOME` / `DESTROY_REQ` / `ENDPLAY` / `NEXT_SPAWN`
are emitted (5 × 40 = 200). Environment additionally emits `SNAP`,
`MAT_WRITE`, `DELEGATE_*`, `EXIT_*` and `MID_CREATE`.

Also on record from Stage 1, unchanged and re-runnable as the post-edit gate:

```
Automation                     23/23 PASS
Research canonical sha256      6ae4fd8c2e630806b089615c612b03d1cf41b013c5b566da9393960b21ac97eb
```

Note that `Accessed None` is **0 in headless for both maps** — the fault is
PIE-only, so **headless cannot prove the cleanup worked.** Only a PIE run can.
That is why PIE on both maps is required — see §7.7.

---

## 6. Effective Demo parameters — the `ResponseWindow=1500` log line

The line

```
[TargetActor] Initialized | Type=0 | SpawnTime=2.042 | Speed=50.0 | ResponseWindow=1500.0 | Scale=1.00 (TEMP)
```

is emitted **inside `InitializeTarget()`, before the Demo overrides are applied.**
`UTargetSpawnerComponent::SpawnNextTarget()` pushes the Demo presentation values
immediately afterwards, in its `if (bDemoMode)` block, and logs the result:

```
[DemoPresentation] SpawnDist=350.0 cm | Speed=20.0 cm/s | Travel=50.0 cm |
                   Scale=0.45 | VisualRadius=22.50 cm | PredEndCentre=300.0 cm |
                   PredNearestSurface=277.5 cm | AngDiam=7.37 deg -> 8.60 deg
```

So the `1500.0` / `50.0` / `1.00` values are **transient pre-override state, not
effective values.** The logging order is misleading but the behaviour is correct.

**Effective response window, measured from the trace (not read from config):**

```
SPAWN -> OUTCOME frame delta : 180-181 frames  in 39 of 40 cycles
SPAWN -> OUTCOME world time  : 2500-2514 ms
180 frames @ 72 fps          = 2500.0 ms
```

**Effective response window = 2500 ms** (`DemoMode_ResponseWindow_MS`). The
181-frame / 2514 ms variant is one frame of quantisation, not a second value.

The single remaining cycle (cycle 40) resolves at 103 frames / 1431 ms with
outcome `Void`. That is the run being force-stopped before the session terminal
state — `EndPlay` → `DestroyAllActiveTargetsAsVoid()`. It is **identical in the
pre-cleanup and post-cleanup captures** and is the headless analogue of the
manually-stopped PIE session. Not a defect, and not introduced by this cleanup.

Effective values confirmed: spawn distance **350 cm**, speed **20 cm/s**, scale
**0.45**, response window **2500 ms** — matching the observer's PIE report.

*Noted for a future pass, not changed here:* `InitializeTarget()`'s log line
reports values that are superseded microseconds later. Moving or amending it
would be a code change and is out of Part 1 scope.

---

## 7. Post-edit validation results

### 7.1 Backups intact

`shasum -a 256 -c` against `MANIFEST.txt`:

```
L_TranquilMind_Environment.umap: OK
L_TranquilMind_Void.umap:        OK
BP_TranquilMindRuntime.uasset:   OK
```

### 7.2 Edited map hashes changed

| File | Bytes before → after | SHA-256 before → after |
|---|---|---|
| `L_TranquilMind_Environment.umap` | 84197 → **74796** (−9401) | `04257e18…` → `2880939a21f8720c…` |
| `L_TranquilMind_Void.umap` | 75365 → **59626** (−15739) | `c13080f4…` → `35e3e742b479126a…` |
| `BP_TranquilMindRuntime.uasset` | 24775 → 24775 | `70731fb2…` → `70731fb2…` **unchanged (correct — not a target)** |

Both maps shrank, consistent with node deletion.

**Dead-path symbols removed from both maps:**

```
                                  Environment   Void
RegisterSessionManager                 0          0     (was present in both)
ApplyStaircaseParams                   0          0     (was present in both)
```

**Delegate machinery correctly preserved in both:** `ReceiveBeginPlay`,
`ResumeStaircase_Event`, `ResumeHardGate_Event`, `OnHardGateResumed`,
`OnStaircaseParamsUpdated`, `K2Node_AddDelegate`, `K2Node_InputKey(Event)`.

**Residual name-table entry in Environment — CLOSED, inert, no action required.**
Environment still carries `BP_TranquilMindRuntime_C_1_ExecuteUbergraph_…_RefProperty`
in its name table; Void does not. Closed on three independent grounds:

1. **Both dead function calls are absent** — `RegisterSessionManager` and
   `ApplyStaircaseParams` return 0 occurrences in both maps (§7.2).
2. **Environment PIE is clean** — 0 `Accessed None`, 0 other Blueprint runtime
   errors (§7.7). Nothing dereferences the entry at runtime.
3. **Runtime behaviour and all regression gates are unchanged** — headless traces
   bit-identical, sequence and cadence bit-identical, automation 23/23, Research
   canonical sha256 unchanged (§7.3–7.6).

A name-table entry with no referencing node is inert package data. It would be
dropped by a future full resave; forcing one now would be an unnecessary write to
an untracked binary asset. Recorded as an asymmetry between the two maps, not a
defect. **No further map modification.**

### 7.3 Headless runs — identical to the pre-cleanup baseline

| Signal | Environment pre → post | Void pre → post |
|---|---|---|
| `TMS1` lines | 12785 → **12785** | 200 → **200** |
| `SPAWN` events | 40 → **40** | 40 → **40** |
| `Spawned ONE target` | 40 → **40** | 40 → **40** |
| `LogScript` lines | 0 → **0** | 0 → **0** |
| `Accessed None` | 0 → **0** | 0 → **0** |

### 7.4 Automation

```
pass=23  fail=0     → 23/23 PASS
```

### 7.5 Research canonical regression

```
baseline sha256 : 6ae4fd8c2e630806b089615c612b03d1cf41b013c5b566da9393960b21ac97eb
post-P1  sha256 : 6ae4fd8c2e630806b089615c612b03d1cf41b013c5b566da9393960b21ac97eb
RESULT: PASS — byte-identical
```

### 7.6 Stimulus sequence and spawn timing — bit-identical

```
stimulus sequence identical : YES
  pre  : 0101010101010101010101010101010101010101
  post : 0101010101010101010101010101010101010101

spawn count            pre=40  post=40
spawn frame deltas     pre : {181:1, 216:35, 217:3}
                       post: {181:1, 216:35, 217:3}
delta sequence identical : YES

outcome distribution   pre : Omission 20, CorrectRejection 19, Void 1
                       post: Omission 20, CorrectRejection 19, Void 1
```

The anomalous 181-frame interval is the Phase II boundary documented in Stage 1
§3.3 and is preserved exactly — confirming the cleanup changed no timing.

### 7.7 PIE validation — both maps PASS

Observer-confirmed. This is the only test that can prove the cleanup worked:
the fault was PIE-only, so headless reads 0 errors on both maps regardless (§5).

| | Environment PIE | Void PIE |
|---|---|---|
| `Accessed None` on `TargetSpawner` | **0** | **0** |
| Other Blueprint runtime errors | **0** | **0** |
| `TargetSpawner` registered | — (C++ path logged) | **YES** |
| Targets continue spawning | **YES** | **YES** |
| Demo presentation parameters active | spawn 350 cm, speed 20 cm/s, scale 0.45 | — |
| Stopped before terminal session state | yes (manual) | yes (manual) |

Both sessions were stopped manually before the terminal state, producing the
`Resolved as Void` / `EndPlay before terminal state` / `Compliant=NO` tail. That
is the forced-teardown case characterised in §6 — present identically in the
pre-cleanup captures and **not** a runtime defect.

The stale reference is gone from the executing path in both maps, while the C++
registration path (`UTargetSpawnerComponent::BeginPlay`,
`ATranquilMindVRPawn::AutoBindRuntimeReferences`) continues to register the
spawner exactly as before — which was the entire premise of Option B: the deleted
Blueprint calls were redundant with C++, so removing them changes nothing.

### 7.8 Post-validation integrity re-check

Re-verified after both PIE sessions, confirming PIE did not modify either map:

```
L_TranquilMind_Environment.umap   2880939a21f8720c…   74796 bytes   unchanged
L_TranquilMind_Void.umap          35e3e742b479126a…   59626 bytes   unchanged
BP_TranquilMindRuntime.uasset     70731fb2fec395f7…   24775 bytes   unchanged
Backups (shasum -c)               all three OK
```

---

## 8. Scope confirmation

Unchanged, as required:

- Stale hard reference **not repaired**
- No `IsValid` added · no `GetActorOfClass` added
- C++ registration path untouched (`UTargetSpawnerComponent::BeginPlay`,
  `ATranquilMindVRPawn::AutoBindRuntimeReferences`)
- No runtime code modified
- Visual behaviour unchanged — headless traces bit-identical
- **Overlap not fixed** — remains deferred to Stage 2
- Stage 1 report not altered

```
PART 1 COMPLETE — all gates PASS, no items outstanding.
Maps final: no further modification.
PART 2 NOT STARTED.  PART 3 NOT STARTED.
Nothing committed.  Nothing packaged.
```
