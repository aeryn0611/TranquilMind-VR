#!/usr/bin/env bash
# phase1_package_research.sh
# 1) Back up DefaultGame.ini.
# 2) Set OperatingMode=Research (only inside [/Script/TranquilMind.TMResearchSettings]).
# 3) Verify the mode line.
# 4) Package Android (ASTC, Development) into a timestamped Research archive.
# 5) Locate EXACTLY one APK; record its exact path.
# 6) Restore the original DefaultGame.ini on exit (even on failure).
#
# Does NOT install to the device.

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
ARCHIVE_DIR="$PROJECT_ROOT/ArchivedBuilds/Phase1Verification/$RUN_TS/Research"

cleanup() {
    local rc=$?
    trap - EXIT INT TERM
    # Always restore the config, even on failure/interrupt.
    restore_default_game_ini || rc=1
    stop_logcat_processes_started_by_script
    log_info "phase1_package_research.sh exiting (rc=$rc). Outputs: $RESEARCH_DIR"
    exit "$rc"
}
trap 'exit 130' INT
trap 'exit 143' TERM
trap cleanup EXIT

require_project_paths
require_engine_paths
capture_git_state

RUNUAT="$UE_ROOT/Engine/Build/BatchFiles/RunUAT.sh"
require_exec "$RUNUAT"

# --- 1-3. Config: backup + set Research + verify ---
backup_default_game_ini
set_operating_mode "Research"
cp -p "$DEFAULT_GAME_INI" "$RESEARCH_DIR/DefaultGame.ini.used"
log_info "Recorded the exact config used -> $RESEARCH_DIR/DefaultGame.ini.used"

# --- 4. Package into a never-reused archive directory. ---
mkdir -p "$(dirname "$ARCHIVE_DIR")"
if ! mkdir "$ARCHIVE_DIR"; then
    log_error "Research archive already exists; refusing overwrite/reuse: $ARCHIVE_DIR"
    exit 1
fi
PACKAGE_STARTED_EPOCH="$(date +%s)"
printf '%s\n' "$PACKAGE_STARTED_EPOCH" > "$RESEARCH_DIR/package_started_epoch.txt"
log_info "Packaging Research build -> $ARCHIVE_DIR"
PKG_RC=0
run_logged "$RESEARCH_DIR/package_research" \
    "$RUNUAT" BuildCookRun \
    -project="$UPROJECT" \
    -platform=Android \
    -cookflavor=ASTC \
    -clientconfig=Development \
    -cook -build -stage -pak -package -archive \
    -archivedirectory="$ARCHIVE_DIR" || PKG_RC=$?

# Surface material/Android warnings regardless of success.
grep -i -E 'warning|material|android|astc' "$RESEARCH_DIR/package_research.log" \
    > "$RESEARCH_DIR/package_warnings.txt" 2>/dev/null || true
log_info "Packaging warnings (if any) -> $RESEARCH_DIR/package_warnings.txt"

if [[ "$PKG_RC" -ne 0 ]]; then
    log_error "Research packaging FAILED (rc=$PKG_RC). See $RESEARCH_DIR/package_research.log"
    exit "$PKG_RC"
fi

# --- 5. Select only a new installable arm64 APK and record provenance. ---
APK_PATH="$(find_run_arm64_apk "$ARCHIVE_DIR" "$PACKAGE_STARTED_EPOCH")"
printf '%s\n' "$APK_PATH" > "$RESEARCH_DIR/apk_path.txt"
record_file_metadata "$APK_PATH" "$RESEARCH_DIR/apk_metadata.txt"
printf 'PACKAGED\n' > "$RESEARCH_DIR/package.status"
log_info "Research APK: $APK_PATH"
log_info "APK evidence -> $RESEARCH_DIR/apk_metadata.txt"

log_info "Done. Config will be restored and hash-verified on exit."
log_info "Next: bash Tools/Verification/phase1_capture_research.sh --resume"
