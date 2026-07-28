# Quest Bubble Rendering — Research & Design Specification

**Project:** TranquilMind (Quest VR GO/NOGO research prototype)
**Status:** DRAFT — research only. No materials, meshes, or assets to be created or modified until approved.
**Author:** Front-end / rendering research pass, 2026-07-08
**Target hardware (verified from repo + baseline report):** Meta Quest 2, Qualcomm SM8250 (Adreno 650), HorizonOS Android 14, Vulkan 1.1, arm64. UE **5.7**, OpenXR, `bPackageForMetaQuest=True`, `vr.MobileMultiView=1`.

> **Claim boundary.** This document concerns rendering realism and performance only. It makes no claim about therapeutic, diagnostic, or clinical value of the visuals.

---

## 0. Executive summary

The realistic-bubble goal is achievable on Quest 2, but **not** through the desktop-style route (real refraction + scene-color distortion + multiple translucent shells). Those are the exact features that are unsupported, expensive, or sorting-unstable on the mobile forward renderer. A believable bubble on Quest comes almost entirely from a **strong Fresnel rim + thin-film emissive tint on a single translucent (or masked-rim/translucent-core hybrid) shell**, not from physically simulated refraction.

Two distinct materials are recommended, because the interactive and decorative bubbles have opposite priorities:

- **Interactive GO/NOGO targets → legibility first.** Higher opacity, saturated GO/NOGO hue carried in the emissive core, controlled Fresnel. Realism is secondary to reliable classification. Recommend **Strategy 2 (hybrid)** or a deliberately higher-opacity variant of Strategy 1.
- **Decorative background bubbles → realism first.** Very low opacity, pronounced iridescent Fresnel rim, cheap. Recommend **Strategy 1 (thin translucent shell)** with tight overdraw budgeting, or **Strategy 3 (baked)** if a Blender artist round-trip is available.

**Key uncertainty:** the repo currently has **no Blender source and no dedicated bubble mesh** — targets reuse `M_TheVoid` (a Substrate Unlit material) on what appears to be an engine sphere primitive. Substrate is disabled on Android in this project (`r.Substrate=False`), so the current material's authored graph and its on-Quest behavior may already diverge. This must be resolved before any implementation (see §7).

---

## 1. Current Unreal rendering setup (inspected locally)

### 1.1 Engine / renderer

| Item | Value | Source |
|---|---|---|
| UE version | 5.7 | `TranquilMind.uproject` (`EngineAssociation": "5.7"`) |
| XR | OpenXR plugin enabled | `TranquilMind.uproject` Plugins |
| Quest packaging | `bPackageForMetaQuest=True` | `Config/Android/AndroidEngine.ini` |
| Stereo | `vr.MobileMultiView=1` | `Config/Android/AndroidEngine.ini` |
| Mobile hardware target | `TargetedHardwareClass=Mobile`, `DefaultGraphicsPerformance=Scalable` | `Config/Android/AndroidEngine.ini` |
| Confirmed device | Quest 2 / SM8250 / Adreno 650 / Vulkan 1.1 | `Docs/Quest_Working_Baseline_Report.md` |

**Rendering path:** On Quest, UE uses the **mobile renderer**, which is forward-based. Translucency in Unreal always resolves through forward shading regardless of platform, and the forward renderer is Meta's recommended path for Quest.[^meta-forward][^ue57-mobile] There is **no explicit `r.ForwardShading=1`** in the desktop `DefaultEngine.ini` — the desktop editor path is deferred + Substrate; the mobile/Quest path is forward via the mobile renderer. This is expected but worth stating because desktop-preview appearance will not match on-device appearance.

### 1.2 Desktop-vs-Android config divergence (important)

`Config/DefaultEngine.ini` enables desktop-only features:
`r.Substrate=True`, `r.RayTracing=True`, `r.DynamicGlobalIlluminationMethod=1` (Lumen), `r.ReflectionMethod=1` (Lumen reflections), `r.Shadow.Virtual.Enable=1`, `TargetedHardwareClass=Desktop`.

