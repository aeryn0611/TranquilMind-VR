#!/usr/bin/env bash
# phase1_capture_demo.sh
# Install + launch the exact Demo APK, capture filtered logcat for >=130s (or until
# the user confirms the Demo Summary appeared), then clean up.
#
# Visual checks (moving targets, GSR LIVE, summary panel) CANNOT be auto-verified and
# are recorded as user-observed.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
if [[ "${1:-}" != "--resume" ]]; then
    printf 'Usage: bash %s --resume\n' "$0" >&2
    exit 2
fi
# shellcheck source=phase1_verify_common.sh
source "$SCRIPT_DIR/phase1_verify_common.sh"
init_run_context resume

DEMO_DIR="$VERIFICATION_DIR/demo"
DEMO_LOGS="$VERIFICATION_DIR/demo_logs"
RAW_LOGCAT_FILE="$DEMO_LOGS/demo_logcat_raw.txt"
FILTERED_LOGCAT_FILE="$DEMO_LOGS/demo_logcat_filtered.txt"
APK_PATH_FILE="$DEMO_DIR/apk_path.txt"

DEMO_FILTER='OperatingMode|DemoMode|GSRStatus|Physiology|DemoSummary|Session|Fatal|Error|crash'
CAPTURE_SECONDS="${DEMO_CAPTURE_SECONDS:-130}"
DEVICE_READY=0

cleanup() {
    local rc=$?
    trap - EXIT INT TERM
    if [[ "$DEVICE_READY" == "1" ]]; then
        stop_logcat_processes_started_by_script || rc=1
        adb_do shell am force-stop "$PACKAGE_NAME" 2>/dev/null || true
        restore_quest_power_state || rc=1
    fi
    log_info "phase1_capture_demo.sh exiting (rc=$rc). Outputs: $DEMO_LOGS"
    exit "$rc"
}
trap 'exit 130' INT
trap 'exit 143' TERM
trap cleanup EXIT

require_adb

if [[ ! -f "$APK_PATH_FILE" ]]; then
    log_error "Missing $APK_PATH_FILE. Run phase1_package_demo.sh first."
    exit 1
fi
APK_PATH="$(cat "$APK_PATH_FILE")"
require_file "$APK_PATH"

adb_single_device_check
DEVICE_READY=1
save_quest_power_state
adb_do shell input keyevent KEYCODE_WAKEUP || true
enable_usb_stay_awake

log_info "Installing Demo APK: $APK_PATH"
run_logged "$DEMO_DIR/install" adb_do install -r "$APK_PATH"

start_logcat_capture "$RAW_LOGCAT_FILE" "$FILTERED_LOGCAT_FILE" "$DEMO_FILTER"

log_info "Launching Demo app."
adb_do shell am force-stop "$PACKAGE_NAME" || true
adb_do shell am start -n "$PACKAGE_NAME/$ACTIVITY_NAME" | tee "$DEMO_DIR/launch.txt" || true

cat <<'CHECKLIST'

================= DEMO HEADSET CHECKLIST (human) =================
Put on the Quest and confirm:
  - Moving-target bubbles spawn and travel.
  - Right trigger resolves targets.
  - GSR LIVE panel visible/updating.
  - Session ends naturally at ~120 seconds.
  - Demo Summary panel appears.
These are user-observed; the script does not verify them.
=================================================================

CHECKLIST

log_info "Capturing Demo logs for up to ${CAPTURE_SECONDS}s (Ctrl+C to stop early after summary)."
# Sleep in short increments so Ctrl+C is responsive; allow early confirm.
SECS=0
while [[ "$SECS" -lt "$CAPTURE_SECONDS" ]]; do
    sleep 5
    SECS=$((SECS + 5))
    if grep -q -i 'DemoSummary' "$RAW_LOGCAT_FILE" 2>/dev/null; then
        log_info "DemoSummary observed in logs at ~${SECS}s (still capturing to be safe)."
    fi
done

printf 'Type CONFIRMED only if the user explicitly observed every Demo item; otherwise press Enter: '
read -r DEMO_CONFIRMATION
if [[ "$DEMO_CONFIRMATION" == "CONFIRMED" ]]; then
    printf 'USER-OBSERVED\n' > "$DEMO_DIR/human_checklist_status.txt"
else
    printf 'NOT VERIFIED\n' > "$DEMO_DIR/human_checklist_status.txt"
fi

stop_logcat_processes_started_by_script
adb_do shell am force-stop "$PACKAGE_NAME" || true
log_info "Demo capture complete. Raw log: $RAW_LOGCAT_FILE"
log_info "Next: phase1_restore_and_report.sh"
