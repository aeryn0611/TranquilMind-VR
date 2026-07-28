# TranquilMind — Protocol Specification v1 (DRAFT)

**Status:** DRAFT for review. **No code, timing, sequence, or adaptation changes** until this specification is approved. This document specifies *intended* protocols; it does not modify the implementation.
**Depends on:** `GoNoGo_Training_Evidence_Review.md` (evidence + confidence), `Quest_Bubble_Rendering_Research.md` (stimulus legibility), `Current_Implementation_Gap_Analysis.md` (what exists vs this spec).
**Date:** 2026-07-08 · **Revised:** 2026-07-08 (user decisions: adolescent target, discrete flashed stimuli, response-window-only adaptation, scientific-EDA roadmap, MCI future track)

> **Positioning (unchanged, restated):** exploratory HCI/VR research prototype; evidence-informed attention/inhibitory-control training *concept*. **Not** a clinical treatment, diagnostic, or validated digital therapeutic. Physiology is an **exploratory interaction input**, not a medical measurement.

> **All numerical values in this document are PILOT DEFAULTS / STARTING VALUES**, chosen to be defensible, not validated as optimal. Every one is subject to change after piloting. Do not present any value here as an established or optimal parameter.

---

## 0. Executive summary

Three protocols are specified for three different purposes, and they must not be confused:

- **Protocol A — 120 s Portfolio Demo.** Communicates the system visually. **Not a scientific dose.** Preserves the current demo format, and may keep **moving-target** stimuli for visual appeal. Makes *no* behavioral claims.
- **Protocol B — Single-session research prototype (~12–20 min). Primary target population: adolescents aged approximately 13–17.** Uses **discrete flashed** bubble stimuli, clean block structure, full behavioral + physiology logging, manageable VR exposure. Produces *within-task* data only.
- **Protocol C — Multi-session exploratory program (provisional).** A future study design for the same adolescent population, explicitly requiring ethics approval and pilot validation before any human use.

**A separate future accessibility variant for MCI / early cognitive decline (older adults)** is noted in §7. It is **not** part of the adolescent protocol, is **not** an Alzheimer's treatment mode, and makes no efficacy claim.

Core scientific commitments: prepotent GO (**80:20**, frozen), **discrete flashed** stimuli (fixed location, ~400 ms visibility), **jittered ITI**, **response window as the single adaptive axis**, PES measured-not-enforced, no efficacy/transfer claims, and age-appropriate parameters + ethics for a minor population.

---

## 1. Shared design commitments (apply to B and C; A is exempt from scientific dosing)

| Commitment | Pilot value | Rationale (see Evidence Review §) |
|---|---|---|
| Core GO:NOGO | 80:20 (frozen) | Prepotency; §3.1 |
| Baseline/familiarization ratio | 100:0 → 90:10 warm-up | Establish prepotency first |
| Stimulus presentation (B/C) | **Discrete flashed**, fixed location, **~400 ms** visibility, then disappears | §1.1, §3.2 |
| Response window (adolescent start) | **1200 ms** (adaptive — see §3.3, §4) | §3.3, §5 |
| ITI mean / jitter | 1200 ms, ±30% jitter, applied **after** each trial | §3.4–3.5 |
| Timing jitter | required | §3.5 |
| Adaptation | **response window only**; everything else frozen | §3.10–3.11 |
| Post-error slowing | **measure only** | §3.9 |
| Feedback | subtle, immediate or block-level, non-punitive; held constant within a study | §3.8 |
| Age scaling | **required — target is minors (13–17)** | §5 / §3.12 |
| VR session cap | ≤ ~20 min active + comfort checks | §4 |
| Claims | within-task only; no transfer/clinical | §9 |

### 1.1 Stimulus-presentation model — RESOLVED (discrete flashed for research)

The two modes use **different** stimulus-presentation models, deliberately:

- **Research modes (Protocol B and C): discrete flashed stimuli.** This replaces the previously-unresolved moving-target assumption. Each trial:
  1. Bubble appears at a **fixed spatial presentation location** (a consistent point in front of the participant; not moving, not depth-travelling).
  2. Bubble is **visible for a nominal ~400 ms** (`stimulusDuration`, pilot default), then **disappears** regardless of response.
  3. A **response is accepted within the active response window** (which starts at stimulus onset and is the single adaptive axis, §3.3). The response window may extend beyond the 400 ms visibility — i.e., the participant can still respond during the blank interval up to the window deadline.
  4. After the window closes (or an early response resolves the trial), a **jittered ITI** elapses before the next stimulus onset.
