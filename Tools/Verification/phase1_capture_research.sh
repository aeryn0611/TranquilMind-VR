#!/usr/bin/env bash
# phase1_capture_research.sh
# Install + launch the exact Research APK, capture raw logcat, guide the human,
# then pull only the JSONL bound to the current launch session.
#
# The script cannot verify headset visuals/timing. Each item remains NOT VERIFIED
# unless the user explicitly confirms it.
#
# Requires: a previously produced Research APK path (research/apk_path.txt) and
# exactly one authorized Quest.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
if [[ "${1:-}" != "--resume" ]]; then
    printf 'Usage: bash %s --resume\n' "$0" >&2
    exit 2
fi
# shellcheck source=phase1_verify_common.sh
source "$SCRIPT_DIR/phase1_verify_common.sh"
init_run_context resume

RESEARCH_DIR="$VERIFICATION_DIR/research"
JSONL_DIR="$VERIFICATION_DIR/jsonl"
RAW_LOGCAT_FILE="$RESEARCH_DIR/research_logcat_raw.txt"
FILTERED_LOGCAT_FILE="$RESEARCH_DIR/research_logcat_filtered.txt"
APK_PATH_FILE="$RESEARCH_DIR/apk_path.txt"

RESEARCH_FILTER='TMResearch|Research|OperatingMode|Mode|Trial|Schedule|Logger|Block|Fatal|Error|crash'
EXPECTED_SEED="20260708"
EXPECTED_HASH="4922870221080512783"
DEVICE_READY=0

cleanup() {
    local rc=$?
    trap - EXIT INT TERM
    if [[ "$DEVICE_READY" == "1" ]]; then
        stop_logcat_processes_started_by_script || rc=1
        adb_do shell am force-stop "$PACKAGE_NAME" 2>/dev/null || true
        restore_quest_power_state || rc=1
    fi
    log_info "phase1_capture_research.sh exiting (rc=$rc). Outputs: $RESEARCH_DIR"
    exit "$rc"
}
trap 'exit 130' INT
trap 'exit 143' TERM
trap cleanup EXIT

require_adb

# --- APK path ---
if [[ ! -f "$APK_PATH_FILE" ]]; then
    log_error "Missing $APK_PATH_FILE. Run phase1_package_research.sh first."
    exit 1
fi
APK_PATH="$(cat "$APK_PATH_FILE")"
require_file "$APK_PATH"
if [[ ! -f "$RESEARCH_DIR/apk_metadata.txt" ]]; then
    log_error "Missing APK metadata for this run."
    exit 1
fi
RECORDED_APK_SHA="$(awk -F= '$1=="sha256" {print $2}' "$RESEARCH_DIR/apk_metadata.txt")"
CURRENT_APK_SHA="$(sha256_file "$APK_PATH")"
if [[ -z "$RECORDED_APK_SHA" || "$CURRENT_APK_SHA" != "$RECORDED_APK_SHA" ]]; then
    log_error "APK hash no longer matches this run's packaged artifact."
    exit 1
fi

# --- Device ---
adb_single_device_check
DEVICE_READY=1

log_info "Device power/wifi context:"
adb_do shell dumpsys power 2>/dev/null | grep -i -E 'mWakefulness|mHoldingDisplaySuspendBlocker' \
    | tee "$RESEARCH_DIR/power_state.txt" || true
adb_do shell dumpsys wifi 2>/dev/null | grep -i 'SSID' \
    | tee "$RESEARCH_DIR/wifi.txt" || log_warn "Could not read wifi SSID (permissions vary)."

# --- Power: require a known original value, wake, and enable USB stay-awake. ---
save_quest_power_state
log_info "Waking device (KEYCODE_WAKEUP)."
adb_do shell input keyevent KEYCODE_WAKEUP || true
enable_usb_stay_awake

# --- Install ---
log_info "Installing Research APK: $APK_PATH"
run_logged "$RESEARCH_DIR/install" adb_do install -r "$APK_PATH"
printf 'INSTALLED\napk_sha256=%s\n' "$CURRENT_APK_SHA" > "$RESEARCH_DIR/install.status"

