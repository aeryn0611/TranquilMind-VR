# GO/NOGO Inhibitory-Control Training — Evidence Review

**Project:** TranquilMind (exploratory VR inhibitory-control / attention research prototype related to ADHD)
**Status:** DRAFT — research only. No timing, sequence, or adaptation code to change until approved.
**Author:** Evidence review pass, 2026-07-08 · **Revised:** 2026-07-08 (user decisions applied)
**Scope:** Summarize primary literature on GO/NOGO task and inhibitory-control *training* parameters to inform a scientifically-defensible protocol. This is not a clinical document.

> **Claim boundaries (apply to the whole document).** TranquilMind is an exploratory HCI/VR research prototype. It is **not** a clinical treatment, **not** a diagnostic instrument, and **not** a validated digital therapeutic. Nothing here should be read as evidence that TranquilMind improves ADHD symptoms or produces far transfer. The literature is used to make task *design* defensible, not to make efficacy claims.

> **Design decisions adopted (2026-07-08) — this review now reads against these.** (1) **Primary population = adolescents ~13–17** (a minor population; ethics + age-scaling mandatory, §3.12/§5). (2) Research modes use **discrete flashed stimuli** (fixed location, ~400 ms visibility), not moving targets. (3) Core **GO:NOGO = 80:20** (frozen). (4) **Response window is the sole adaptive axis** (adolescent start 1200 ms, bounds 800–1400 ms, step 50 ms, provisional 3-down/1-up). (5) Future physiology goal is **scientific EDA** via a firmware roadmap (32 Hz target, necessary-not-sufficient), not 1 Hz demo only. (6) Older-adult **MCI / early-decline** work is a **separate future accessibility track**, never merged here and never framed as Alzheimer's treatment. Full parameterization lives in `TranquilMind_Protocol_Specification_v1.md`. All numeric values are **pilot starting values**, not validated optima.

---

## 1. Executive summary

- The GO/NOGO task is a well-standardized **measure** of response inhibition; a **training** protocol reuses the same mechanics but with different goals, and the two must not be conflated (§2).
- To train inhibition you need a **prepotent (habitual) GO response**, which requires GO to substantially outnumber NOGO. The most common defensible ratios are **75:25 and 80:20**; 50:50 does not build strong prepotency, and very high GO (90:10) maximizes false alarms but can frustrate and fatigue.[^ratio-erp][^optimal-ratio][^food-gng]
- Typical **stimulus duration ~200–500 ms**, **response deadline ~1000 ms** (often = or slightly less than a fixed trial window), **ISI/ITI ~1000–2000 ms**, frequently **jittered** to prevent temporal prediction.[^cpt-params][^isi-shorter][^predictability][^alcohol-rct]
- Published inhibition-training interventions run on the order of **single sessions of ~10–20 min up to multi-week programs of ~10–20 sessions**; VR ADHD pilots have used ~**20 sessions over 4–6 weeks**.[^vr-adhd-jmir][^alcohol-rct][^online-inhib-rct]
- **The evidence on benefit is weak and contested.** Rigorous, blinded meta-analyses of computerized cognitive training in ADHD find **little to no effect on core symptoms** and **limited far transfer**; near-transfer is inconsistent. This is the single most important caveat for TranquilMind's claims.[^cortese-2023][^cortese-2015][^online-inhib-rct]
- **Adaptive difficulty via staircase is standard**, but **changing several difficulty variables at once destroys interpretability** — you can't attribute a performance change to any one manipulation. Prefer one adaptive axis at a time for research builds (§7, §8). **Adopted design: the single adaptive axis is the response window** (adolescent start 1200 ms; §3.3/§3.10); ratio, stimulus duration, ITI, and stimulus difficulty are frozen.
- **Population adopted = adolescents ~13–17.** Because inhibition is still maturing in this group (adults make fewer commission errors, §3.12), timing is scaled **more generously** than adult defaults, and a **minor-population ethics pathway** (guardian consent + assent) is mandatory (§5).

---

## 2. Diagnostic task vs training protocol — keep them separate

