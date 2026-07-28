#!/usr/bin/env bash
# phase1_restore_and_report.sh
# Restore config + Quest power state if any backup remains, stop toolkit logcat,
# capture final read-only git state, and assemble FINAL_REPORT.md from whatever
# evidence exists. Never marks a step PASS without evidence.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
if [[ "${1:-}" != "--resume" ]]; then
    printf 'Usage: bash %s --resume [--close]\n' "$0" >&2
    exit 2
fi
CLOSE_RUN=0
if [[ "${2:-}" == "--close" ]]; then
    CLOSE_RUN=1
elif [[ -n "${2:-}" ]]; then
    printf 'Unknown argument: %s\n' "$2" >&2
    exit 2
fi
# shellcheck source=phase1_verify_common.sh
source "$SCRIPT_DIR/phase1_verify_common.sh"
init_run_context resume

REPORT="$VERIFICATION_DIR/FINAL_REPORT.md"

cleanup() {
    local rc=$?
    trap - EXIT INT TERM
    stop_logcat_processes_started_by_script
    log_info "phase1_restore_and_report.sh exiting (rc=$rc)."
    exit "$rc"
}
trap 'exit 130' INT
trap 'exit 143' TERM
trap cleanup EXIT

# --- 1. Restore exact config bytes when this run owns a backup. ---
RESTORE_FAILURE=0
if [[ -f "$VERIFICATION_DIR/config/backup_manifest.txt" ]]; then
    restore_default_game_ini || RESTORE_FAILURE=1
fi

# --- 2. Restore this run's device state and owned logcat process. ---
DEVICE_EVIDENCE=0
if [[ -f "$VERIFICATION_DIR/device/adb_serial.txt" ]]; then
    if adb_single_device_check; then
        DEVICE_EVIDENCE=1
        stop_logcat_processes_started_by_script || RESTORE_FAILURE=1
        if [[ -f "$VERIFICATION_DIR/device/quest_stay_awake.orig" ]]; then
            restore_quest_power_state || RESTORE_FAILURE=1
        fi
    else
        log_error "Could not reselect this run's Quest for restoration."
        RESTORE_FAILURE=1
    fi
fi

# --- 3. Final read-only Git evidence. ---
capture_git_state
{
    printf '%s\n' '--- git diff --stat ---'
    ( cd "$PROJECT_ROOT" && git diff --stat ) 2>&1 || true
    printf '%s\n' '--- git diff --name-only ---'
    ( cd "$PROJECT_ROOT" && git diff --name-only ) 2>&1 || true
} > "$VERIFICATION_DIR/git/final_git.txt"

status_from_file() {
    local file="$1" label="$2"
    if [[ -s "$file" ]]; then
        printf '%s' "$label"
    else
        printf 'NOT VERIFIED'
    fi
}

COMPILED_STATUS="$(status_from_file "$VERIFICATION_DIR/build/build.status" COMPILED)"
TESTED_STATUS="$(status_from_file "$VERIFICATION_DIR/tests/automation.status" TESTED)"
PACKAGED_STATUS="$(status_from_file "$VERIFICATION_DIR/research/package.status" PACKAGED)"
INSTALLED_STATUS="$(status_from_file "$VERIFICATION_DIR/research/install.status" INSTALLED)"
LAUNCHED_STATUS="$(status_from_file "$VERIFICATION_DIR/research/launch_evidence.txt" LAUNCHED)"
DEVICE_LOG_STATUS="$(status_from_file "$VERIFICATION_DIR/research/device_log.status" DEVICE-LOG\ VERIFIED)"
PULLED_STATUS="$(status_from_file "$VERIFICATION_DIR/jsonl/pull.status" JSONL\ PULLED)"
INSPECTED_STATUS="$(status_from_file "$VERIFICATION_DIR/jsonl/inspection.status" JSONL\ INSPECTED)"
DEMO_STATUS="$(status_from_file "$VERIFICATION_DIR/demo/human_checklist_status.txt" DEMO\ REGRESSION)"
RESTORED_STATUS="NOT VERIFIED"
if [[ "$RESTORE_FAILURE" == "0" ]] &&
   { [[ ! -f "$VERIFICATION_DIR/config/backup_manifest.txt" ]] ||
     [[ -f "$VERIFICATION_DIR/config/restore_status.txt" ]]; } &&
   { [[ "$DEVICE_EVIDENCE" == "0" ]] ||
     [[ ! -f "$VERIFICATION_DIR/device/quest_stay_awake.orig" ]] ||
     [[ -f "$VERIFICATION_DIR/device/quest_power_restore.status" ]]; }; then
    RESTORED_STATUS="RESTORED"