- **Demo mode (Protocol A): may keep moving-target stimuli** (spawn/travel/resolve) for visual appeal. This is cosmetic and non-scientific.

**Consequence:** demo and research builds are **not** the same task. Demo appearance must never be used to infer research behavior, and research analysis must never include demo-mode trials. The gap analysis lists "separate Demo and Research modes" as the first implementation priority precisely because of this split.

Because the flashed model differs from the current moving-target implementation, direct comparison of any future TranquilMind data to standard 2D flashed-GNG norms is *closer* than the moving-target variant would allow, but still a VR variant — document it as such.

---

## 2. Protocol A — 120-second Portfolio Demo

**Purpose:** visually communicate the system to a non-scientific audience. Preserve the current short demo. **Explicitly not a training dose and not a data source.**

| Field | Specification |
|---|---|
| Exact total duration | **120 s** hard stop (matches current `DemoMode_Duration_SEC = 120`) |
| Stimulus-presentation model | **Moving-target** allowed (spawn/travel/resolve) for visual appeal — *different from research modes* |
| Baseline phase | None required (demo). Optional 5 s calm intro. |
| Familiarization trials | 2–3 GO-only targets to show the interaction |
| Number of blocks | 1 continuous demo block |
| Trials per block | Whatever fits 120 s at demo pacing (~30–40) |
| GO:NOGO ratio | Demo-friendly ~75:25 **or** the current alternating pattern **relabeled as demo-only** (see note) |
| Sequence generation | Seeded constrained randomizer (§5) with a fixed demo seed **OR** scripted for visual appeal; must be labeled non-scientific |
| Stimulus duration | Generous (current demo `ResponseWindow_MS = 2500`) for visibility |
| Response window | 2500 ms (current demo value) — comfortable, not scientific |
| ITI/ISI | ~3000 ms (current `DemoMode_ISI_MS = 3000`), lightly jittered for natural feel |
| Rest timing | None |
| Feedback policy | Immediate, friendly (hit/miss visual); purely illustrative |
| Adaptation policy | **None** (fixed) |
| Termination rules | 120 s elapsed → end + summary panel (current behavior) |
| Cybersickness checks | Short enough that formal checks optional; still allow instant exit |
| Data recorded | Minimal; **flag as demo, exclude from any analysis** |
| Intended claims | "This is what the system looks/feels like." |
| Claims NOT allowed | Any performance, training, inhibition, or physiological interpretation |

> **Note:** the current demo/debug path uses **strict GO/NOGO alternation**, which is scientifically invalid (perfectly predictable). For the demo it is *cosmetically* fine, but it must be clearly labeled demo-only and must **not** be reused for B/C. Recommend switching even the demo to the constrained randomizer (§5) with a fixed seed so the demo also looks non-trivial.

---

## 3. Protocol B — Single-session research prototype (~12–20 min)

**Primary target population: adolescents aged approximately 13–17.** All values below are **pilot starting values for adolescents**, not adult defaults and not validated optima. See §5 for the age-specific caution and ethics requirements — these are **mandatory** because the target population is minors.

### 3.1 Timeline (target ~16 min active; adjustable 12–20)

| Phase | Duration (pilot) | Content |
|---|---|---|
| 0. Fit & comfort | ~1–2 min (not counted) | Headset fit, IPD, comfort baseline, SSQ pre-check |
| 1. Physiology baseline (rest) | **60–90 s** | Eyes-open calm, no stimuli; establish GSR baseline (§6) |
| 2. Familiarization | **~10–15 GO-only trials** then ~6 trials at 90:10 | Learn the flashed-bubble interaction; build prepotency; **not scored for inhibition** |
| 3. Baseline behavioral block | **1 block, ~90 s** | 80:20, adaptation frozen; establishes per-participant RT baseline |
| 4. Core training | **~6–8 blocks × ~90 s** (≈ 9–12 min) | 80:20; **response-window** adaptation only; short micro-rest between blocks |
| 5. Cooldown | **~60 s** | No stimuli; physiology return-to-baseline; comfort check |