`Config/Android/AndroidEngine.ini` **overrides all of them off** for device builds:
`r.Substrate=False`, `r.RayTracing=False`, GI `=0`, ReflectionMethod `=0`, VSM `=0`.

Consequence for bubbles: **any appearance authored against Substrate, Lumen reflections, or ray tracing in the editor will not be present on Quest.** Reflections in particular are off — a bubble cannot rely on real-time environment reflection on-device.

### 1.3 Existing materials / assets

| Asset | What it is (from binary string inspection) | Note |
|---|---|---|
| `M_TheVoid.uasset` | Parent material. Contains `MaterialExpressionSubstrateUnlitBSDF`, `FrontMaterial`, `Opacity`, `OpacityMask`, `Refraction` inputs. Shading model **MSM_Unlit**. | Substrate-authored; Substrate is **off** on Android. |
| `MI_Target_Go.uasset` | Material **instance** parented to `/Game/M_TheVoid`. `MSM_Unlit`. Has `BasePropertyOverrides`, `OpacityMaskClipValue`, scalar/vector param overrides, and a `HasSceneColor` flag. | GO target = param override of the void material. |
| `MI_Target_NoGo.uasset` | Same parent, NOGO variant. | NOGO target = param override. |
| `BP_Target.uasset` | Blueprint with `StaticMeshComponent` + `OverrideMaterials` → `MI_Target_Go`. | Mesh is a static mesh (engine sphere primitive; no custom mesh asset in `Content/`). |
| `M_TheVoid` sphere | Renders the environment "void" (confirmed rendering on Quest). | — |

**No custom bubble mesh, no textures** (only `Saved/AutoScreenshot.png` exists). GO/NOGO targets and the background currently share one material lineage, differentiated by instance parameters.

### 1.4 Blender source availability

**None found locally.** Searched the whole tree for `*.blend`, `*.fbx`, `*.obj`, `*.gltf/glb`, `*.tga`, `*.exr`, and non-cooked `*.png`. Result: no Blender project, no mesh interchange files, no source textures. **Strategy 3 (baked Blender mesh) is therefore currently blocked** on locating or re-authoring the original concept file. This is an open question for the user (§9).

---

## 2. Mobile / Quest translucency constraints (from primary docs)

These are the hard constraints that shape every strategy below.

1. **Overdraw is the dominant cost.** Translucent (alpha-blended) surfaces cannot early-z reject; every overlapping layer re-shades the pixel. Meta and ARM both call transparency/overdraw the primary mobile bottleneck and advise minimizing it.[^meta-transmask][^arm-trans][^ue-transparency] On a fill-rate-bound tiled GPU like Adreno 650 this is the single biggest risk for a "field of bubbles."
2. **Masked (alpha-test) is much cheaper than translucent** but produces hard binary edges (no soft rim, no partial transparency) and can alias. Meta's guidance is to prefer masked over translucent wherever the look allows.[^meta-transmask]
3. **Translucency always uses forward shading**, and glass-like looks should use Lighting Mode **Surface ForwardShading**.[^ue-transparency][^meta-forward] Unlit translucency is cheapest of all (no lighting), which suits an emissive-rim bubble.
4. **Refraction / SceneColor distortion is a mobile liability.** Scene-color access forces a scene-color resolve, refractive translucency has documented sort-order problems (refractive draws on top of non-refractive), and Meta's own workaround relies on Custom Depth passes rather than true refraction.[^ue-refract-sort][^ue-additive-sort][^meta-transmask] Treat real refraction as **not Quest-safe** for this project.
5. **Depth sorting is per-object, not per-pixel.** Translucent objects sort by a single sort key; interpenetrating or co-located translucent spheres can pop/flicker in sort order. More simultaneous translucent bubbles = more visible sorting instability.
6. **Instruction / sampler budget.** Mobile materials have tighter instruction and texture-sampler limits than desktop; the mobile HLSL is a reduced feature set and some nodes silently no-op on mobile.[^ue57-mobile][^ue-mobile-materials] Keep the bubble graph lean (Fresnel + a couple of lerps + one optional thin-film sample).
7. **No reliable real-time reflections on this config** (ReflectionMethod off on Android). Any "reflective glass" cue must be faked with the Fresnel term and/or a small baked cubemap/gradient, not real reflection capture.
8. **MSAA, not TAA, on Quest forward.** Forward + MSAA is the norm; thin translucent rims and alpha edges interact with MSAA differently than with desktop TAA. Rim width must be wide enough to survive MSAA at Quest resolution and not shimmer.