# --- Start raw logcat, then launch. Do not clear unrelated device logs. ---
start_logcat_capture "$RAW_LOGCAT_FILE" "$FILTERED_LOGCAT_FILE" "$RESEARCH_FILTER"

log_info "Force-stop + launch $PACKAGE_NAME/$ACTIVITY_NAME"
adb_do shell am force-stop "$PACKAGE_NAME" || true
if ! adb_do shell am start -W -n "$PACKAGE_NAME/$ACTIVITY_NAME" \
    | tee "$RESEARCH_DIR/launch.txt"; then
    log_error "Application launch command failed."
    exit 1
fi

# Fail closed unless the current launch logs prove Research/default contract startup.
SESSION_ID=""
for _ in $(seq 1 30); do
    if grep -q 'OperatingMode = DEMO' "$RAW_LOGCAT_FILE" 2>/dev/null; then
        log_error "Current package launched in Demo Mode; stopping before human trial."
        exit 1
    fi
    SESSION_ID="$(extract_research_session_from_log "$RAW_LOGCAT_FILE" || true)"
    if [[ -n "$SESSION_ID" ]] &&
       grep -q 'OperatingMode = RESEARCH' "$RAW_LOGCAT_FILE" &&
       grep -q "seed=$EXPECTED_SEED | hash=$EXPECTED_HASH" "$RAW_LOGCAT_FILE" &&
       grep -q '\[Research\] Initialized' "$RAW_LOGCAT_FILE"; then
        break
    fi
    sleep 1
done

APP_PID="$(adb_do shell pidof "$PACKAGE_NAME" 2>/dev/null | tr -d '\r\n ' || true)"
MODE_EVIDENCE_COUNT="$(grep -c 'OperatingMode = RESEARCH' "$RAW_LOGCAT_FILE" || true)"
SCHEDULE_EVIDENCE_COUNT="$(grep -c "seed=$EXPECTED_SEED | hash=$EXPECTED_HASH" "$RAW_LOGCAT_FILE" || true)"
START_EVIDENCE_COUNT="$(grep -c '\[Research\] Initialized' "$RAW_LOGCAT_FILE" || true)"
if [[ -z "$APP_PID" || -z "$SESSION_ID" ||
      ! "$SESSION_ID" =~ ^[A-Za-z0-9-]+$ ||
      "$MODE_EVIDENCE_COUNT" -lt 1 ||
      "$SCHEDULE_EVIDENCE_COUNT" -lt 1 ||
      "$START_EVIDENCE_COUNT" -lt 1 ]]; then
    log_error "Research launch evidence is incomplete or ambiguous; stopping before human trial."
    exit 1
fi
{
    printf 'LAUNCHED\n'
    printf 'app_pid=%s\n' "$APP_PID"
    printf 'session_id=%s\n' "$SESSION_ID"
    printf 'mode=Research\nseed=%s\nsequence_hash=%s\n' "$EXPECTED_SEED" "$EXPECTED_HASH"
    printf 'block_start_evidence=[Research] Initialized\n'
    printf 'raw_log=%s\n' "$RAW_LOGCAT_FILE"
} > "$RESEARCH_DIR/launch_evidence.txt"
printf '%s\n' "$SESSION_ID" > "$RESEARCH_DIR/session_id.txt"

# --- Human checklist. No item is marked observed unless explicitly supplied. ---
cat <<'CHECKLIST'

================ Research 头显检查（需要用户观察） ================
请在 50 个试次中尽量完成：GO Hit、GO Omission、NOGO Commission、
NOGO CorrectRejection、视觉消失后但 deadline 前响应、快速重复扣动。
请观察：刺激固定在中央且不沿深度移动；早期响应后仍显示约 400 ms；
视觉消失后 deadline 前仍可响应；一次按键只解决当前试次；重复按键不产生
可见双重计分；无崩溃；区块正常结束。