- **Exact total duration:** configurable; default template ≈ **16 min active**. Keep ≤ 20 min for an adolescent VR session.
- **Number of blocks:** 1 baseline + 6–8 core = **7–9 scored blocks**.
- **Trials per block:** size each block so it yields **≥ 8–10 NOGO trials** → at 80:20, ~**40–50 trials/block**. This is also the **minimum data requirement before the response-window staircase may step** (§3.3, §4).

### 3.2 Parameters (adolescent pilot defaults)

| Field | Specification |
|---|---|
| Stimulus-presentation model | **Discrete flashed** (§1.1): fixed location, ~400 ms visibility, disappears after duration, response accepted within active window, jittered ITI after each trial |
| GO:NOGO ratio | 80:20 (baseline + core, **frozen**); familiarization 100:0 → 90:10 |
| Sequence-generation constraints | §5 (seeded; max-run limits; segment balancing; no-NOGO-first) |
| Stimulus duration (visibility) | **~400 ms** (pilot default, **frozen** — not an adaptive axis) |
| Response window | **Adaptive, adolescent start 1200 ms** (§3.3); bounds 800–1400 ms |
| ITI distribution | mean 1200 ms, **uniform jitter ±30%** (≈ 840–1560 ms), applied **after** each trial, logged per trial (**frozen**) |
| Stimulus visual difficulty | **Frozen** (no noise/discrimination adaptation) |
| Rest timing | ~10–15 s between core blocks; longer if comfort flags |
| Feedback policy | Subtle immediate hit/CR acknowledgment; **non-punitive** on commission; block-level summary optional. Held constant across participants. |
| Adaptation policy | **Response window is the ONLY adaptive axis** (§3.3). GO:NOGO ratio, stimulus duration, ITI mean/jitter, and stimulus visual difficulty are all **frozen**. Evaluation blocks non-adaptive. |
| Termination rules | Normal end after cooldown; **early stop** on comfort/SSQ threshold, HMD removal, or participant/guardian request; abort logged with reason |
| Cybersickness checks | SSQ (or short comfort rating) pre, mid (between core blocks, verbal 0–10), post; stop rule if mid-score exceeds threshold — **applied conservatively for a minor population** |
| Data recorded | Per trial: index, block, stimulus type, seed, scheduled vs realized onset, stimulus-offset time, RT, outcome (hit/omission/CR/commission/void), response-window value in effect, adaptation state, PES flag. Per block: counts, RT median, adaptation step, NOGO count. Physiology: raw + smoothed GSR, quality, timestamps, block/epoch markers (§6). Session: IDs, device, build hash, comfort ratings, abort reason. |
| Intended claims | "Within-session behavioral inhibition proxies and exploratory physiology under a VR discrete-flashed GNG variant, in adolescents." |
| Claims NOT allowed | Training efficacy, symptom change, diagnosis, transfer, GSR→emotion/therapy inferences |

### 3.3 Response-window adaptation (the ONLY adaptive axis)

| Field | Pilot value (adolescent) | Note |
|---|---|---|
| Adaptive axis | **Response window** (ms) | Sole axis; all else frozen |
| Start | **1200 ms** | Pilot default, not validated |
| Bounds | **800–1400 ms** | Hard clamp |
| Step | **50 ms** | Symmetric |
| Rule | **3-down / 1-up (defined on GO trials only)** | **3 consecutive GO-Hits → shorten window by one step (harder); 1 GO-omission → lengthen by one step (easier).** See correctness + counter rows below. |
| Definition of "correct" | **`correct` = GO Hit only** (a response within the current window on a GO trial). **`incorrect` (the 1-up trigger) = GO omission** (no response within the window on a GO trial). | Rationale: the response window only pressures GO timing, so the manipulated variable and the correctness criterion are the **same construct** (timely GO responding). The window converges to the participant's GO reaction-time threshold. **Pooling GO+NOGO is rejected** — at 80:20, "free" correct-rejections would dominate the counter and bias the window shorter, mixing constructs. |
| NOGO trials in the staircase | **Pass-through: NOGO trials neither advance nor reset the run counter.** "3 consecutive" means **3 consecutive GO-Hits**, skipping over any NOGO trials between them. | NOGO outcomes (correct rejection / commission) are **scored and logged** as the primary inhibition measures, but do **not** drive the window staircase. Commission errors are therefore kept unpolluted by adaptation. |
| Counter reset | A GO-omission resets the GO-Hit run counter to 0 (and triggers 1-up). A GO-Hit increments it; reaching 3 triggers 3-down and resets to 0. NOGO trials leave it unchanged. | Deterministic; log the counter value per trial. |
| Data gate | **May not step until the block has ≥ ~40–50 trials AND ≥ 8–10 NOGO trials** | Prevents adapting on tiny/noisy samples |
| Evaluation blocks | **Non-adaptive** (window frozen at a fixed value) | For change-over-time comparability |
| Direction convention | shorter window = harder | Log every step + the window value in effect per trial |