> These are established mobile-forward constraints confirmed against UE 5.7 and Meta Horizon docs; exact instruction counts are project- and driver-specific and should be read from the Material Stats panel in the mobile preview feature level, not assumed.

---

## 3. Three implementation strategies compared

### Strategy 1 — True translucent shell on a low-poly UV sphere
Single translucent, **Unlit**, Surface-ForwardShading sphere. Low opacity center (see-through), strong Fresnel drives a bright emissive rim, thin-film tint (cyan/violet/pink) modulated by Fresnel and/or view angle. No refraction, no scene color, no reflection capture. Optional faint fixed highlight via a baked gradient or a second Fresnel lobe.

### Strategy 2 — Hybrid: masked/dithered shell + limited translucent inner layer
The bubble body is **masked or dithered-opacity** (cheap, early-z friendly) to carry the readable core color; a **thin translucent rim band** (narrow Fresnel-gated ring) supplies the soft glassy edge. Reduces the translucent overdraw area to just the rim annulus rather than the whole disc. Dithered opacity (a.k.a. stochastic/temporal alpha) gives pseudo-transparency without alpha-blend overdraw but adds noise that MSAA only partly cleans.

### Strategy 3 — Blender-authored mesh with baked normals + rim/thin-film texture, simple Quest material
Author the bubble in Blender: baked normal map for micro-surface, baked rim coloration / thin-film gradient into a texture, optional low-poly inner shell. Import mesh + textures; in Unreal use a **simple** translucent-or-masked material that samples the baked textures plus a live Fresnel. Moves visual richness offline (texture) instead of runtime (instructions/overdraw).

### Comparison matrix