================ Research headset checks (USER-OBSERVED) ==========
During the 50 trials, deliberately demonstrate where feasible: GO Hit,
GO Omission, NOGO Commission, NOGO CorrectRejection, a post-offset but
pre-deadline response, and a rapid duplicate-trigger attempt.
Observe: fixed central stimulus with no depth travel; about 400 ms visibility
even after an early response; post-offset response acceptance before deadline;
one press resolves only the active trial; no visible double scoring; no crash;
and normal block completion.
====================================================================

CHECKLIST

printf 'After the block, enter ALL only if the user explicitly confirmed every item; otherwise enter comma-separated observed IDs or NONE: '
read -r OBSERVED_IDS
printf '%s\n' "$OBSERVED_IDS" > "$RESEARCH_DIR/human_observation_input.txt"
CHECK_IDS="hit omission commission correct_rejection post_offset_response duplicate_attempt fixed_location no_depth fixed_exposure post_offset_acceptance single_resolution no_double_scoring no_crash normal_completion"
: > "$RESEARCH_DIR/human_checks.tsv"
for check_id in $CHECK_IDS; do
    status="NOT VERIFIED"
    if [[ "$OBSERVED_IDS" == "ALL" ]] ||
       printf ',%s,' "$OBSERVED_IDS" | grep -q ",$check_id,"; then
        status="USER-OBSERVED"
    fi
    printf '%s\t%s\n' "$check_id" "$status" >> "$RESEARCH_DIR/human_checks.tsv"
done

# --- Stop only our logcat, force-stop app ---
stop_logcat_processes_started_by_script
adb_do shell am force-stop "$PACKAGE_NAME" || true

# --- Device-log completion/finalization evidence. ---
if ! grep -q '\[Research\] BLOCK BEHAVIORAL-COMPLETE' "$RAW_LOGCAT_FILE" ||
   ! grep -q '\[Research\]\[Logger\] CLOSED healthy | completion=normal' "$RAW_LOGCAT_FILE"; then
    log_error "Normal block completion and healthy footer were not both proven in device logs."
    exit 1
fi
if grep -Eqi 'FOOTER WRITE FAILED|CLOSED UNHEALTHY|Assertion failed|SIGSEGV|Fatal error|beginning of crash' "$RAW_LOGCAT_FILE"; then
    log_error "Logger failure, crash, or assertion found in current device log."
    exit 1
fi
printf 'DEVICE-LOG VERIFIED\nsession_id=%s\n' "$SESSION_ID" > "$RESEARCH_DIR/device_log.status"

# --- Pull only the JSONL whose filename matches the current launch session. ---
REMOTE_JSONL="$(select_session_jsonl_remote_path "$SESSION_ID")"
DEST="$JSONL_DIR/$SESSION_ID.jsonl"
if [[ -e "$DEST" ]]; then
    log_error "Local JSONL already exists; refusing overwrite: $DEST"
    exit 1
fi
run_logged "$JSONL_DIR/pull" adb_do pull "$REMOTE_JSONL" "$DEST"
printf '%s\n' "$REMOTE_JSONL" > "$JSONL_DIR/device_path.txt"
printf '%s\n' "$DEST" > "$JSONL_DIR/pulled_path.txt"
record_file_metadata "$DEST" "$JSONL_DIR/pull_metadata.txt"
printf 'remote_path=%s\nsession_id=%s\npull_utc=%s\n' \
    "$REMOTE_JSONL" "$SESSION_ID" "$(date -u +%Y-%m-%dT%H:%M:%SZ)" \
    >> "$JSONL_DIR/pull_metadata.txt"
printf 'JSONL PULLED\n' > "$JSONL_DIR/pull.status"

python3 "$SCRIPT_DIR/phase1_inspect_jsonl.py" "$DEST" \
    --expected-session "$SESSION_ID" \
    --json-out "$JSONL_DIR/inspection_summary.json" \
    --markdown-out "$JSONL_DIR/inspection_summary.md"
printf 'JSONL INSPECTED\n' > "$JSONL_DIR/inspection.status"
log_info "Research capture, session-bound pull, and strict inspection completed."