> **Status of this rule:** the GO-only correctness definition and NOGO pass-through counter are now the specified defaults, but the **numeric** values (start 1200 ms, bounds 800–1400 ms, step 50 ms, run length 3) remain pilot defaults. The full rule (definition + numbers) **must be pre-registered before any scored use** (§11). An alternative that deliberately couples inhibition — treating a commission error as an `incorrect`/1-up event to relieve time pressure — was considered and **not adopted**, because it makes the window reflect both speed and inhibition and sacrifices interpretability; revisit only if a study specifically wants inhibition to drive difficulty.

**Frozen while the window adapts:** GO:NOGO ratio, stimulus duration (~400 ms), ITI mean/jitter, stimulus visual difficulty. This single-axis design is what preserves interpretability (Evidence Review §3.11): any performance change can be attributed to the one moving parameter.

> The 3-down/1-up rule, start value, bounds, and step are **pilot defaults**, not established optima. The exact staircase rule and the correct/incorrect mapping are an **unresolved scientific decision** to lock before scored use (§9).

---

## 4. Protocol C — Multi-session exploratory program (PROVISIONAL)

**Same population (adolescents 13–17). Provisional and requiring ethics approval and pilot validation before any human use.** Values are placeholders derived from weak-design literature (Evidence Review §3.7, §6) and must be revisited after B piloting. **All values are pilot defaults.**

| Field | Provisional specification |
|---|---|
| Population | Adolescents ~13–17 (as Protocol B) |
| Exact total duration | Per session ≈ Protocol B (≤ 20 min active) |
| Stimulus-presentation model | **Discrete flashed** (as B) |
| Baseline phase | Session 1 full baseline; brief re-baseline each session |
| Familiarization | Full in session 1; abbreviated refresh thereafter |
| Number of blocks | ~6–8 core/session |
| Trials per block | As B (≥ 8–10 NOGO/block) |
| GO:NOGO ratio | 80:20 core, **frozen** across the program unless ratio *is* the studied variable |
| Sequence generation | §5, **separate training vs evaluation seeds**; evaluation blocks fixed & non-adaptive |
| Stimulus duration | ~400 ms (frozen, as B) |
| Response window | Adaptive **response-window only** (as B), age-scaled; evaluation blocks frozen |
| ITI | As B (jittered, frozen) |
| Rest timing | As B; plus between-session spacing (e.g., not >1 session/day) |
| Feedback policy | As B, constant across program |
| Adaptation policy | **Response window only**, within a session; evaluation blocks non-adaptive |
| # Sessions | Provisional **10–20** over several weeks (Evidence Review §3.7) — to be justified by power analysis at study design |
| Cybersickness checks | Every session (pre/mid/post); cumulative-exposure monitoring; conservative for minors |
| Data recorded | As B, plus cross-session linkage, adherence, and **fixed evaluation-block** performance for change-over-time |
| Intended claims | Feasibility, acceptability, adherence, within-task learning curves — **exploratory** |
| Claims NOT allowed | Clinical efficacy, ADHD symptom improvement, diagnosis, far transfer. Requires **active control + blinded outcomes** before any efficacy question is even posed (Evidence Review §8). |

**Mandatory pre-conditions for C:** IRB/ethics approval; **guardian informed consent + adolescent assent** (minor population); pilot validation of parameters via B; pre-registration of any hypothesis (including the staircase rule); defined active control and blinded outcome plan for any efficacy aim.

---

## 5. Age-specific caution & ethics — adolescents 13–17 (mandatory)

Protocol B/C target **minors**. This is not optional overhead.