| Aspect | Diagnostic / assessment GO/NOGO (e.g., CPT-style) | Training protocol |
|---|---|---|
| Goal | Estimate an individual's inhibition/attention at one time | Repeatedly exercise inhibition, ideally improving it |
| Parameters | **Fixed, standardized** so scores are comparable across people | **Adaptive** to keep the person near threshold |
| Ratio | Often 50:50 or a fixed published ratio | Higher GO (75:25 / 80:20) to build prepotency |
| Dose | One block/session | Many sessions over weeks |
| Output | A score (commission errors, d′, RT) | A change over time |

TranquilMind is a **training** prototype. **Do not import a diagnostic protocol's fixed parameters and call the result a training dose, and do not report training-block performance as if it were a standardized diagnostic score.** Where this review cites diagnostic-task parameters (e.g., CPT timing), they inform plausible ranges only.[^cpt-params][^phenx-gng]

---

## 3. Parameter-by-parameter synthesis

### 3.1 GO:NOGO ratio and prepotency
A high proportion of GO trials builds a **prepotent** (automatic) response, so the rare NOGO genuinely demands inhibition; this is the mechanistic reason inhibition tasks skew GO-heavy.[^ratio-erp][^optimal-ratio] Ratio directly modulates inhibition-related brain activity and false-alarm rate.[^ratio-erp] Methodological work on "optimal go/no-go ratios to maximize false alarms" shows the false-alarm signal (the thing you want to train against) is strongest at high GO proportions.[^optimal-ratio] Food/alcohol inhibition-training paradigms often go further (up to 90:10 associations) to force strong stimulus–stop pairing.[^food-gng]
- **Common defensible choices:** 75:25, 80:20. 50:50 is a weak prepotency builder (use for baseline/familiarization, not core inhibition training). 90:10 maximizes false alarms but risks boredom/fatigue and very few scored NOGO trials per block.
- **Caution:** *More NOGO does not mean better inhibition training.* Raising NOGO proportion **reduces** prepotency and makes each NOGO less of an inhibition demand. This is a common misconception to avoid.

### 3.2 Stimulus duration
Typical **~200–500 ms**; diagnostic CPTs commonly display each stimulus **200–500 ms**.[^cpt-params] Some paradigms use 100–250 ms. Longer durations ease perception (less discrimination load); shorter durations add perceptual difficulty.
- **Defensible default:** 250–500 ms.

### 3.3 Response window (deadline)
Commonly **~1000 ms**; accuracy scoring frequently treats responses slower than ~1000 ms as omissions.[^cpt-params][^isi-shorter] Alcohol GNG used a **1000 ms** target deadline.[^alcohol-rct] The window trades off omissions (too short) vs weakened prepotency and slow, non-inhibitory responding (too long).
- **Defensible default:** 800–1200 ms for adults; longer for young children (§5).

### 3.4 ISI / ITI
Wide range in the literature: ~**1100–1700 ms**, ~**1000 ms**, and longer designs (2/5/8 s).[^predictability][^food-gng][^isi-general] Shorter ISIs increase response prepotency and time pressure and have been argued to better expose inhibition failures.[^isi-shorter] CPTs commonly use **1–2 s** intervals.[^cpt-params]
- **Defensible default:** 1000–2000 ms.

### 3.5 Fixed vs jittered timing
Predictable stimulus onset lets participants deploy **proactive** control (they prepare), changing what the task measures; jittering onset/ISI reduces temporal prediction and keeps the response reactive.[^predictability]
- **Recommendation:** **jitter** the ISI (e.g., uniform or truncated distribution around the mean) for training and especially for any evaluation block, and log the exact realized timing.

### 3.6 Trials per block, blocks per session
CPTs commonly present **300–400 stimuli** total.[^cpt-params] Training paradigms often use short blocks of a few dozen trials (e.g., food GNG: **3 blocks × 40 stimuli**).[^food-gng] Block length balances enough NOGO trials for a stable estimate against fatigue.
- With 20–25% NOGO, a block needs enough total trials to yield a usable NOGO count (e.g., 40 trials → ~8–10 NOGO). Very short blocks give noisy per-block inhibition estimates — relevant because TranquilMind currently adapts on 30 s blocks (see gap analysis).

### 3.7 Session duration, frequency, number of sessions
- **Single-session** effects on GO RT have been reported after one training session.[^post-error-review]
- **Multi-session** programs: online inhibition-training RCT used **6 sessions**;[^online-inhib-rct] VR ADHD pilot (Floreo) used **up to 20 sessions over 4–6 weeks**;[^vr-adhd-jmir] cognitive-training ADHD trials commonly run multi-week.
- **Frequency:** commonly several sessions/week across a few weeks; no single canonical schedule.
- **VR-specific:** total in-headset time is constrained by cybersickness/fatigue (§4).

