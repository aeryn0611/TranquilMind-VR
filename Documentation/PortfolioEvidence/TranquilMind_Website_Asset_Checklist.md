# TranquilMind — Website Asset Checklist (Public Research Case Study)

Audit date: 2026-07-28. Every asset lists: purpose · source evidence · existing/missing · public/private · format · redaction · priority (P1 = needed for first publishable page, P2 = strengthens, P3 = nice-to-have).

Standing rules: no serials/SSIDs/IPs/personal paths/ProjectID GUID; calm-language only (no "therapy/therapeutic/treatment/clinical/measures attention" — Spec §14); all research-protocol content labeled DRAFT/pilot; JSONL shown must be developer self-test data and labeled as such; the literature document must be called a *scoping review*.

| # | Asset | Purpose | Source evidence | Existing? | Pub/Priv | Format | Redaction needed | Priority |
|---|---|---|---|---|---|---|---|---|
| 1 | Hero image/video — bubble in HDRI environment on Quest | First impression; prove it's a real shipped-to-device VR app | **MISSING** — no screenshots/video exist in the project | Missing | Public | 16:9 still (PNG) + 20–40 s captured video (MP4, device casting/scrcpy) | None if captured clean | **P1** |
| 2 | Research vs Demo architecture diagram (two pipelines, one seam) | Core engineering story: separation with enforcement | Fact Sheet §4–6; class list; 5 guard layers | Missing (data ready) | Public | SVG diagram | Use class names only, no paths | **P1** |
| 3 | Initial system diagram (session manager / spawner / target / pawn / physiology) | Orient readers before deep dives | Fact Sheet §2, §4–5 | Missing (data ready) | Public | SVG | None | P2 |
| 4 | Research-contamination failure-path diagram (hard-gate resume → self-scoring bubble → ledger/JSONL divergence) + the guard fix | Flagship honest-failure narrative; shows test-driven containment | Timeline M8; `TargetSpawnerComponent.cpp:303-327`; `TMSpawnContainmentTest.cpp` header | Missing (data ready — bug chain fully documented) | Public | SVG sequence/flow diagram, before/after | No file paths beyond class names | **P1** |
| 5 | Player-relative session frame diagram (immutable basis; origin/orientation split; XR readiness ladder) | Distinctive VR-engineering depth (STAGE-origin arbitrariness insight) | Timeline M9–M11; Pass 0C capture log line | Missing (data ready) | Public | SVG (top-down head + spawn geometry) | None | P2 |
| 6 | Quest baseline card (Quest 2 · Android 14 · 72/72 FPS locked · GPU ≈7 ms of 13.9 ms at lowest clocks · 120 s session) | Credibility: measured, on-device numbers | Evidence Register #7–10 (⚠ raw capture not archived — archive first) | Missing (numbers ready) | Public | Stat card (HTML/SVG) | No serial; say "Quest 2" | **P1** |
| 7 | Original-vs-revised bubble geometry comparison (200 cm/scale 1.0/84° closest vs 350 cm/0.45/7.4°→8.6°) | Strongest before/after design-iteration evidence | Evidence Register #15–16; Timeline M15 | Missing (numbers ready) | Public | Side-by-side scale diagram (SVG), angular-diameter annotated | None | **P1** |
| 8 | Implementation timeline graphic (M1→M18) | Show sustained, verifiable progression | `TranquilMind_Development_Timeline.md` | Missing (data ready) | Public | Horizontal timeline SVG | Drop personal dates granularity if desired | P2 |
| 9 | Nine-phase lifecycle diagram (Spawn→…→Destroy, A-prime timings) | Show the designed presentation architecture | Spec §4.1 | Missing (spec ready) | Public | SVG phase bar with ms annotations | **Must be labeled "designed, not yet implemented"** | P2 |
| 10 | Go/NoGo concept comparison (motion-direction + structure cues; hue never load-bearing; no red) | Accessibility-aware design reasoning | Spec §6; Decision Table D12–D15 | Missing (spec ready) | Public | SVG concept pair | Label as design concept; note CVD test planned | P2 |
| 11 | Quiet Core concept illustration (single-mesh in-material NoGo core) | Signature visual-design idea | Spec §6.2; Plan §10 | Missing (spec ready) | Public | SVG/render mock | Label as concept | P3 |
| 12 | Ambient-layer diagram (far 3 / mid 2 / wisps 0–2, distance bands, budget gates) | Show perf-disciplined visual ambition | Spec §7; Decision Table D17–D18 | Missing (spec ready) | Public | SVG top-down bands | Label as planned | P3 |
| 13 | Performance budget diagram (13.9 ms frame → tiers 7.7/9.5/11.0 ms; measured ≈7 ms point plotted) | Connect measurement to discipline | Spec §9 + Pass 0C numbers | Missing (data ready) | Public | Stacked-bar SVG | None | P2 |
| 14 | Automation-test evidence panel (8/8 archived summary excerpt; 23-test suite list; fail-closed toolkit principles) | Prove verification culture | `automation_summary.json`; test list in Fact Sheet §11; toolkit README | Partially existing (raw JSON exists) | Public | Screenshot of summary + styled list | Strip absolute paths from any log excerpt | **P1** |
| 15 | Determinism fingerprint visual (seed → 50-char schedule string → hash) | Make reproducibility tangible | Evidence Register #1 | Existing data (log line) | Public | Monospace styled block/SVG | None | P2 |
| 16 | GSR chain photo + diagram (electrodes → ESP32 → Wi-Fi UDP → Quest panel) | Hardware-integration breadth | Milestone doc; **hardware photo MISSING** | Diagram data ready; photo missing | Public | Photo (JPEG) + SVG chain | No SSID/IPs; blur any labels | P2 |
| 17 | Reflection & limitations section (text) | Research maturity: 1 Hz GSR not EDA; alternation demo-only; flash unresolved; 120 s proves nothing about attention; scoping-review coverage gaps (Domains E/G/H zero verified sources) | Evidence Register #3, #17; Literature Review §0/§11; Protocol spec §6.5 | Missing (all source text ready) | Public | Prose | Follow claim-language rules | **P1** |
| 18 | Research protocol summary card (Protocol B: 80:20, 400 ms, adaptive 800–1400 ms window, blocks) | Show research design competence | Protocol Specification v1 | Missing (spec ready) | Public | Table/card | Label DRAFT, pilot defaults, not yet run with participants | P3 |
| 19 | JSONL schema sample (redacted developer self-test trial row) | Data-engineering credibility | `Saved/TranquilMind/Research/*.jsonl` | Existing (needs excerpting) | Public | Syntax-highlighted block | Strip sessionId GUIDs; label self-test | P3 |
| 20 | Video: full 120 s demo session with GSR panel and summary panel | End-to-end proof | **MISSING** — must be captured on device | Missing | Public | MP4 ≤60 s edit | Ensure no guardian/passthrough leakage in capture | **P1** |

## Production order recommendation

1. **Capture pass on device (unblocks #1, #6-archive, #14, #16-photo, #20):** screenshots + video + logcat perf archive in one Quest session.
2. **P1 diagrams from existing data:** #2 architecture, #4 failure path, #7 geometry before/after.
3. **P1 text:** #17 limitations, then stat card #6.
4. P2/P3 as the page grows.

Private-only (never publish): verification run directories with raw logs (paths), GSR Phase 2 doc as-is (SSID/IPs), any file bearing the device serial, `Saved/` session GUID filenames, firmware sketch if it embeds Wi-Fi credentials.