- **Ethics/consent:** requires IRB/ethics approval, **guardian consent and adolescent assent**, age-appropriate information sheets, and a clear withdraw-any-time process. A guardian should be present/available for VR sessions.
- **Parameters are age-scaled, not adult defaults.** Adolescents make more commission errors and have still-maturing inhibition relative to adults (Evidence Review §3.12, §5). The **1200 ms** starting response window and **1400 ms** upper bound are deliberately more generous than adult values (~1000 ms) for this reason. Do **not** substitute adult timing.
- **All timing values are pilot starting values** to be confirmed by adolescent piloting — not carried over from adult literature as-is.
- **VR safety for minors:** conservative cybersickness/comfort monitoring, shorter caps, headset fit for smaller heads/IPD, and stronger stop rules than an adult study would use.
- **Data protection:** minor participant data requires stricter handling/retention/anonymization.
- **No clinical framing to participants or guardians:** this is research on an exploratory prototype, not treatment.

---

## 6. Physiology (GSR/EDA) role & scientific-EDA roadmap

**Principle:** GSR is an **exploratory interaction input**, not a measurement of stress, calm, arousal, diagnosis, or therapeutic success. It must **not** drive difficulty in v1. The **future goal is scientific EDA**, reached via an explicit roadmap — not by relabeling the current stream.

### 6.1 Three explicitly separate use cases (do not conflate)

1. **Current 1 Hz demo stream.** A live number/needle for the demo panel. **Adequate for demo visualization only.** This is what exists today (latest-sample UDP, port 4210).
2. **Coarse tonic trend monitoring.** Block/epoch-boundary tonic (SCL) drift vs baseline over 30–90 s epochs. 1 Hz can capture *gross* trend only; quality-gated and clearly labeled **coarse / exploratory**. No phasic analysis, no closed-loop control.
3. **Future scientific EDA.** Proper electrodermal analysis (tonic SCL + phasic SCRs, amplitude/latency/rise-time). **Not achievable at 1 Hz** and **not achievable by sample-rate alone** (see §6.3). This is a future track, not a v1 capability.

### 6.2 Allowed roles in v1
- **Collect continuously** through the session (baseline → cooldown).
- **Establish a baseline** from the rest phase (§3.1 phase 1) with a quality gate.
- **Slow trends only**, computed at block/epoch boundaries; in v1, prefer **logging/visualization only** — **no closed-loop difficulty from GSR**.

### 6.3 Future scientific-EDA roadmap (requirements)

Practical prototype firmware target: **~32 Hz** sampling for the ESP32 GSR path (enough to resolve phasic SCR shape/latency for a prototype). **Raising the rate to 32 Hz alone does NOT make the system research-grade EDA.** Achieving scientific EDA additionally requires:

- **Raw sample preservation** (no lossy on-device smoothing-only; keep the raw stream).
- **Board (device) timestamps** on every sample.
- **Packet sequence numbers** to detect loss/reordering.
- **Batched UDP transport** (multiple timestamped samples per packet) rather than one-sample-per-packet, to sustain 32 Hz reliably over Wi-Fi.
- **Calibrated conductance units** (microsiemens) if feasible, not raw ADC counts.
- **Electrode placement documentation** (site, type, prep) recorded per session.
- **Motion-artifact and signal-quality flags** per sample/epoch.
- **Synchronization with stimulus and response events** (shared clock / event markers aligned to trial onsets and responses).
- **Validation against a research-grade EDA device** (concurrent recording, agreement analysis) before any scientific EDA claim.

> The ESP32 firmware change to reach 32 Hz + batching is **explicitly out of scope for this document** (no firmware to be modified now); it is flagged as a separate future task. Until validated, describe physiology as exploratory/low-rate. **No GSR-driven adaptation in v1** under any use case.

### 6.4 Data preservation (always, even in v1)
Preserve, with aligned timestamps: **raw** GSR, **smoothed** GSR, **quality** flag, **device timestamps**, **behavioral events** (trial onsets, offsets, responses, outcomes), and **block/epoch markers**. Never store only a derived index.

### 6.5 Prohibited (v1)
- GSR driving trial/second-level difficulty.
- Inferring diagnosis, stress, calmness, emotion, or therapeutic success from GSR.
- **Fusing input-pressure/interaction artefacts with physiological arousal** into one "arousal" number.
- Claiming scientific-EDA adequacy of the current 1 Hz stream, or of 32 Hz alone.

---

## 7. Future Accessibility Variant for MCI / Early Cognitive Decline

**This is a separate, future track. It is NOT part of the adolescent ADHD protocol, is NOT merged with it, and is NOT an Alzheimer's treatment mode. No treatment or efficacy claims of any kind.**