### 3.8 Feedback after each outcome
Reward/feedback context measurably reduces later commission errors in children (a "context monitoring" account), so feedback is not neutral — it changes behavior.[^reward-children] Design choices: whether to give trial-level feedback (can aid learning but may distract/slow) vs block-level summary. Correct GO (hit), omission, correct rejection, and commission each can be signaled differently; over-punishing commissions can induce excessive caution (slow, omission-heavy responding).
- **Defensible approach:** subtle, immediate, non-punitive feedback (or block-level feedback) for a research build; log everything, and treat feedback style as a variable to hold constant within a study.

### 3.9 Post-error slowing (PES)
PES — slower RT immediately after an error — is a robust, well-documented adaptive control process.[^post-error-review][^post-error-constant] It is a **behavioral phenomenon to measure, not a rule to enforce.** Forcing slowing (e.g., artificially delaying the next trial after an error) would confound the natural adaptive signal.
- **Recommendation:** **measure** PES (log RT relative to preceding-trial outcome); **do not enforce** it.

### 3.10 Adaptive difficulty methods
Options seen in the literature and practice: **staircase** (e.g., N-down/1-up on accuracy), **response-window adaptation** (shorten deadline as accuracy rises), **ISI adaptation** (shorten to raise prepotency/time pressure), **NOGO-frequency adaptation** (rarer NOGO = stronger prepotency), and **stimulus-discrimination difficulty** (perceptual similarity/noise). Adaptive individualized inhibition training has shown feasibility.[^adaptive-pilot]
- **Critical constraint:** adapting **multiple axes simultaneously** confounds interpretation — a change in performance cannot be attributed to any single manipulation, and the difficulty trajectory becomes non-reproducible in meaning. For research builds, **adapt one axis at a time**, or freeze all but one within a block/condition.

### 3.11 Does changing multiple variables at once harm interpretability?
**Yes.** This is a general experimental-design principle: simultaneous manipulation of ISI *and* stimulus noise *and* window means the independent variable is undefined, so per-block performance changes are uninterpretable and the "difficulty" is not a single quantity. This directly implicates TranquilMind's current dual-track (ISI + noise) staircase (see gap analysis).

### 3.12 Age-related differences
Response inhibition develops from early childhood into early adulthood; **adults make fewer commission errors than children/adolescents**, and efficiency improves with age (older children inhibit at an earlier movement stage than younger children).[^dev-midchild][^dev-6to8][^reward-children] Boys have shown higher commission rates in some samples.[^dev-6to8]
- **Implication:** parameters must be **age-scaled**. Preschool/school-age children need **longer response windows, longer stimulus durations, gentler ratios, shorter sessions, more feedback/scaffolding** than adults. A single parameter set is not valid across ages.

### 3.13 VR-specific fatigue / cybersickness / session limits
See §4. Immersive VR adds cybersickness and fatigue constraints not present in 2D tasks, capping session duration and requiring breaks and symptom checks.

### 3.14 Evidence limitations and transfer
The most rigorous synthesis is unflattering: a blinded/objective-outcome meta-analysis of computerized cognitive training in ADHD found **no significant effect on core ADHD symptoms** with probably-blinded raters, and cognitive gains that **did not transfer** to other executive domains or academics.[^cortese-2023] The earlier Cortese et al. (2015) meta-analysis showed effects shrink dramatically once raters are blinded / active controls are used.[^cortese-2015] A pre-registered online inhibition-training RCT found only limited near transfer and no mid-transfer.[^online-inhib-rct]
- **Bottom line:** improvements on the trained task are expected (practice), but **near transfer is inconsistent and far transfer is largely unsupported under rigorous designs.** TranquilMind must not infer real-world attention/ADHD benefit from in-task improvement.

---

## 4. VR fatigue / cybersickness constraints (session-duration authority)

