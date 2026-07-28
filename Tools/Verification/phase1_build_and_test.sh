#!/usr/bin/env bash
# phase1_build_and_test.sh
# 1) Capture read-only git state.
# 2) Build the TranquilMindEditor Mac Development target.
# 3) Run the Research sequence automation tests.
# 4) Save full logs + extract first compiler-error context and a test summary.
# Stops (nonzero) before packaging if the build OR the tests fail.
#
# This script does NOT package or touch the device.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RUN_ARG="${1:-}"
case "$RUN_ARG" in
    --new-run) RUN_ACTION="new" ;;
    --resume) RUN_ACTION="resume" ;;
    *)
        printf 'Usage: bash %s --new-run|--resume\n' "$0" >&2
        exit 2
        ;;
esac
# shellcheck source=phase1_verify_common.sh
source "$SCRIPT_DIR/phase1_verify_common.sh"
init_run_context "$RUN_ACTION"

BUILD_DIR="$VERIFICATION_DIR/build"
TESTS_DIR="$VERIFICATION_DIR/tests"

cleanup() {
    local rc=$?
    trap - EXIT INT TERM
    stop_logcat_processes_started_by_script
    log_info "phase1_build_and_test.sh exiting (rc=$rc). Outputs: $VERIFICATION_DIR"
    exit "$rc"
}
trap 'exit 130' INT
trap 'exit 143' TERM
trap cleanup EXIT

# --- Preconditions ---
require_project_paths
require_engine_paths

# --- 1. Git preflight (read-only) ---
capture_git_state

# --- 2. Build editor target ---
BUILD_SH="$UE_ROOT/Engine/Build/BatchFiles/Mac/Build.sh"
require_exec "$BUILD_SH"

log_info "Building TranquilMindEditor (Mac Development)..."
BUILD_RC=0
run_logged "$BUILD_DIR/build" \
    "$BUILD_SH" TranquilMindEditor Mac Development -project="$UPROJECT" || BUILD_RC=$?

if [[ "$BUILD_RC" -ne 0 ]]; then
    log_error "Editor build FAILED (rc=$BUILD_RC). Extracting first error context."
    # First compiler error context (a few lines around the first 'error').
    grep -n -i -E 'error[: ]' "$BUILD_DIR/build.log" | head -n 20 \
        > "$BUILD_DIR/first_errors.txt" 2>/dev/null || true
    # Wider context around the first match.
    {
        FIRST_LINE="$(grep -n -i -E 'error[: ]' "$BUILD_DIR/build.log" | head -n 1 | cut -d: -f1 || true)"
        if [[ -n "${FIRST_LINE:-}" ]]; then
            START=$(( FIRST_LINE > 10 ? FIRST_LINE - 10 : 1 ))
            sed -n "${START},$((FIRST_LINE + 15))p" "$BUILD_DIR/build.log"
        else
            echo "No line matched /error[: ]/ — inspect $BUILD_DIR/build.log directly."
        fi
    } > "$BUILD_DIR/first_error_context.txt" 2>/dev/null || true
    log_error "See $BUILD_DIR/first_error_context.txt and $BUILD_DIR/build.log"
    log_error "STOP: not proceeding to tests or packaging."
    exit "$BUILD_RC"
fi
log_info "Editor build succeeded."
printf 'COMPILED\n' > "$BUILD_DIR/build.status"

# --- 3. Run both Research automation suites into dedicated raw UE logs. ---
UE_CMD="$UE_ROOT/Engine/Binaries/Mac/UnrealEditor-Cmd"
require_exec "$UE_CMD"

SEQUENCE_RAW="$TESTS_DIR/sequence_ue.log"
LIFECYCLE_RAW="$TESTS_DIR/lifecycle_ue.log"
TEST_RC=0

log_info "Running automation suite: TranquilMind.Research.Sequence"
run_logged "$TESTS_DIR/sequence_console" \
    "$UE_CMD" "$UPROJECT" \
    -ExecCmds="Automation RunTests TranquilMind.Research.Sequence; Quit" \
    -unattended -nopause -nullrhi -nosplash "-abslog=$SEQUENCE_RAW" || TEST_RC=$?
if [[ "$TEST_RC" -ne 0 ]]; then
    log_error "Sequence automation process failed (rc=$TEST_RC)."
    exit "$TEST_RC"
fi

log_info "Running automation suite: TranquilMind.Research.Lifecycle"
run_logged "$TESTS_DIR/lifecycle_console" \
    "$UE_CMD" "$UPROJECT" \
    -ExecCmds="Automation RunTests TranquilMind.Research.Lifecycle; Quit" \
    -unattended -nopause -nullrhi -nosplash "-abslog=$LIFECYCLE_RAW" || TEST_RC=$?
if [[ "$TEST_RC" -ne 0 ]]; then
    log_error "Lifecycle automation process failed (rc=$TEST_RC)."
    exit "$TEST_RC"
fi

# --- 4. Parse actual Automation Framework evidence; fail closed if ambiguous. ---
python3 "$SCRIPT_DIR/phase1_parse_automation.py" \
    --suite "TranquilMind.Research.Sequence|7|$SEQUENCE_RAW" \
    --suite "TranquilMind.Research.Lifecycle|1|$LIFECYCLE_RAW" \
    --output "$TESTS_DIR/automation_summary.json" \
    > "$TESTS_DIR/automation_summary.txt"
printf 'TESTED\n' > "$TESTS_DIR/automation.status"

log_info "Build + tests completed. Review:"
log_info "  Build log     : $BUILD_DIR/build.log"
log_info "  Sequence raw  : $SEQUENCE_RAW"
log_info "  Lifecycle raw : $LIFECYCLE_RAW"
log_info "  Parsed result : $TESTS_DIR/automation_summary.json"
log_info "Next: bash Tools/Verification/phase1_package_research.sh --resume"