Intended as a **separate protocol** for older adults with mild cognitive impairment (MCI) / early cognitive decline, prioritizing accessibility, usability, and feasibility — not inhibition training dose and not clinical outcome.

- **Separate protocol**, not shared parameters with the adolescent work; different population, different goals, different ethics pathway.
- **Slower response windows** (longer than adolescent values; generous, non-time-pressured).
- **Simpler instructions** (minimal steps, plain language, repeated/optional re-explanation).
- **Lower visual and memory load** (fewer stimuli, higher contrast/legibility, no reliance on remembering complex rules).
- **Clinician/caregiver involvement** in setup and supervision.
- **Stronger comfort and cybersickness monitoring** (older adults may be more susceptible; frequent checks, shorter exposure, easy exit).
- **Usability and feasibility first** — the research question is "can this population use it comfortably," not "does it improve cognition."
- **Explicitly no Alzheimer's treatment, no cognitive-improvement, and no efficacy claims.** Framing is accessibility/feasibility research on an exploratory prototype.

Full parameterization is deferred; this section exists to record intent and boundaries so the population is never accidentally folded into the adolescent protocol.

---

## 8. Sequence generation — reproducible constrained randomization

**Requirement:** GO/NOGO order must **not** be simple alternation and must be reproducible, seeded, and non-misleading. This replaces both the current memoryless Bernoulli draw (no run control) and the demo/debug strict alternation.

### 8.1 Algorithm (deterministic, seeded)

Inputs (all configuration data, not hard-coded):
```
seed                : uint64      // reproducible; separate training vs evaluation seeds
nTrials             : int         // trials in the block
targetNoGoRatio     : float       // 0.20 (exact; frozen)
maxConsecutiveGo    : int         // 6   (provisional engineering constraint)
maxConsecutiveNoGo  : int         // 1   (provisional engineering constraint)
minGapAfterNoGo     : int         // 1   (>=1 GO between NoGos)
forbidNoGoFirstN    : int         // 1   (no NOGO on the first trial)
segmentCount        : int         // 3   (balanced early/middle/late)
```

Procedure:
1. Compute `nNoGo = round(nTrials * targetNoGoRatio)`; `nGo = nTrials - nNoGo`. This **fixes the exact NOGO count** (no ratio drift, unlike Bernoulli) — the advertised **80:20 is the real 80:20**.
2. Partition the block into `segmentCount` contiguous segments; distribute `nNoGo` across segments as evenly as possible (balanced early/middle/late). Within-segment NOGO positions chosen by the seeded RNG.
3. Place NOGO trials by seeded sampling subject to constraints:
   - not within `forbidNoGoFirstN` of the start (**no NOGO as the first trial**),
   - never a NOGO run longer than `maxConsecutiveNoGo` (default 1),
   - at least `minGapAfterNoGo` GO trials after a NOGO,
   - never a GO run longer than `maxConsecutiveGo` (default 6).
4. If the constraint set is infeasible, **fail loudly** (log + assert) rather than silently relaxing.
5. Emit the full ordered schedule and **log it** (seed + parameters + resulting sequence hash) so the exact block can be replayed.
6. Same seed + parameters ⇒ byte-identical schedule (verified by hash).

### 8.2 Determinism & replay
- Named, documented RNG (e.g., `FRandomStream` or a PCG variant) seeded once per block from `(sessionSeed, blockIndex)`.
- Persist `seed`, all constraint parameters, `nTrials`, and the emitted sequence to the trial log. Replay = re-run with the same inputs; assert the hash matches.
- **Separate seeds:** derive training-block and evaluation-block seeds from **independent** master seeds so evaluation sequences are never accidentally identical to trained ones.

### 8.3 Defaults & design answers