fi

HUMAN_STATUS="NOT VERIFIED"
if [[ -s "$VERIFICATION_DIR/research/human_checks.tsv" ]] &&
   ! grep -q $'\tNOT VERIFIED$' "$VERIFICATION_DIR/research/human_checks.tsv"; then
    HUMAN_STATUS="USER-OBSERVED"
fi

OVERALL="NOT VERIFIED"
if [[ "$COMPILED_STATUS" == "COMPILED" &&
      "$TESTED_STATUS" == "TESTED" &&
      "$PACKAGED_STATUS" == "PACKAGED" &&
      "$INSTALLED_STATUS" == "INSTALLED" &&
      "$LAUNCHED_STATUS" == "LAUNCHED" &&
      "$DEVICE_LOG_STATUS" == "DEVICE-LOG VERIFIED" &&
      "$PULLED_STATUS" == "JSONL PULLED" &&
      "$INSPECTED_STATUS" == "JSONL INSPECTED" &&
      "$HUMAN_STATUS" == "USER-OBSERVED" &&
      "$RESTORED_STATUS" == "RESTORED" ]]; then
    OVERALL="PASS"
fi

CURRENT_HEAD="$(cat "$VERIFICATION_DIR/git/head.txt" 2>/dev/null || echo unknown)"
{
    printf '# TranquilMind Phase 1 — Verification Report\n\n'
    printf -- '- Run: `%s`\n' "$RUN_TS"
    printf -- '- HEAD: `%s`\n' "$CURRENT_HEAD"
    printf -- '- Overall: **%s**\n\n' "$OVERALL"
    printf '> Overall PASS requires every automated gate plus explicit user-observed headset checks.\n\n'
    printf '## Evidence status model\n\n'
    printf '| Gate | Status | Evidence |\n|---|---|---|\n'
    printf '| Build | %s | `%s` |\n' "$COMPILED_STATUS" "$VERIFICATION_DIR/build/build.log"
    printf '| Automation | %s | `%s` |\n' "$TESTED_STATUS" "$VERIFICATION_DIR/tests/automation_summary.json"
    printf '| Research package | %s | `%s` |\n' "$PACKAGED_STATUS" "$VERIFICATION_DIR/research/apk_metadata.txt"
    printf '| Install | %s | `%s` |\n' "$INSTALLED_STATUS" "$VERIFICATION_DIR/research/install.log"
    printf '| Launch | %s | `%s` |\n' "$LAUNCHED_STATUS" "$VERIFICATION_DIR/research/launch_evidence.txt"
    printf '| Device log | %s | `%s` |\n' "$DEVICE_LOG_STATUS" "$VERIFICATION_DIR/research/research_logcat_raw.txt"
    printf '| JSONL pull | %s | `%s` |\n' "$PULLED_STATUS" "$VERIFICATION_DIR/jsonl/pull_metadata.txt"
    printf '| JSONL inspection | %s | `%s` |\n' "$INSPECTED_STATUS" "$VERIFICATION_DIR/jsonl/inspection_summary.json"
    printf '| Quest headset checks | %s | `%s` |\n' "$HUMAN_STATUS" "$VERIFICATION_DIR/research/human_checks.tsv"
    printf '| Demo regression | %s | — |\n' "$DEMO_STATUS"
    printf '| Config/device restoration | %s | `%s/config`, `%s/device` |\n\n' \
        "$RESTORED_STATUS" "$VERIFICATION_DIR" "$VERIFICATION_DIR"

    if [[ -f "$VERIFICATION_DIR/jsonl/inspection_summary.md" ]]; then
        printf '## JSONL Inspector\n\n'
        cat "$VERIFICATION_DIR/jsonl/inspection_summary.md"
        printf '\n'
    fi

    printf '## Final Git status\n\n```\n'
    cat "$VERIFICATION_DIR/git/status_short.txt" 2>/dev/null || true
    printf '```\n\n'
    printf '## Guardrails\n\n'
    printf -- '- No headset observation was inferred automatically.\n'
    printf -- '- No Git staging, commit, or push was performed.\n'
    printf -- '- Demo regression remains a separate next gate.\n'
} > "$REPORT"

log_info "Final evidence-driven report written: $REPORT"
if [[ "$CLOSE_RUN" == "1" ]]; then
    close_current_run
fi
if [[ "$RESTORE_FAILURE" != "0" ]]; then
    exit 1
fi