- Longer VR exposure → higher probability and intensity of cybersickness; symptoms scale with duration.[^cyber-review][^cyber-immersion]
- Practical guidance and manufacturer ranges cluster around **breaks roughly every ~30 min**, with typical comfortable single-session exposures on the order of **tens of minutes**; validated work (VRNQ) examined maximum immersive durations without adverse symptomatology.[^vrnq][^cyber-oms]
- Evidence supporting exact manufacturer duration limits is **limited**, and individuals vary widely — so build in symptom checks rather than relying on a fixed "safe" number.[^vrnq]
- **Design implication:** a single research session should be **well under** the point where cybersickness/fatigue confounds behavior — on the order of **~12–20 min of active task** plus setup — with a Simulator Sickness / comfort check and the ability to stop early (§ Protocol Spec).

---

## 5. Age-scaling summary

| Group | Response window | Stimulus dur. | Ratio | Session | Notes |
|---|---|---|---|---|---|
| Preschool (~3–5) | Long (e.g., ~1500 ms+) | Longer (~500 ms) | Gentle (e.g., 70:30) | Very short | High commission rates are normal; heavy scaffolding/feedback; VR generally not advised at this age. |
| School-age (~6–12) | ~1000–1500 ms | ~300–500 ms | 75:25 | Short | Rapid development; large individual variance; VR comfort/fit a concern. |
| **Adolescents (~13–17) — ADOPTED TARGET (Protocol B/C)** | **start 1200 ms, bounds 800–1400 ms (adaptive)** | ~400 ms (frozen) | **80:20 (frozen)** | Moderate (≤20 min VR) | Still maturing; more commission errors than adults → **more generous window than adult ~1000 ms**. **Minor population: guardian consent + assent, VR-safety, data protection all mandatory.** |
| Adults (18+) | ~800–1200 ms | ~250–500 ms | 75:25 / 80:20 | Moderate | Baseline reference only — **not** the TranquilMind target; do not apply adult defaults to the adolescent protocol. |
| Older adults, MCI / early decline | Slower (generous, non-time-pressured) | Longer/high-legibility | Gentle | Short, frequent breaks | **Separate future accessibility track only** (Protocol Spec §7); usability/feasibility first; **not** merged with adolescent work; **no Alzheimer's treatment or efficacy claim.** |

These are literature-informed *starting ranges*, not validated protocol values. The adopted adolescent row values are **pilot starting values** to be confirmed by adolescent piloting. Any minor use requires ethics approval, guardian consent + adolescent assent, and age-specific piloting.

---

## 6. Evidence table

Abbreviations: N/R = not reported / not applicable in the source as summarized; GNG = go/no-go; ICT = inhibitory-control training; RT = reaction time; CE = commission error. Numeric parameters are as summarized from the cited source; where a range/uncertainty exists it is noted. **Full-text figures should be confirmed against the original PDF before being treated as exact.**