- **Exact ratio:** 80:20, enforced by exact NOGO count per block (frozen).
- **`maxConsecutiveGo = 6`**, **`maxConsecutiveNoGo = 1`**, **balanced early/middle/late segments**, **no NOGO as first trial** — these are the defaults. **Run-length values are provisional engineering constraints**, not psychophysically-optimized values; adjust after piloting.
- **Consecutive NOGO?** Default **no** (`maxConsecutiveNoGo = 1`): at 80:20 the point is a rare NOGO interrupting a GO habit; back-to-back NOGO is rare and dilutes prepotency. Allow at most 2 only for a specific study, set explicitly.
- **First block easier ratio?** Familiarization is GO-only → 90:10; the first *scored* block is standard 80:20 with adaptation **frozen** (don't bias the baseline).
- **Ratios change between blocks?** No — 80:20 frozen so blocks are comparable and the response window is the only moving part.
- **How adaptation preserves interpretability:** response-window is the sole axis; everything else frozen; evaluation blocks non-adaptive; full adaptation trajectory logged.

### 8.4 "No misleading sequences" guarantees
- Exact NOGO count per block (no drift) → advertised ratio = real ratio.
- Segment balancing → NOGO not clustered only early/late.
- Run-length caps + min-gap → no exploitable periodicity, no accidental alternation, no impossible bursts.
- First-trial NOGO prohibition → no penalizing a participant before prepotency exists.

---

## 9. Recommended default values (consolidated) + adjustment ranges

**All values are PILOT DEFAULTS, not validated optima.**

| Parameter | Pilot default | Range | Confidence |
|---|---|---|---|
| Core GO:NOGO | 0.80 : 0.20 (frozen) | 0.75–0.80 GO | High |
| Stimulus presentation (B/C) | Discrete flashed, fixed location | — | High (decision made) |
| Stimulus duration (visibility) | 400 ms (frozen) | 250–500 ms | Medium |
| Response window (adolescent start) | 1200 ms (adaptive) | 800–1400 ms bounds | Medium |
| Response-window step | 50 ms | 25–100 ms | Medium |
| Staircase rule (structure) | 3-down/1-up; **correct = GO Hit only; NOGO pass-through** (§3.3) | — | High (structure) / Low–Med (numbers) |
| ITI mean | 1200 ms (frozen) | 1000–2000 ms | Medium |
| ITI jitter | ±30% uniform (frozen) | ±20–40% | High (that jitter is needed) |
| Trials/block | ~45 (≥8–10 NOGO) | 40–80 | Medium |
| Scored blocks (B) | 7–9 | 5–10 | Medium |
| Active session time (B) | ~16 min | 12–20 min | Medium |
| maxConsecutiveGo | 6 (provisional) | 4–8 | Medium |
| maxConsecutiveNoGo | 1 (provisional) | 1–2 | Medium |
| Adaptive axes active | **1 (response window)** | 1 | High |
| GSR closed-loop control | none (v1) | none | High |
| Future EDA firmware rate | 32 Hz target | ≥ 32 Hz | Medium (necessary, not sufficient) |

---

## 10. Claim boundaries (restate on every build/report)
Allowed: exploratory, evidence-informed *design*, within-task behavioral proxies, feasibility, physiology-as-interaction-input.
Not allowed: treats/diagnoses ADHD; efficacy; transfer; GSR→emotion/therapy; training-block metrics as diagnostic scores; MCI variant as Alzheimer's treatment. (Full list: Evidence Review §9.)

---

## 11. Unresolved scientific decisions (require user / supervisor)
1. **Staircase rule details — RESOLVED (structure):** `correct = GO Hit only`, `incorrect (1-up) = GO omission`, **NOGO trials pass-through** in the run counter (§3.3). GO+NOGO pooling and commission-driven adaptation were both rejected. **Still to lock and pre-register: the numeric values** (start 1200 ms, bounds 800–1400 ms, step 50 ms, run length 3) after the first adolescent pilot.
2. **Fixed presentation location vs multiple locations:** spec currently uses a single fixed location; decide whether spatial variability is desired later (would add a spatial-attention component and change the construct).
3. **Response window vs stimulus-visibility coupling:** confirm response accepted during the post-offset blank up to the window deadline (current spec) vs response only while visible.
4. **Feedback style:** trial-level vs block-level; non-punitive commission signal.
5. **Staircase start/bounds/step** for adolescents — pilot values only; confirm after first pilot.
6. **Adolescent recruitment/ethics pathway** and guardian-consent logistics.
7. **EDA firmware upgrade** (32 Hz + batching + timestamps + sequence numbers) — separate future task; confirm intent and validation-device availability.
8. **MCI variant** parameterization — deferred; confirm when/if that track is opened.

---

## Sources
See `GoNoGo_Training_Evidence_Review.md` §Sources for the full primary-literature list underpinning every parameter here (ratios, timing, adaptation, PES, age scaling incl. adolescents, VR session limits, transfer limitations). Implementation facts (current ISI/window/generation/staircase/GSR) are cited in `Current_Implementation_Gap_Analysis.md`.
