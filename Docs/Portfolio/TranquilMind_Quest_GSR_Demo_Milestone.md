# TranquilMind — Quest GSR Demo Milestone

**Date:** 2026-07-06
**Milestone commit:** `02d6e41` (feat: add VR demo summary panel)
**Status:** Verified end-to-end on Meta Quest hardware

## Project summary

TranquilMind is a Meta Quest VR prototype that combines a GO/NOGO attention-training task with live physiological sensing. A Grove GSR (skin conductance) sensor is read by an ESP32 microcontroller, smoothed on-device, and streamed over Wi-Fi UDP directly to the Quest headset, where a custom Unreal Engine C++ subsystem receives, parses, and exposes the signal to VR-visible UI in real time. The full chain — sensor electrode to in-headset display — was verified on device with continuous ~1 Hz data and no crashes, culminating in a 120-second demo session that ends with an automatic summary panel reporting live signal status.

## System architecture

| Layer | Component | Role |
|---|---|---|
| Sensor | Grove GSR on ESP32 (GPIO34, 12-bit ADC) | Skin-conductance acquisition |
| Firmware | Arduino sketch on ESP32 NodeMCU (2.4 GHz Wi-Fi) | EMA smoothing, signal-quality estimate, 1 Hz JSON packets |
| Transport | Wi-Fi UDP, port 4210, single-subnet LAN | `{"src":"esp32_gsr","raw":…,"smoothed":…,"ts":…,"qual":…}` |
| Receiver | `UTranquilMindPhysiologyReceiver` (UWorldSubsystem, C++) | Non-blocking socket thread → game-thread sample cache |
| UI | `ATranquilMindGSRStatusPanel`, `ATranquilMindDemoSummaryPanel`, `ATranquilMindHintPanel` (TextRender, C++) | VR-visible live status, demo-end summary, task hints |
| App | Unreal Engine 5.7, OpenXR, Meta Quest (`com.tranquilmind.vr`) | GO/NOGO task loop and session state machine |

## Verified data path

```
GSR sensor -> ESP32 ADC -> EMA smoothing -> UDP JSON -> Quest Wi-Fi -> Unreal receiver -> VR UI
```

Every hop was verified independently (serial monitor, Mac UDP listener, Mac standalone build, Quest logcat) before the full on-device chain was confirmed.

## Key features

- **GO/NOGO VR task loop** — target spawning, trigger response, session phase state machine
- **Auto-hiding hint panel** — camera-anchored intro instructions that dismiss after 5 seconds
- **GSR UDP physiology receiver** — background-thread socket, JSON parsing, thread-safe game-thread handoff, CVar kill-switch (`tranquilmind.physiology.enabled`)
- **VR GSR LIVE status panel** — peripheral readout of raw / smoothed / quality with LIVE / STALE / WAITING freshness states
- **120 s demo summary panel** — hidden during the task, reveals automatically at demo end with live signal status and claim-boundary text

## Verification evidence (Quest logcat)

- `[Physiology] UDP socket bound OK — listening on 0.0.0.0:4210`
- Continuous GSR packets at ~1 Hz (57/57 s in the receiver test; 113+ in the demo run)
- `[GSRStatus] First LIVE sample observed — raw=2368 smooth=2373 qual=97` (1 ms after first packet)
- `[DemoMode] Session ended | Elapsed=120.0 s`
- `[DemoSummary] Demo end detected — summary panel shown` with body `Physiological stream: GSR LIVE / Signal quality: 81`
- No crash, no fatal errors, no Android network/permission errors across all runs

## Claim boundary

- This is an **exploratory prototype**, not a clinical assessment tool.
- The physiological stream is used as **prototype input** for demonstration; it is not a validated diagnostic or therapeutic measurement.
- No claims are made about attention, stress, or health outcomes.

## Future work

- Baseline calibration (per-user resting-level normalization before sessions)
- Adaptive regulation rules (task parameters responding to physiological trends)
- HR / SpO₂ sensor extension (MAX30102 module, already on hand)
- Controlled user study with proper protocol and consent

## Latest relevant commits

| Commit | Description |
|---|---|
| `a478e6b` | feat: add auto-hiding VR hint panel |
| `789d32c` | feat: add GSR UDP physiology receiver |
| `a548f7e` | docs: record Quest GSR UDP verification |
| `b08a862` | feat: add minimal VR GSR status panel |
| `02d6e41` | feat: add VR demo summary panel |