| # | Citation (short) | Population / age | N | Clinical? | Platform | Task type | GO:NOGO | Stim dur | Resp window | ISI/ITI | Trials/block | Blocks/session | Session dur | # Sessions | Adaptation | Outcomes | Limitations | Relevance to TranquilMind |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | Cortese et al. 2023, *Mol Psychiatry* — computerized cognitive training in ADHD, blinded/objective meta-analysis[^cortese-2023] | ADHD, children–adults (across RCTs) | Many RCTs | Clinical (ADHD) | Mixed (mostly 2D) | Cognitive training (incl. inhibition) | varies | varies | varies | varies | varies | varies | varies | multi-week | varies | **No sig. effect on ADHD core symptoms with blinded raters; cognitive gains don't transfer** | Heterogeneous; few blinded trials | **Central caveat** — sets the ceiling on efficacy claims |
| 2 | Cortese et al. 2015, *JAACAP* — cognitive training ADHD meta-analysis[^cortese-2015] | ADHD children/adolescents | Multiple RCTs | Clinical | 2D | Cognitive/WM/inhibition training | varies | varies | varies | varies | varies | varies | varies | multi-week | some adaptive | Effects shrink sharply with blinded raters / active controls | Rater blinding is decisive | Blinding + active control are mandatory design lessons |
| 3 | Online inhibition-training RCT (pre-registered), 2023[^online-inhib-rct] | Adults | 73 | Non-clinical | 2D online | Visual-search / ICT vs active control | N/R | N/R | N/R | N/R | N/R | N/R | short | **6** | Fixed | Near transfer to Flanker present; **no mid-transfer** to shoot/don't-shoot | Single population; specific tasks | Transfer is limited even when near-transfer appears |
| 4 | VR ADHD cognitive-control training (Floreo pilot), JMIR Pediatrics 2025[^vr-adhd-jmir] | Youth with ADHD, mean ~12.5 yr | 30 | Clinical (ADHD) | **VR (HMD)** | Gamified multi-domain incl. impulse control | N/R | N/R | N/R | N/R | N/R | N/R | per-session (short) | **up to 20 over 4–6 wk** | Adaptive/gamified | Improved clinician-rated inattention; near transfer to impulse-control/processing speed; well tolerated, minor AEs | Open-label pilot; small N; not blinded RCT | **Closest analog** — feasibility & dosing template; note weak design |
| 5 | "Optimal go/no-go ratios to maximize false alarms," *Behav Res Methods* 2018[^optimal-ratio] | Adults (methodological) | N/R | Non-clinical | 2D | GNG | Varied systematically | N/R | N/R | N/R | N/R | N/R | N/R | N/R | Ratio as IV | Higher GO proportion maximizes false-alarm (inhibition-failure) signal | Lab task | Direct basis for choosing 75:25 / 80:20 |
| 6 | Go/No-Go ratios modulate inhibition-related ERP, PMC11117662[^ratio-erp] | Adults | N/R | Non-clinical | 2D EEG | GNG | 75:25 / 50:50 / 25:75 compared | N/R | N/R | N/R | N/R | N/R | N/R | 1 | Ratio as IV | Ratio changes inhibition-related brain activity & CE rate | EEG lab | Confirms prepotency depends on ratio |
| 7 | Shorter ISI GNG study, PMC8048576[^isi-shorter] | Adults | N/R | Non-clinical | 2D | GNG | N/R | N/R | ~1000 ms deadline | Short vs long ISI | N/R | N/R | N/R | N/R | ISI as IV | Shorter ISI raises prepotency/time pressure; may better expose inhibition | Lab task | Supports jittered, moderate-to-short ISI |
| 8 | Stimulus-onset predictability & proactive control, PMC4410600[^predictability] | Adults | N/R | Non-clinical | 2D | GNG | N/R | N/R | N/R | Predictable vs jittered | N/R | N/R | N/R | N/R | Timing predictability as IV | Predictable onset engages proactive control, changing what's measured | Lab task | Justifies **jittering** timing |
| 9 | Combined GNG + CPT psychometrics across childhood, PMC10041761[^cpt-params][^psychometric-child] | Children (developmental) | Large | Non-clinical | 2D | GNG/CPT | N/R | ~200–500 ms | ~1000 ms | 1–2 s | 300–400 total (CPT) | N/R | N/R | N/R | Fixed | Age-related psychometrics of inhibition/attention | Cross-sectional | Source for canonical timing ranges & age effects |
| 10 | Reward decreases commission in children 4–12, PMC4146709[^reward-children] | Children 4–12 | N/R | Non-clinical | 2D | GNG | N/R | N/R | N/R | N/R | N/R | N/R | N/R | N/R | Reward context | Prior reward lowers later CE (context monitoring) | Lab task | Feedback/reward is not neutral; design it deliberately |
| 11 | Alcohol ICT RCT, PMC6277130[^alcohol-rct] | Problem drinkers, adults | RCT | Clinical-ish (behavior) | 2D | Stop/GNG-style | 75:25 (stop on 25%) | fixation 500 ms | ~1000 ms | N/R | N/R | N/R | short | multi | Fixed | ICT effects on drinking (mixed field) | Behavioral, not ADHD | Concrete 75:25 + timing template |
| 12 | Food GNG training, various[^food-gng] | Adults | N/R | Non-clinical | 2D | GNG | up to 90:10 stim–stop | N/R | N/R | 1000 ms ISI | 40/block | 3/category | short | multi | Fixed pairing | Improved stimulus-specific inhibition | Domain-specific | Shows high-GO extreme & block sizing |
| 13 | Developmental efficiency of inhibition mid-childhood, PubMed 19046150[^dev-midchild] | Children 5–11 | N/R | Non-clinical | 2D | GNG | N/R | N/R | N/R | N/R | N/R | N/R | N/R | N/R | — | Older children inhibit earlier in movement | Cross-sectional | Age-scaling of windows/difficulty |
| 14 | Substantial inhibition development 6–8 yr, unpredictable GNG[^dev-6to8] | Children 6–8 | N/R | Non-clinical | 2D | Unpredictable GNG | N/R | N/R | N/R | Unpredictable | N/R | N/R | N/R | N/R | — | Big gains 6→8 yr; boys higher CE | Cross-sectional | Young children need gentler params |
| 15 | Post-error control under constant speeded response, PMC4306303[^post-error-constant] | Adults | N/R | Non-clinical | 2D EEG | Speeded task | N/R | N/R | N/R | N/R | N/R | N/R | N/R | N/R | — | PES is a neurobehavioral adaptive process | Lab | **Measure** PES, don't enforce |
| 16 | Adaptive individualized ICT for binge drinking (pilot)[^adaptive-pilot] | Adults | pilot | Behavioral | 2D | Adaptive ICT | N/R | N/R | Adaptive | N/R | N/R | N/R | short | multi | **Adaptive (individualized)** | Feasible/accepted; preliminary efficacy | Pilot | Precedent for adaptive difficulty (single-axis) |
| 17 | VRNQ — max immersive VR duration without adverse symptoms[^vrnq] | Adults | N/R | Non-clinical | **VR** | — | — | — | — | — | — | — | studied max durations | — | — | Duration limits before cybersickness symptoms | Self-report instrument | Sets VR session-length ceiling |
| 18 | Cybersickness across immersion levels, PMC8132277[^cyber-immersion] | Adults | N/R | Non-clinical | VR | — | — | — | — | — | — | — | — | — | — | Higher immersion / longer exposure → more sickness | Lab | Justifies short VR sessions + checks |

