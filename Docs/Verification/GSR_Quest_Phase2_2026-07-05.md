# GSR UDP Physiology Receiver — Quest Phase 2 Verification

**Date:** 2026-07-05
**Commit:** `789d32c22fd528fd5f0a45b48033ab858c2f3590` (feat: add GSR UDP physiology receiver)
**Result:** Success

## Network configuration

| Device | Value |
|---|---|
| Wi-Fi SSID | `302` (2.4GHz) |
| ESP32 IP | `192.168.1.160` |
| Quest IP | `192.168.1.186` |
| UDP port | `4210` |

## Verified on-device (Quest, package `com.tranquilmind.vr`)

- `UTranquilMindPhysiologyReceiver` subsystem starts on world begin play
- UDP socket binds successfully on `0.0.0.0:4210`
- 57 GSR packets received continuously over 57 seconds (~1 Hz)
- Parsed `raw` / `smoothed` / `qual` / `ts` fields logged correctly, e.g.:
  ```
  LogTranquilMindPhysiology: [Physiology] GSR raw=2368 smooth=2361 qual=97 ts=804445
  ```
- No Android network or permission errors observed (zero E/-level lines for the app process)
- No crash

## Known gotcha: silent app if Quest is asleep/unworn

If the headset is idle and not being worn, Horizon OS auto-pauses the app almost immediately after launch (Activity fires `onResume` then `onPause`/`onStop` within the same millisecond) and the engine produces **zero** further log output of any kind — this looks like a hang or regression but isn't one.

**Check first:** `adb shell dumpsys power | grep mWakefulness`. If it reports `Asleep`, that's the cause.

**Fix:**
```bash
adb shell input keyevent KEYCODE_WAKEUP
adb shell am force-stop com.tranquilmind.vr
adb shell monkey -p com.tranquilmind.vr -c android.intent.category.LAUNCHER 1
```
