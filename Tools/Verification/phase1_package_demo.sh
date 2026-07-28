#!/usr/bin/env bash
# phase1_package_demo.sh
# Set OperatingMode=Demo and package a separate Demo regression build.
# Restores the original DefaultGame.ini on exit (even on failure).

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
ARCHIVE_DIR="$PROJECT_ROOT/ArchivedBuilds/Phase1Verification/$RUN_TS/Demo"

cleanup() {
    local rc=$?
    trap - EXIT INT TERM
    restore_default_game_ini || rc=1
    stop_logcat_processes_started_by_script
    log_info "phase1_package_demo.sh exiting (rc=$rc). Outputs: $DEMO_DIR"
    exit "$rc"
}
trap 'exit 130' INT
trap 'exit 143' TERM
trap cleanup EXIT

require_project_paths
require_engine_paths

RUNUAT="$UE_ROOT/Engine/Build/BatchFiles/RunUAT.sh"
require_exec "$RUNUAT"

backup_default_game_ini
set_operating_mode "Demo"
cp -p "$DEFAULT_GAME_INI" "$DEMO_DIR/DefaultGame.ini.used"

mkdir -p "$(dirname "$ARCHIVE_DIR")"
if ! mkdir "$ARCHIVE_DIR"; then
    log_error "Demo archive already exists; refusing overwrite/reuse: $ARCHIVE_DIR"
    exit 1
fi
PACKAGE_STARTED_EPOCH="$(date +%s)"
log_info "Packaging Demo build -> $ARCHIVE_DIR"
PKG_RC=0
run_logged "$DEMO_DIR/package_demo" \
    "$RUNUAT" BuildCookRun \
    -project="$UPROJECT" \
    -platform=Android \
    -cookflavor=ASTC \
    -clientconfig=Development \
    -cook -build -stage -pak -package -archive \
    -archivedirectory="$ARCHIVE_DIR" || PKG_RC=$?

grep -i -E 'warning|material|android|astc' "$DEMO_DIR/package_demo.log" \
    > "$DEMO_DIR/package_warnings.txt" 2>/dev/null || true

if [[ "$PKG_RC" -ne 0 ]]; then
    log_error "Demo packaging FAILED (rc=$PKG_RC). See $DEMO_DIR/package_demo.log"
    exit "$PKG_RC"
fi

APK_PATH="$(find_run_arm64_apk "$ARCHIVE_DIR" "$PACKAGE_STARTED_EPOCH")"
printf '%s\n' "$APK_PATH" > "$DEMO_DIR/apk_path.txt"
record_file_metadata "$APK_PATH" "$DEMO_DIR/apk_metadata.txt"
printf 'PACKAGED\n' > "$DEMO_DIR/package.status"
log_info "Demo APK: $APK_PATH"
log_info "Recorded -> $DEMO_DIR/apk_path.txt"
log_info "Config restored on exit. Next: phase1_capture_demo.sh"