> Where a cell is N/R, the parameter was not extractable from the summary consulted; it must be filled from the full text before being used as a protocol authority. Several rows are methodological/measurement studies included because they anchor a specific parameter decision, not because they are training trials.

---

## 7. Confidence levels for proposed parameters

| Parameter | Recommended default | Acceptable range | Confidence | Basis / disagreement |
|---|---|---|---|---|
| GO:NOGO ratio (core training) | **80:20** | 75:25 – 80:20 | **High** | Multiple converging sources on prepotency;[^ratio-erp][^optimal-ratio] exact best value within 75–80 is unsettled |
| Ratio (baseline/familiarization) | 100:0 GO then ~90:10 warm-up | — | Medium | Common practice to establish prepotency first |
| Stimulus presentation (research) | **discrete flashed, fixed location** | — | **High (decision)** | Adopted decision; replaces moving-target |
| Stimulus duration (visibility, frozen) | **400 ms** | 250–500 ms | Medium | Within canonical range[^cpt-params]; pilot default |
| Response window — **adopted target = adolescent** | **start 1200 ms (adaptive)** | bounds 800–1400 ms; adult ref ~1000 ms | Medium | Adolescents need more generous window than adults[^dev-6to8][^dev-midchild]; pilot default |
| Response-window step | **50 ms** | 25–100 ms | Medium | Pilot default |
| Staircase rule | **provisional 3-down/1-up** | — | Low–Medium | Rule/mapping unresolved; pre-register before scored use |
| ISI/ITI mean (frozen) | **1200 ms** | 1000–2000 ms | Medium | Wide literature range[^isi-general][^predictability] |
| Timing jitter (frozen) | **±30% jittered** | ±20–40% | **High** | Predictability changes the construct[^predictability] |
| Trials/block | enough for ≥8–10 NOGO | ~40–80 GNG trials | Medium | Block must yield stable NOGO estimate;[^food-gng] also the data-gate before the window may adapt |
| Feedback | subtle/immediate or block-level, non-punitive | — | Low–Medium | Feedback alters behavior;[^reward-children] optimal style unsettled |
| Post-error slowing | measure only | — | **High** | Robust phenomenon; enforcing confounds[^post-error-review] |
| Adaptive axis | **response window only** | 1 axis | **High** | Experimental-design principle; adopted decision |
| Single VR session active-task time | **12–20 min** | ≤ ~20 min + checks | Medium | Cybersickness/fatigue scaling;[^vrnq][^cyber-immersion] more conservative for minors |
| # Sessions (future program) | ~10–20 | 6–20 | Low–Medium | Ranges across weak-design studies[^vr-adhd-jmir][^online-inhib-rct] |
| Physiology role (v1) | logging + baseline + coarse trend; **no closed loop** | — | **High** | 1 Hz OK for demo/trend; **not** EDA-grade |
| Future EDA firmware rate | **32 Hz target** | ≥32 Hz | Medium | Necessary but **not sufficient** for research-grade EDA (see Protocol Spec §6.3) |
| Efficacy / transfer | **no claim** | — | **High (that we can't claim)** | Blinded meta-analyses null/limited[^cortese-2023][^cortese-2015] |

---

## 8. Disagreements & gaps in the literature

1. **Best ratio (75:25 vs 80:20):** both are defensible; direct head-to-head optimization for *training* (not measurement) is thin. Treat as a tunable, held constant within a study.
2. **ISI:** no consensus value; shorter ISI raises prepotency but also fatigue. Jitter is agreed; the mean is not.
3. **Feedback:** helps learning vs distracts/slows — unresolved; depends on population and goal.
4. **Adaptation target:** window vs ISI vs NOGO-frequency vs discrimination difficulty — no consensus on which best drives inhibition improvement; strong consensus that they shouldn't be combined uninterpretably.
5. **Does inhibition training even work?** The core disagreement. Task-specific improvement is real; transfer and clinical benefit are weakly supported under rigorous, blinded designs.[^cortese-2023][^online-inhib-rct]
6. **VR-specific:** almost no rigorous dosing literature for VR inhibition training specifically; VR ADHD studies are small, open-label pilots.[^vr-adhd-jmir]

---

## 9. Explicit claim boundaries (for any report, paper, or portfolio text)

**Allowed:** "exploratory," "evidence-informed task design," "measures behavioral inhibition proxies (commission/omission/RT) within the task," "feasibility/interaction research," "physiological signal explored as an interaction input."

**Not allowed:** any statement that TranquilMind treats/reduces ADHD; that it diagnoses or screens; that in-task improvement reflects real-world attention gains; that GSR indicates stress/calm/therapeutic success; that results generalize (far transfer). Do not present training-block metrics as standardized diagnostic scores. **For the future MCI / early-cognitive-decline accessibility variant (Protocol Spec §7): no Alzheimer's treatment claim, no cognitive-improvement claim, no efficacy claim — it is usability/feasibility research only and must never be merged with or presented as the adolescent ADHD protocol.**

---

## 10. Risks

- **Overclaiming** efficacy or transfer (reputational/ethical) — mitigated by §9.
- **Uninterpretable data** from multi-axis adaptation — mitigated by single-axis adaptation (§3.10–3.11).
- **Cybersickness/fatigue** confounding behavior and harming participants — mitigated by short sessions + checks (§4).
- **Age-inappropriate parameters** if adult defaults are applied to children — mitigated by §5 and ethics review.
- **Pressure/artifact contamination** of physiology (see Protocol Spec GSR section) — mitigated by not fusing input-pressure with arousal.

---

## Sources

- Cortese S. et al. (2023). Computerized cognitive training in ADHD: meta-analysis of RCTs with blinded and objective outcomes. *Molecular Psychiatry*. https://www.nature.com/articles/s41380-023-02000-7
- Cortese S. et al. (2015). Cognitive Training for ADHD: Meta-Analysis of Clinical and Neuropsychological Outcomes From RCTs. *JAACAP*. https://pmc.ncbi.nlm.nih.gov/articles/PMC4382075/
- Pre-registered RCT — limited evidence of transfer following online inhibition training (2023). https://www.ncbi.nlm.nih.gov/pmc/articles/PMC10637678/
- VR cognitive-control training for ADHD (Floreo), JMIR Pediatrics and Parenting (2025). https://pediatrics.jmir.org/2025/1/e66617
- Optimal go/no-go ratios to maximize false alarms. *Behavior Research Methods* (2018). https://link.springer.com/article/10.3758/s13428-017-0923-5
- Go/No-Go Ratios Modulate Inhibition-Related Brain Activity (ERP). PMC11117662. https://pmc.ncbi.nlm.nih.gov/articles/PMC11117662/
- Do shorter inter-stimulus intervals in the go/no-go task enable better assessment of response inhibition? PMC8048576 / PubMed 33011995. https://www.ncbi.nlm.nih.gov/pmc/articles/PMC8048576/
- Stimulus onset predictability modulates proactive action control in a Go/No-go task. PMC4410600. https://pmc.ncbi.nlm.nih.gov/articles/PMC4410600/
- Psychometric Properties of a Combined Go/No-go and CPT across Childhood. PMC10041761. https://pmc.ncbi.nlm.nih.gov/articles/PMC10041761/
- Previous reward decreases errors of commission on later No-Go trials in children 4–12. PMC4146709. https://pmc.ncbi.nlm.nih.gov/articles/PMC4146709/
- RCT of Inhibitory Control Training for reduction of alcohol consumption in problem drinkers. PMC6277130. https://pmc.ncbi.nlm.nih.gov/articles/PMC6277130/
- Pilot testing of an adaptive, individualized inhibitory control training for binge drinking. *Psychological Research* (Springer). https://link.springer.com/article/10.1007/s00426-022-01725-4
- Go or no-go? Developmental improvements in efficiency of response inhibition in mid-childhood. PubMed 19046150. https://pubmed.ncbi.nlm.nih.gov/19046150/
- Evidence of substantial development of inhibitory control 6–8 yr on an unpredictable Go/No-Go task. *J Exp Child Psychol*. https://www.sciencedirect.com/science/article/abs/pii/S0022096516303186
- Post-error action control under constant speeded response. PMC4306303. https://www.ncbi.nlm.nih.gov/pmc/articles/PMC4306303/
- Validation of the Virtual Reality Neuroscience Questionnaire (VRNQ) — max immersive VR session duration. PMC6901952. https://www.ncbi.nlm.nih.gov/pmc/articles/PMC6901952/
- Cyber sickness in low-, semi-, and fully-immersive VR. PMC8132277. https://pmc.ncbi.nlm.nih.gov/articles/PMC8132277/
- VR Motion Sickness fixes / break guidance — Oxford Medical Simulation. https://oxfordmedicalsimulation.com/vr-motion-sickness-5-proven-fixes-for-motion-free-clinical-simulation/
- PhenX Toolkit — Response Inhibition (Go/NoGo) protocol. https://www.phenxtoolkit.org/protocols/view/530701

[^cortese-2023]: Cortese et al. 2023, *Molecular Psychiatry* — blinded/objective meta-analysis of computerized cognitive training in ADHD.
[^cortese-2015]: Cortese et al. 2015, *JAACAP* — cognitive training ADHD meta-analysis; blinding sensitivity.
[^online-inhib-rct]: Pre-registered RCT, 2023 — limited transfer after online inhibition training.
[^vr-adhd-jmir]: VR cognitive-control training for ADHD (Floreo pilot), JMIR Pediatrics 2025.
[^optimal-ratio]: Optimal go/no-go ratios to maximize false alarms, *Behav Res Methods* 2018.
[^ratio-erp]: Go/No-Go ratios modulate inhibition-related brain activity (ERP), PMC11117662.
[^isi-shorter]: Shorter ISI in go/no-go and assessment of inhibition, PMC8048576.
[^predictability]: Stimulus-onset predictability & proactive control, PMC4410600.
[^cpt-params]: Combined GNG/CPT psychometrics across childhood + CPT canonical timing, PMC10041761.
[^psychometric-child]: Same as above — developmental psychometrics.
[^reward-children]: Prior reward decreases commission errors in children 4–12, PMC4146709.
[^alcohol-rct]: RCT of ICT for alcohol reduction, PMC6277130.
[^food-gng]: Food go/no-go training paradigms (high GO:NOGO, block sizing).
[^isi-general]: General ISI ranges across GNG literature (search synthesis).
[^dev-midchild]: Developmental efficiency of inhibition, mid-childhood, PubMed 19046150.
[^dev-6to8]: Substantial inhibition development 6–8 yr, unpredictable GNG.
[^post-error-review]: Post-error slowing as adaptive control (review/synthesis).
[^post-error-constant]: Post-error action control under constant speeded response, PMC4306303.
[^adaptive-pilot]: Adaptive individualized ICT for binge drinking pilot, Springer.
[^vrnq]: VRNQ validation — maximum immersive VR duration, PMC6901952.
[^cyber-immersion]: Cybersickness across immersion levels, PMC8132277.
[^cyber-review]: Cybersickness duration relationship (search synthesis).
[^cyber-oms]: VR motion sickness break guidance, Oxford Medical Simulation.
[^phenx-gng]: PhenX Toolkit — standardized Go/NoGo protocol.