| Criterion | Strategy 1 (translucent shell) | Strategy 2 (hybrid masked+rim) | Strategy 3 (baked Blender) |
|---|---|---|---|
| Visual quality (realism) | High — soft, genuinely see-through | Medium-high — soft rim, firmer body | High — richest surface detail |
| Transparency realism | Highest | Medium (body less transparent) | High (baked, but static) |
| Quest compatibility | Good if opacity + count controlled | **Best** (lowest overdraw) | Good; depends on texture budget |
| Overdraw cost | **High** (full-disc translucency) | **Low** (only rim annulus translucent) | Low–medium (masked body) |
| Sorting risk | High with many overlapping | Low (masked body doesn't sort) | Low–medium |
| Material complexity | Low–medium | Medium (two-zone logic) | Low runtime / higher asset effort |
| Implementation time | **Fast** (material only) | Medium | **Slow** (needs Blender round-trip; blocked — no source) |
| Best for GO/NOGO targets | OK if opacity raised for legibility | **Recommended** (legible + soft) | Overkill; static look fights state changes |
| Best for decorative bubbles | **Recommended** (max realism, few on screen) | OK | Recommended if artist time exists |

---

## 4. Proposed materials (recommendation)

### 4.1 Primary material — interactive GO/NOGO targets (`M_Bubble_Target`, proposed)
**Priority: legibility and reliable GO≠NOGO discrimination. Realism is subordinate.**

- Strategy **2 (hybrid)**, or Strategy 1 tuned to the higher end of the opacity range.
- **Unlit**, Blend = Translucent for the rim, **Surface ForwardShading** lighting mode; masked/dithered core.
- GO vs NOGO carried primarily by the **emissive core color** (high chroma, high luminance contrast) — not by transparency. Keep the two states separated in both hue **and** luminance so they remain distinguishable to red-green-deficient viewers and under MSAA. (Current design uses "deep indigo" GO vs "dark crimson" NOGO per `TranquilMindTypes.h` display names — verify these clear an accessibility contrast check before locking.)
- Center opacity higher than decorative (target legibility), Fresnel rim present but not so bright it washes out the state color.
- **No refraction, no scene color, no second/third shell.**
- Optional: a subtle constant inner highlight (baked gradient) for spherical read, but keep it from competing with the state color.

### 4.2 Decorative material — background bubbles (`M_Bubble_Decor`, proposed)
**Priority: realism. Never classified, so it can be as transparent as desired.**

- Strategy **1**. Unlit, Translucent, Surface ForwardShading.
- Very low center opacity; **strong** Fresnel rim; thin-film cyan/violet/pink driven by Fresnel and a slow time/parallax factor for shimmer.
- Strictly overdraw-budgeted (see §5.5): few on screen, small screen area, or spawned away from the gaze center where fill cost per pixel matters most.
- No refraction, no reflection capture.

### 4.3 Shared authoring notes
- Both should be authored and previewed in the **mobile/Quest preview feature level**, reading the **Material Stats** for instruction/sampler counts, not the desktop preview.
- Decouple all tunables (opacity, Fresnel exponent, rim width, thin-film colors, GO/NOGO colors) as **named material parameters** so they become configuration data, consistent with the project's stated data/UI decoupling rule.

---

## 5. Concrete recommendations (defaults + ranges)

> Confidence: these are engineering starting points for Quest 2, to be confirmed by on-device profiling (§8). They are not hard limits from a cited benchmark.

### 5.1 Sphere polygon count
- **Interactive targets:** ~**320–560 tris** (UV sphere ≈ 16–24 segments × 12–16 rings). Enough for a smooth silhouette at arm's length; Fresnel needs decent normals but not high density.
- **Decorative bubbles:** ~**180–320 tris**; smaller on screen, can be lower.
- Avoid the default engine sphere if it is high-poly; a purpose-built low-poly UV sphere is cheaper and gives predictable normals for Fresnel.

### 5.2 Texture sizes
- Strategy 1/2 (procedural rim): **no textures required**, or a single **256×256** thin-film gradient (R8G8B8, no alpha) if a baked shimmer is wanted.
- Strategy 3 (baked): normal map **512×512**, rim/thin-film **512×512**; **1024×1024 only** if a single hero decorative bubble justifies it. Prefer ASTC compression. Keep total sampler count ≤ 2–3 in the mobile material.

### 5.3 Maximum simultaneous transparent bubbles
- **Interactive targets on screen at once: keep to 1–3.** (The current code already enforces a single active target in demo/debug — see gap analysis. Scientifically, one target at a time is also cleaner for GO/NOGO scoring.)
- **Decorative translucent bubbles: budget ~6–12 simultaneously**, and only if their combined screen coverage stays within the overdraw budget (§5.5). Fewer, larger, well-separated bubbles beat many overlapping ones (sorting + overdraw).
- Treat this as a **fill-rate budget, not a raw count** — 12 tiny bubbles ≠ 12 fullscreen bubbles.

### 5.4 Opacity range
- **Interactive targets:** center alpha **0.55–0.80** (legibility). Rim can approach 0.9–1.0.
- **Decorative bubbles:** center alpha **0.06–0.25**; rim **0.5–0.9**.
- Never 0.0 center (invisible target) for interactive; never ≥0.9 flat (opaque glowing sphere — explicitly unwanted).

### 5.5 Fresnel exponent & rim width
- **Fresnel exponent (power):** **2.5–5.0**. Lower (~2.5) = wide, soft rim; higher (~5) = thin, sharp rim. Decorative can go slightly higher for a delicate edge; interactive should stay lower/wider so the rim survives MSAA and doesn't shimmer.
- **Rim width:** aim for a visible band of roughly **8–20% of the sphere's screen radius**. Below ~6% it aliases/shimmers under Quest MSAA; above ~25% it stops reading as a thin film.
- Expose both as parameters; tune on-device.

### 5.6 Features to AVOID on this Quest config
| Feature | Verdict | Why |
|---|---|---|
| Real refraction / SceneColor distortion | **Avoid** | Not Quest-safe; sort-order bugs; forces scene-color resolve.[^ue-refract-sort][^meta-transmask] |
| Two-sided rendering | **Avoid by default** | Doubles translucent overdraw (front+back faces both shade); only revisit if a hollow-shell look is essential and profiled. |
| Multiple stacked shells (inner + outer translucent) | **Avoid** | Multiplies overdraw and sorting instability. At most one masked inner + one translucent rim (Strategy 2). |
| DepthFade for soft intersection | **Use sparingly / avoid** | Needs scene depth access; on mobile forward it adds cost and can be unreliable. Only if profiled and clearly needed for a specific intersection artifact. |
| Real-time reflection capture / SSR | **Not available** | Reflections off on Android config; fake with Fresnel. |
| Lit translucency with dynamic lights | **Avoid** | Use Unlit + emissive; cheaper and looks better for glowing bubbles. |

---

## 6. Interactive vs decorative — the non-negotiable distinction

Realism must **never** be traded against GO/NOGO classification clarity. Operationally:

- Interactive targets keep a **fixed, high-contrast state color** and a **minimum opacity floor** even at the "most transparent" difficulty setting. Any adaptive stimulus-difficulty (e.g., the existing `NoiseAlpha` staircase) must **not** be allowed to push an interactive bubble below the legibility floor or blur the GO/NOGO hue/luminance separation.
- Decorative bubbles are visually distinct enough (much lower opacity, no state color, positioned as background) that they cannot be confused with a target. Consider a deliberate silhouette or size difference so a decorative bubble is never mistaken for a GO/NOGO stimulus.
- If ambiguity is possible, bias toward making targets **more** obviously targets (halo, size, steadier motion), even at some realism cost.

---

## 7. Substrate-on-Quest issue (must resolve before implementation)

`M_TheVoid` (the parent material for both the void and the GO/NOGO instances) is authored with **Substrate Unlit BSDF** nodes, but `r.Substrate=False` on Android. Before any bubble work:
1. Confirm on-device whether the current targets render as intended or via a fallback (they do render per the baseline report, but the *material path* may differ from the editor).
2. Decide whether new bubble materials are authored in **legacy (non-Substrate) Unlit** to match the Android runtime, avoiding a Substrate/non-Substrate split between editor preview and device. **Recommendation: author the new bubble materials as legacy Unlit** so preview and device agree, unless the project intends to enable Substrate mobile support (heavier, and not currently on).

---

## 8. Quest verification method (performance + sorting)

A bubble material/asset is "Quest-verified" only after all of the following on-device (not in editor):

**A. Performance / fill-rate**
1. Build and deploy to Quest 2; run in the actual `L_TranquilMind_Void` scene.
2. Enable **`stat unit`** and **`stat rhi`** (or OVR Metrics Tool / Meta's performance HUD) and confirm frame time holds the target (72/90 Hz → ≤ ~13.8/11.1 ms).
3. Use **`viewmode shadercomplexity`** / mobile overdraw visualization to inspect the bubble field; the bubbles should not paint the screen red.
4. Stress test: spawn the **maximum intended simultaneous bubble count** (interactive + decorative) at the **largest intended screen size** (bubbles near the camera) and confirm frame time still holds. Record the count/size at which it fails — that defines the real budget.
5. Check with MobileMultiView on (both eyes), since translucency cost is per-eye.

**B. Sorting / visual correctness**
6. Move the head so decorative bubbles **overlap each other and overlap a target**; watch for sort popping/flicker. Log any order the sort gets wrong.
7. Verify GO and NOGO remain **unambiguously distinguishable** at the far end of spawn distance, under motion, and under MSAA — ideally with a quick check by someone naive to the color mapping, and a color-contrast check for CVD safety.
8. Confirm the rim does not shimmer/crawl during head motion (MSAA + thin rim interaction).

**C. Instruction budget**
9. In the mobile preview feature level, read **Material Stats**; record instruction count and sampler count for both materials and keep headroom below the mobile ceiling.

**D. Record**
10. Log results in `Docs/Verification/` (matching the existing `GSR_Quest_Phase2_*.md` pattern): device, build hash, frame time, max bubble count sustained, sorting notes, screenshots.

---

## 9. Open questions for the user / supervisor

1. **Blender source:** Does the original bubble concept `.blend` exist anywhere (external drive, cloud, another machine)? Strategy 3 is blocked without it. If not, do you want it re-authored, or shall we commit to procedural Strategy 1/2?
2. **Substrate:** Author new bubble materials as legacy Unlit (recommended, matches Android runtime) or invest in Substrate-mobile? 
3. **GO/NOGO color mapping:** Keep deep-indigo/dark-crimson, or move to a pairing with stronger luminance separation and CVD safety? (Affects the "legibility floor.")
4. **Decorative bubble budget:** Is a background bubble field actually wanted in the research/pilot builds, or only in the portfolio/demo build? (Affects how aggressively we must budget overdraw.)
5. **Refresh target:** 72 Hz or 90 Hz on Quest 2? Sets the frame-time budget the verification must hold.

---

## Sources

Rendering (primary / vendor docs):

- Meta Horizon OS Developers — Forward Shading Renderer (Unreal): https://developers.meta.com/horizon/documentation/unreal/unreal-forward-renderer/
- Meta Horizon OS Developers — Translucent vs Masked Rendering in Real-Time Applications: https://developers.meta.com/horizon/blog/translucent-vs-masked-rendering-in-real-time-applications/
- Unreal Engine 5.7 Documentation — Mobile Rendering and Shading Modes: https://dev.epicgames.com/documentation/unreal-engine/mobile-rendering-and-shading-modes-for-unreal-engine
- Unreal Engine Documentation — Using Transparency in Unreal Engine Materials: https://dev.epicgames.com/documentation/en-us/unreal-engine/using-transparency-in-unreal-engine-materials
- Unreal Engine 4.26/4.27 Documentation — Materials for Mobile Platforms: https://docs.unrealengine.com/4.26/en-US/SharingAndReleasing/Mobile/Materials
- Arm Developer — Unreal Engine Material and Shader Best Practices: Transparency considerations: https://developer.arm.com/documentation/102676/latest/Transparency-considerations
- Epic Developer Community Forums — Translucent render order of refractive materials: https://forums.unrealengine.com/t/translucent-render-order-of-refractive-materials/342163
- Epic Developer Community Forums — Rendering order between additive and translucent material with Scene Color access: https://forums.unrealengine.com/t/rendering-order-between-additive-and-translucent-material-with-scene-color-access/215329
- Epic Developer Community Forums — translucent material causing overdraw and poor performance in UE5: https://forums.unrealengine.com/t/why-is-my-translucent-material-causing-overdraw-and-poor-performance-in-complex-scenes-in-unreal-engine-5/2500029

Local project inspection (this repo): `TranquilMind.uproject`, `Config/DefaultEngine.ini`, `Config/Android/AndroidEngine.ini`, `Content/M_TheVoid.uasset`, `Content/MI_Target_Go.uasset`, `Content/MI_Target_NoGo.uasset`, `Content/BP_Target.uasset`, `Docs/Quest_Working_Baseline_Report.md`.

[^meta-forward]: Meta Horizon OS — Forward Shading Renderer (Unreal).
[^meta-transmask]: Meta Horizon OS — Translucent vs Masked Rendering in Real-Time Applications.
[^ue57-mobile]: Unreal Engine 5.7 — Mobile Rendering and Shading Modes.
[^ue-transparency]: Unreal Engine — Using Transparency in Unreal Engine Materials.
[^ue-mobile-materials]: Unreal Engine — Materials for Mobile Platforms.
[^arm-trans]: Arm Developer — Material and Shader Best Practices: Transparency considerations.
[^ue-refract-sort]: Epic Developer Community Forums — Translucent render order of refractive materials.
[^ue-additive-sort]: Epic Developer Community Forums — Rendering order between additive and translucent material with Scene Color access.
