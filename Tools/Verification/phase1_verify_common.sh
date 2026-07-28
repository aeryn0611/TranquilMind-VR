#!/usr/bin/env bash
# phase1_verify_common.sh
# Shared config + helper functions for the TranquilMind Phase 1 verification toolkit.
#
# This file is SOURCED by the other scripts (it is not meant to be run directly).
# It defines paths, a timestamped output directory, and safe helper functions.
#
# SAFETY: nothing here (or in any toolkit script) ever runs git clean/reset/checkout/
# restore/commit/stage/push, and nothing deletes Packaged/, ArchivedBuilds/, Content
# assets, or user files. Config/DefaultGame.ini and the Quest stay-awake setting are
# always backed up and restored (including on error / Ctrl+C) via traps.

set -euo pipefail

# ------------------------------------------------------------------
# Fixed project configuration (exact paths).
# ------------------------------------------------------------------
PROJECT_ROOT="${PHASE1_PROJECT_ROOT:-/Users/aeryn/UnrealProjects/TranquilMind}"
UPROJECT="$PROJECT_ROOT/TranquilMind.uproject"
UE_ROOT="${PHASE1_UE_ROOT:-/Users/Shared/Epic Games/UE_5.7}"
ADB="${PHASE1_ADB:-/Users/aeryn/Library/Android/sdk/platform-tools/adb}"
PACKAGE_NAME="com.tranquilmind.vr"
ACTIVITY_NAME="com.epicgames.unreal.GameActivity"

DEFAULT_GAME_INI="$PROJECT_ROOT/Config/DefaultGame.ini"
RESEARCH_SETTINGS_SECTION="/Script/TranquilMind.TMResearchSettings"

# ------------------------------------------------------------------
# Explicit run creation/resume. Sourcing this file never creates or resumes a run.
# Call init_run_context new or init_run_context resume after parsing a script's
# required --new-run / --resume argument.
# ------------------------------------------------------------------
VERIFICATION_BASE="${PHASE1_VERIFICATION_BASE:-$PROJECT_ROOT/Verification/Phase1}"
RUN_TS_POINTER="$VERIFICATION_BASE/.current_run"
RUN_TS=""
VERIFICATION_DIR=""

_create_run_layout() {
    mkdir -p "$VERIFICATION_DIR"/{build,tests,research,demo,jsonl,git,demo_logs,device,config}
}

_validate_run_id() {
    [[ "$1" =~ ^[0-9]{8}_[0-9]{6}(_[0-9]{3})?$ ]]
}

_validate_resumed_run() {
    if [[ -L "$RUN_TS_POINTER" || ! -f "$RUN_TS_POINTER" ]]; then
        log_error "Run pointer is missing or is a symlink: $RUN_TS_POINTER"
        return 1
    fi

    local line_count
    line_count="$(awk 'END { print NR }' "$RUN_TS_POINTER")"
    if [[ "$line_count" != "1" ]]; then
        log_error "Run pointer must contain exactly one line: $RUN_TS_POINTER"
        return 1
    fi

    IFS= read -r RUN_TS < "$RUN_TS_POINTER"
    if ! _validate_run_id "$RUN_TS"; then
        log_error "Malformed run id in $RUN_TS_POINTER: '$RUN_TS'"
        return 1
    fi

    VERIFICATION_DIR="$VERIFICATION_BASE/$RUN_TS"
    if [[ -L "$VERIFICATION_DIR" || ! -d "$VERIFICATION_DIR" ]]; then
        log_error "Run directory is missing or is a symlink: $VERIFICATION_DIR"
        return 1
    fi

    local base_real parent_real run_real
    base_real="$(cd "$VERIFICATION_BASE" && pwd -P)"
    parent_real="$(cd "$(dirname "$VERIFICATION_DIR")" && pwd -P)"
    run_real="$(cd "$VERIFICATION_DIR" && pwd -P)"
    if [[ "$parent_real" != "$base_real" || "$run_real" != "$base_real/$RUN_TS" ]]; then
        log_error "Run pointer escapes Verification/Phase1: $RUN_TS_POINTER -> $run_real"
        return 1
    fi
}

_create_new_run() {
    mkdir -p "$VERIFICATION_BASE"
    if [[ -L "$VERIFICATION_BASE" ]]; then
        log_error "Verification base must not be a symlink: $VERIFICATION_BASE"
        return 1
    fi

    local base_id suffix
    base_id="$(date +%Y%m%d_%H%M%S)"
    RUN_TS="$base_id"
    suffix=0
    while ! mkdir "$VERIFICATION_BASE/$RUN_TS" 2>/dev/null; do
        suffix=$((suffix + 1))
        if [[ "$suffix" -gt 999 ]]; then
            log_error "Could not allocate a unique verification run id."
            return 1
        fi
        RUN_TS="${base_id}_$(printf '%03d' "$suffix")"
    done

    VERIFICATION_DIR="$VERIFICATION_BASE/$RUN_TS"
    _create_run_layout

    local pointer_tmp
    pointer_tmp="$(mktemp "$VERIFICATION_BASE/.current_run.tmp.XXXXXX")"
    printf '%s\n' "$RUN_TS" > "$pointer_tmp"
    mv "$pointer_tmp" "$RUN_TS_POINTER"
    log_info "Created new verification run: $VERIFICATION_DIR"
}

init_run_context() {
    local action="$1"
    case "$action" in
        new)
            _create_new_run
            ;;
        resume)
            _validate_resumed_run
            _create_run_layout
            log_info "Explicitly resumed verification run: $VERIFICATION_DIR"
            ;;
        *)
            log_error "Run action must be 'new' or 'resume' (got '$action')."
            return 2
            ;;
    esac
}

close_current_run() {
    _validate_resumed_run
    local closed_pointer="$VERIFICATION_DIR/current_run.closed"
    if [[ -e "$closed_pointer" ]]; then
        log_error "Run already has a close marker: $closed_pointer"
        return 1
    fi
    mv "$RUN_TS_POINTER" "$closed_pointer"
    printf '%s\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)" > "$VERIFICATION_DIR/closed_utc.txt"
    log_info "Closed verification run without deleting its evidence: $VERIFICATION_DIR"
}

# ------------------------------------------------------------------
# Logging helpers.
# ------------------------------------------------------------------
log_info()  { printf '[INFO]  %s\n' "$*"; }
log_warn()  { printf '[WARN]  %s\n' "$*" >&2; }
log_error() { printf '[ERROR] %s\n' "$*" >&2; }

# run_logged <log_basepath> <cmd...>
# Prints the command, runs it, tees stdout+stderr to <log_basepath>.log, and
# returns the command's real exit code (does not swallow failures).
run_logged() {
    local logbase="$1"; shift
    local logfile="${logbase}.log"
    mkdir -p "$(dirname "$logfile")"
    {
        printf '### CMD: '
        printf '%q ' "$@"
        printf '\n### TS : %s\n\n' "$(date +%Y-%m-%dT%H:%M:%S)"
    } | tee "$logfile"
    log_info "Running (output -> $logfile)"
    # Preserve exit status through the pipe.
    set +e
    "$@" 2>&1 | tee -a "$logfile"
    local rc="${PIPESTATUS[0]}"
    set -e
    printf '\n### EXIT: %s\n' "$rc" | tee -a "$logfile" >/dev/null
    return "$rc"
}

# ------------------------------------------------------------------
# Path / precondition checks.
# ------------------------------------------------------------------
require_file() {
    local p="$1"
    if [[ ! -f "$p" ]]; then
        log_error "Required file not found: $p"
        return 1
    fi
    log_info "OK file: $p"
}

require_dir() {
    local p="$1"
    if [[ ! -d "$p" ]]; then
        log_error "Required directory not found: $p"
        return 1
    fi
    log_info "OK dir : $p"
}

require_exec() {
    local p="$1"
    if [[ ! -x "$p" ]]; then
        log_error "Required executable not found / not executable: $p"
        return 1
    fi
    log_info "OK exec: $p"
}

# Verify the core project + engine + adb paths exist. Callers decide which subset
# they need; engine/adb are only required by build/package/device scripts.
require_project_paths() {
    require_file "$UPROJECT"
    require_file "$DEFAULT_GAME_INI"
}

require_engine_paths() {
    require_dir "$UE_ROOT"
    require_exec "$UE_ROOT/Engine/Build/BatchFiles/Mac/Build.sh"
    require_exec "$UE_ROOT/Engine/Binaries/Mac/UnrealEditor-Cmd"
}

require_adb() {
    require_exec "$ADB"
}

# ------------------------------------------------------------------
# Git state capture (READ-ONLY).
# ------------------------------------------------------------------
capture_git_state() {
    local outdir="$VERIFICATION_DIR/git"
    mkdir -p "$outdir"
    log_info "Capturing read-only git state -> $outdir"
    ( cd "$PROJECT_ROOT" && git status --short )            > "$outdir/status_short.txt" 2>&1 || true
    ( cd "$PROJECT_ROOT" && git diff --stat )               > "$outdir/diff_stat.txt"    2>&1 || true
    ( cd "$PROJECT_ROOT" && git diff --name-only )          > "$outdir/diff_name_only.txt" 2>&1 || true
    ( cd "$PROJECT_ROOT" && git log -1 --oneline )          > "$outdir/head.txt"         2>&1 || true
    (
        cd "$PROJECT_ROOT"
        for source_file in \
            Source/TranquilMind/Private/TMResearchRunner.cpp \
            Source/TranquilMind/Private/TMTrialLogger.cpp \
            Source/TranquilMind/Private/TargetSpawnerComponent.cpp \
            Source/TranquilMind/Private/Tests/TMSequenceGeneratorTest.cpp \
            Source/TranquilMind/Private/Tests/TMResearchLifecycleTest.cpp \
            Config/DefaultGame.ini; do
            if [[ -f "$source_file" ]]; then
                shasum -a 256 "$source_file"
            else
                printf 'MISSING  %s\n' "$source_file"
            fi
        done
    ) > "$outdir/source_sha256.txt" 2>&1
    log_info "HEAD: $(cat "$outdir/head.txt" 2>/dev/null || echo unknown)"
}

sha256_file() {
    shasum -a 256 "$1" | awk '{print $1}'
}

file_size_bytes() {
    stat -f '%z' "$1" 2>/dev/null || stat -c '%s' "$1"
}

file_mtime_epoch() {
    stat -f '%m' "$1" 2>/dev/null || stat -c '%Y' "$1"
}

file_mtime_text() {
    stat -f '%Sm' -t '%Y-%m-%dT%H:%M:%S%z' "$1" 2>/dev/null || stat -c '%y' "$1"
}

# ------------------------------------------------------------------
# ADB device validation.
#
# Requires exactly ONE authorized device unless a serial is provided via the
# ADB_SERIAL environment variable. Sets global ADB_TARGET to the adb -s prefix args.
# ------------------------------------------------------------------
ADB_TARGET=()  # array; expands to nothing or (-s <serial>)

adb_single_device_check() {
    require_adb
    local inventory="$VERIFICATION_DIR/device/adb_devices.txt"
    local inventory_tmp="${inventory}.tmp.$$"
    mkdir -p "$(dirname "$inventory")"
    if ! "$ADB" devices -l > "$inventory_tmp" 2>&1; then
        mv "$inventory_tmp" "$inventory"
        log_error "adb devices failed; inventory saved to $inventory"
        return 1
    fi
    mv "$inventory_tmp" "$inventory"
    log_info "ADB inventory saved: $inventory"
    cat "$inventory"

    local devices bad_states
    devices="$(awk 'NR>1 && $2=="device" {print $1}' "$inventory")"
    bad_states="$(awk 'NR>1 && NF>=2 && $2!="device" {print $1 "\t" $2}' "$inventory")"

    if [[ -n "${ADB_SERIAL:-}" ]]; then
        if ! grep -qx "$ADB_SERIAL" <<< "$devices"; then
            log_error "Requested ADB_SERIAL=$ADB_SERIAL is not an authorized device."
            return 1
        fi
        ADB_TARGET=(-s "$ADB_SERIAL")
        log_info "Using explicit authorized device: $ADB_SERIAL"
    else
        if [[ -n "$bad_states" ]]; then
            log_error "ADB inventory contains unauthorized/offline/non-device entries:"
            printf '%s\n' "$bad_states" >&2
            return 1
        fi
        local count
        count="$(printf '%s\n' "$devices" | sed '/^$/d' | wc -l | tr -d ' ')"
        if [[ "$count" != "1" ]]; then
            log_error "Exactly one authorized device is required without ADB_SERIAL; found $count."
            printf '%s\n' "$devices" >&2
            return 1
        fi
        ADB_TARGET=(-s "$(printf '%s' "$devices")")
        log_info "Using sole authorized device: $(printf '%s' "$devices")"
    fi

    local selected="${ADB_TARGET[1]}"
    local selected_file="$VERIFICATION_DIR/device/adb_serial.txt"
    if [[ -f "$selected_file" ]]; then
        local recorded
        recorded="$(cat "$selected_file")"
        if [[ "$recorded" != "$selected" ]]; then
            log_error "Run is bound to ADB serial '$recorded', not '$selected'."
            return 1
        fi
    else
        printf '%s\n' "$selected" > "$selected_file"
    fi
}

# adb_do <args...> : run adb against the validated single device.
adb_do() {
    "$ADB" "${ADB_TARGET[@]}" "$@"
}

# ------------------------------------------------------------------
# Config (DefaultGame.ini) backup / mode switch / restore.
#
# The backup preserves the EXACT original bytes. Restore copies them back.
# set_operating_mode edits ONLY the OperatingMode entry within the
# [/Script/TranquilMind.TMResearchSettings] section (never a blind global replace).
# ------------------------------------------------------------------
CONFIG_BACKUP=""
CONFIG_MANIFEST=""

backup_default_game_ini() {
    require_file "$DEFAULT_GAME_INI"
    CONFIG_BACKUP="$VERIFICATION_DIR/config/DefaultGame.ini.orig"
    CONFIG_MANIFEST="$VERIFICATION_DIR/config/backup_manifest.txt"
    mkdir -p "$(dirname "$CONFIG_BACKUP")"

    if [[ ! -e "$CONFIG_BACKUP" && ! -e "$CONFIG_MANIFEST" ]]; then
        local original_sha backup_sha manifest_tmp
        original_sha="$(sha256_file "$DEFAULT_GAME_INI")"
        cp -p "$DEFAULT_GAME_INI" "$CONFIG_BACKUP"
        backup_sha="$(sha256_file "$CONFIG_BACKUP")"
        if [[ "$original_sha" != "$backup_sha" ]]; then
            log_error "Config backup hash mismatch immediately after copy."
            return 1
        fi
        manifest_tmp="${CONFIG_MANIFEST}.tmp.$$"
        {
            printf 'original_path=%s\n' "$DEFAULT_GAME_INI"
            printf 'original_sha256=%s\n' "$original_sha"
            printf 'backup_path=%s\n' "$CONFIG_BACKUP"
            printf 'backup_sha256=%s\n' "$backup_sha"
        } > "$manifest_tmp"
        mv "$manifest_tmp" "$CONFIG_MANIFEST"
        log_info "Captured fresh per-run config backup: $CONFIG_BACKUP"
        return 0
    fi

    if [[ ! -f "$CONFIG_BACKUP" || ! -f "$CONFIG_MANIFEST" || -L "$CONFIG_BACKUP" || -L "$CONFIG_MANIFEST" ]]; then
        log_error "Incomplete or symlinked config backup state in current run; refusing reuse."
        return 1
    fi

    local manifest_original manifest_original_sha manifest_backup manifest_backup_sha
    manifest_original="$(awk -F= '$1=="original_path" {sub(/^[^=]*=/, ""); print}' "$CONFIG_MANIFEST")"
    manifest_original_sha="$(awk -F= '$1=="original_sha256" {print $2}' "$CONFIG_MANIFEST")"
    manifest_backup="$(awk -F= '$1=="backup_path" {sub(/^[^=]*=/, ""); print}' "$CONFIG_MANIFEST")"
    manifest_backup_sha="$(awk -F= '$1=="backup_sha256" {print $2}' "$CONFIG_MANIFEST")"

    if [[ "$manifest_original" != "$DEFAULT_GAME_INI" || "$manifest_backup" != "$CONFIG_BACKUP" ]]; then
        log_error "Config manifest paths do not belong to this run."
        return 1
    fi
    if [[ "$(sha256_file "$CONFIG_BACKUP")" != "$manifest_backup_sha" ||
          "$manifest_backup_sha" != "$manifest_original_sha" ]]; then
        log_error "Existing run backup does not match its recorded original snapshot."
        return 1
    fi
    if [[ "$(sha256_file "$DEFAULT_GAME_INI")" != "$manifest_original_sha" ]]; then
        log_error "Live DefaultGame.ini differs from this run's original snapshot; refusing stale backup reuse."
        return 1
    fi
    log_info "Existing backup verified as the same current-run original snapshot."
}

restore_default_game_ini() {
    CONFIG_BACKUP="${CONFIG_BACKUP:-$VERIFICATION_DIR/config/DefaultGame.ini.orig}"
    CONFIG_MANIFEST="${CONFIG_MANIFEST:-$VERIFICATION_DIR/config/backup_manifest.txt}"
    if [[ ! -f "$CONFIG_BACKUP" || ! -f "$CONFIG_MANIFEST" ||
          -L "$CONFIG_BACKUP" || -L "$CONFIG_MANIFEST" ]]; then
        log_warn "No config backup recorded; nothing to restore."
        return 0
    fi

    local expected_original expected_backup recorded_path
    recorded_path="$(awk -F= '$1=="original_path" {sub(/^[^=]*=/, ""); print}' "$CONFIG_MANIFEST")"
    expected_original="$(awk -F= '$1=="original_sha256" {print $2}' "$CONFIG_MANIFEST")"
    expected_backup="$(awk -F= '$1=="backup_sha256" {print $2}' "$CONFIG_MANIFEST")"
    if [[ "$recorded_path" != "$DEFAULT_GAME_INI" ||
          "$(sha256_file "$CONFIG_BACKUP")" != "$expected_backup" ||
          "$expected_original" != "$expected_backup" ]]; then
        log_error "Current-run config backup manifest failed validation; refusing restore."
        return 1
    fi

    cp -p "$CONFIG_BACKUP" "$DEFAULT_GAME_INI"
    local restored_sha
    restored_sha="$(sha256_file "$DEFAULT_GAME_INI")"
    if [[ "$restored_sha" != "$expected_original" ]]; then
        log_error "DefaultGame.ini restore hash mismatch: expected $expected_original, got $restored_sha"
        return 1
    fi
    printf 'RESTORED\nsha256=%s\n' "$restored_sha" > "$VERIFICATION_DIR/config/restore_status.txt"
    log_info "Restored exact DefaultGame.ini bytes and verified SHA-256: $restored_sha"
}

# set_operating_mode <Demo|Research>
# Edits only the OperatingMode line inside the TMResearchSettings section using awk.
set_operating_mode() {
    local mode="$1"
    if [[ "$mode" != "Demo" && "$mode" != "Research" ]]; then
        log_error "set_operating_mode: mode must be Demo or Research (got '$mode')."
        return 1
    fi
    require_file "$DEFAULT_GAME_INI"

    local section_count mode_count
    section_count="$(awk -v section="[$RESEARCH_SETTINGS_SECTION]" '$0==section {n++} END {print n+0}' "$DEFAULT_GAME_INI")"
    mode_count="$(awk -v section="[$RESEARCH_SETTINGS_SECTION]" '
        $0==section {in_sec=1; next}
        in_sec && /^\[.*\]/ {in_sec=0}
        in_sec && /^OperatingMode[ \t]*=/ {n++}
        END {print n+0}
    ' "$DEFAULT_GAME_INI")"
    if [[ "$section_count" -gt 1 ]]; then
        log_error "Duplicate TMResearchSettings sections detected ($section_count); refusing mutation."
        return 1
    fi
    if [[ "$mode_count" -gt 1 ]]; then
        log_error "Duplicate OperatingMode entries detected ($mode_count); refusing mutation."
        return 1
    fi

    local tmp
    tmp="$(mktemp "${TMPDIR:-/tmp}/DefaultGame.XXXXXX.ini")"

    # State machine: only inside the target section, replace an existing
    # OperatingMode=... line. If the section exists but has no OperatingMode line,
    # insert one right after the section header. If the section is absent, append it.
    awk -v section="[$RESEARCH_SETTINGS_SECTION]" -v mode="$mode" '
        BEGIN { in_sec=0; seen_sec=0; replaced=0 }
        {
            line=$0
            if (line == section) {
                in_sec=1; seen_sec=1
                print line
                next
            }
            # A new section header ends the target section.
            if (in_sec==1 && line ~ /^\[.*\]/ && line != section) {
                if (replaced==0) {
                    print "OperatingMode=" mode
                    replaced=1
                }
                in_sec=0
                print line
                next
            }
            if (in_sec==1 && line ~ /^OperatingMode[ \t]*=/) {
                print "OperatingMode=" mode
                replaced=1
                next
            }
            print line
        }
        END {
            if (seen_sec==1 && replaced==0) {
                # Section was the last block and had no OperatingMode line.
                print "OperatingMode=" mode
            } else if (seen_sec==0) {
                # Section absent entirely; append it.
                print ""
                print section
                print "OperatingMode=" mode
            }
        }
    ' "$DEFAULT_GAME_INI" > "$tmp"

    mv "$tmp" "$DEFAULT_GAME_INI"

    # Verify exactly one resulting line.
    local resulting resulting_count
    resulting="$(awk -v section="[$RESEARCH_SETTINGS_SECTION]" '
        $0==section { in_sec=1; next }
        in_sec==1 && /^\[.*\]/ { in_sec=0 }
        in_sec==1 && /^OperatingMode[ \t]*=/ { print; exit }
    ' "$DEFAULT_GAME_INI")"
    resulting_count="$(awk -v section="[$RESEARCH_SETTINGS_SECTION]" '
        $0==section {in_sec=1; next}
        in_sec && /^\[.*\]/ {in_sec=0}
        in_sec && /^OperatingMode[ \t]*=/ {n++}
        END {print n+0}
    ' "$DEFAULT_GAME_INI")"

    if [[ "$resulting" != "OperatingMode=$mode" || "$resulting_count" != "1" ]]; then
        log_error "Failed to set exactly one OperatingMode=$mode (count=$resulting_count, found='${resulting:-<none>}')."
        restore_default_game_ini
        return 1
    fi
    log_info "OperatingMode set exactly once to $mode."
}

# ------------------------------------------------------------------
# Quest stay-awake power state save / restore.
#
# stay_on_while_plugged_in is a bitmask (0 = off). We record the exact original
# value and restore it verbatim. We only *offer* to enable USB stay-awake (bit 4)
# when the caller passes enable=1; even then we restore the original afterward.
# ------------------------------------------------------------------
QUEST_POWER_FILE=""
QUEST_POWER_ORIG=""

save_quest_power_state() {
    QUEST_POWER_FILE="$VERIFICATION_DIR/device/quest_stay_awake.orig"
    if [[ -f "$QUEST_POWER_FILE" ]]; then
        QUEST_POWER_ORIG="$(cat "$QUEST_POWER_FILE")"
        if [[ ! "$QUEST_POWER_ORIG" =~ ^[0-9]+$ ]]; then
            log_error "Recorded Quest power value is malformed: '$QUEST_POWER_ORIG'"
            return 1
        fi
        log_info "Using verified current-run Quest power snapshot: $QUEST_POWER_ORIG"
        return 0
    fi

    if ! QUEST_POWER_ORIG="$(adb_do shell settings get global stay_on_while_plugged_in 2>/dev/null | tr -d '\r\n ')"; then
        log_error "Could not determine Quest stay_on_while_plugged_in; refusing mutation."
        QUEST_POWER_ORIG=""
        return 1
    fi
    if [[ ! "$QUEST_POWER_ORIG" =~ ^[0-9]+$ ]]; then
        log_error "Quest returned an indeterminate stay-awake value: '$QUEST_POWER_ORIG'"
        QUEST_POWER_ORIG=""
        return 1
    fi
    printf '%s\n' "$QUEST_POWER_ORIG" > "$QUEST_POWER_FILE"
    log_info "Saved Quest stay_on_while_plugged_in=$QUEST_POWER_ORIG -> $QUEST_POWER_FILE"
}

# USB power is bit 2 in Android's stay_on_while_plugged_in bitmask.
enable_usb_stay_awake() {
    if [[ -z "$QUEST_POWER_ORIG" ]]; then
        log_error "No saved power state; refusing stay-awake mutation."
        return 1
    fi
    local temporary current
    temporary=$((QUEST_POWER_ORIG | 2))
    adb_do shell settings put global stay_on_while_plugged_in "$temporary"
    current="$(adb_do shell settings get global stay_on_while_plugged_in | tr -d '\r\n ')"
    if [[ "$current" != "$temporary" ]]; then
        log_error "Quest stay-awake mutation verification failed: expected $temporary, got '$current'"
        return 1
    fi
    printf 'original=%s\ntemporary=%s\n' "$QUEST_POWER_ORIG" "$temporary" \
        > "$VERIFICATION_DIR/device/quest_stay_awake.modified"
    log_info "Temporarily enabled USB stay-awake and verified value=$temporary."
}

restore_quest_power_state() {
    QUEST_POWER_FILE="${QUEST_POWER_FILE:-$VERIFICATION_DIR/device/quest_stay_awake.orig}"
    if [[ -z "$QUEST_POWER_ORIG" && -f "$QUEST_POWER_FILE" ]]; then
        QUEST_POWER_ORIG="$(cat "$QUEST_POWER_FILE")"
    fi
    if [[ -z "$QUEST_POWER_ORIG" ]]; then
        log_info "No Quest power state was modified; nothing to restore."
        return 0
    fi
    if [[ ! "$QUEST_POWER_ORIG" =~ ^[0-9]+$ ]]; then
        log_error "Cannot restore malformed Quest power value: '$QUEST_POWER_ORIG'"
        return 1
    fi

    adb_do shell settings put global stay_on_while_plugged_in "$QUEST_POWER_ORIG"
    local restored
    restored="$(adb_do shell settings get global stay_on_while_plugged_in | tr -d '\r\n ')"
    if [[ "$restored" != "$QUEST_POWER_ORIG" ]]; then
        log_error "Quest power restore verification failed: expected $QUEST_POWER_ORIG, got '$restored'"
        return 1
    fi
    printf 'RESTORED\nvalue=%s\n' "$restored" > "$VERIFICATION_DIR/device/quest_power_restore.status"
    log_info "Restored and verified Quest stay_on_while_plugged_in=$restored."
}

# ------------------------------------------------------------------
# Raw logcat process tracking. There is no pipeline: $! is the adb process.
# ------------------------------------------------------------------
start_logcat_capture() {
    # start_logcat_capture <raw_file> <filtered_file> <grep_regex>
    local raw_file="$1" filtered_file="$2" regex="$3"
    local process_dir="$VERIFICATION_DIR/device/logcat_process"
    mkdir -p "$process_dir" "$(dirname "$raw_file")" "$(dirname "$filtered_file")"
    if [[ -f "$process_dir/pid" ]]; then
        stop_logcat_processes_started_by_script
    fi

    "$ADB" "${ADB_TARGET[@]}" logcat -v threadtime -T 1 > "$raw_file" 2>&1 &
    local pid=$!
    sleep 1
    if ! kill -0 "$pid" 2>/dev/null; then
        wait "$pid" 2>/dev/null || true
        log_error "Raw logcat process failed to stay alive; see $raw_file"
        return 1
    fi

    local ps_start command
    ps_start="$(ps -p "$pid" -o lstart= | sed 's/^[[:space:]]*//')"
    command="$ADB -s ${ADB_TARGET[1]} logcat -v threadtime -T 1"
    printf '%s\n' "$pid" > "$process_dir/pid"
    printf '%s\n' "$command" > "$process_dir/command"
    printf '%s\n' "$ps_start" > "$process_dir/ps_start"
    printf '%s\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)" > "$process_dir/start_utc"
    printf '%s\n' "$raw_file" > "$process_dir/raw_file"
    printf '%s\n' "$filtered_file" > "$process_dir/filtered_file"
    printf '%s\n' "$regex" > "$process_dir/filter_regex"
    printf 'RUNNING\n' > "$process_dir/status"
    log_info "Started owned raw logcat PID=$pid -> $raw_file"
}

stop_logcat_processes_started_by_script() {
    local process_dir="$VERIFICATION_DIR/device/logcat_process"
    [[ -d "$process_dir" ]] || return 0
    local pid command stored_start current_start current_command stop_result
    pid="$(cat "$process_dir/pid" 2>/dev/null || true)"
    command="$(cat "$process_dir/command" 2>/dev/null || true)"
    stored_start="$(cat "$process_dir/ps_start" 2>/dev/null || true)"
    stop_result="already_stopped"

    if [[ "$pid" =~ ^[0-9]+$ ]] && kill -0 "$pid" 2>/dev/null; then
        current_start="$(ps -p "$pid" -o lstart= | sed 's/^[[:space:]]*//')"
        current_command="$(ps -p "$pid" -o command=)"
        if [[ "$current_start" != "$stored_start" ||
              "$current_command" != *"$ADB"* ||
              "$current_command" != *" -s ${ADB_TARGET[1]} "* ||
              "$current_command" != *" logcat"* ]]; then
            log_error "PID ownership check failed; refusing to stop PID $pid."
            printf 'ownership_check_failed\n' > "$process_dir/stop_result"
            return 1
        fi

        kill "$pid"
        local attempts=0
        while kill -0 "$pid" 2>/dev/null && [[ "$attempts" -lt 50 ]]; do
            sleep 0.1
            attempts=$((attempts + 1))
        done
        if kill -0 "$pid" 2>/dev/null; then
            kill -KILL "$pid"
            stop_result="killed_after_term_timeout"
        else
            stop_result="terminated"
        fi
        wait "$pid" 2>/dev/null || true
    fi

    local raw_file filtered_file regex
    raw_file="$(cat "$process_dir/raw_file" 2>/dev/null || true)"
    filtered_file="$(cat "$process_dir/filtered_file" 2>/dev/null || true)"
    regex="$(cat "$process_dir/filter_regex" 2>/dev/null || true)"
    if [[ -n "$raw_file" && -f "$raw_file" && -n "$filtered_file" && -n "$regex" ]]; then
        grep -E "$regex" "$raw_file" > "$filtered_file" 2>/dev/null || true
    fi
    printf '%s\n' "$stop_result" > "$process_dir/stop_result"
    printf '%s\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)" > "$process_dir/stop_utc"
    printf 'STOPPED\n' > "$process_dir/status"
    log_info "Owned logcat PID=$pid stop result: $stop_result; command: $command"
}

# ------------------------------------------------------------------
# APK discovery: accept only a newly produced, non-split arm64 APK.
# ------------------------------------------------------------------
find_run_arm64_apk() {
    local search_dir="$1" minimum_mtime="$2"
    if [[ ! -d "$search_dir" ]]; then
        log_error "APK search dir does not exist: $search_dir"
        return 1
    fi
    local apks=()
    while IFS= read -r -d '' f; do
        local name lower mtime
        name="$(basename "$f")"
        lower="$(printf '%s' "$name" | tr '[:upper:]' '[:lower:]')"
        mtime="$(file_mtime_epoch "$f")"
        [[ "$lower" == *-arm64.apk ]] || continue
        [[ "$lower" == *symbol* || "$lower" == *test* || "$lower" == *split* ||
           "$lower" == config.* || "$lower" == base-* ]] && continue
        [[ "$mtime" -ge "$minimum_mtime" ]] || continue
        apks+=("$f")
    done < <(find "$search_dir" -type f -name '*.apk' -print0 2>/dev/null)

    if [[ "${#apks[@]}" -eq 0 ]]; then
        log_error "No newly produced installable arm64 APK found under: $search_dir"
        return 1
    fi
    if [[ "${#apks[@]}" -gt 1 ]]; then
        log_error "Multiple new arm64 APK candidates found; refusing selection:"
        printf '  %s\n' "${apks[@]}" >&2
        return 1
    fi
    printf '%s\n' "${apks[0]}"
}

record_file_metadata() {
    local file="$1" output="$2"
    {
        printf 'path=%s\n' "$file"
        printf 'size_bytes=%s\n' "$(file_size_bytes "$file")"
        printf 'mtime_epoch=%s\n' "$(file_mtime_epoch "$file")"
        printf 'mtime=%s\n' "$(file_mtime_text "$file")"
        printf 'sha256=%s\n' "$(sha256_file "$file")"
        printf 'recorded_utc=%s\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)"
    } > "$output"
}

extract_research_session_from_log() {
    local raw_log="$1"
    grep -Eo '\[Research\]\[Logger\] OPEN ok \| session=[^ |]+' "$raw_log" \
        | tail -n 1 | sed 's/.*session=//'
}

validate_session_jsonl_path() {
    local session_id="$1" candidate="$2"
    if [[ ! "$session_id" =~ ^[A-Za-z0-9-]+$ ||
          "$(basename "$candidate")" != "$session_id.jsonl" ]]; then
        log_error "JSONL path does not belong to expected session '$session_id': $candidate"
        return 1
    fi
}

select_session_jsonl_remote_path() {
    local session_id="$1"
    if [[ ! "$session_id" =~ ^[A-Za-z0-9-]+$ ]]; then
        log_error "Malformed session id; refusing device search: '$session_id'"
        return 1
    fi
    local root="/sdcard/Android/data/$PACKAGE_NAME/files"
    local candidates
    candidates="$(adb_do shell find "$root" -type f -name "$session_id.jsonl" 2>/dev/null \
        | tr -d '\r' | sed '/^$/d' || true)"
    local count
    count="$(printf '%s\n' "$candidates" | sed '/^$/d' | wc -l | tr -d ' ')"
    if [[ "$count" != "1" ]]; then
        log_error "Expected exactly one JSONL for session $session_id under $root; found $count."
        [[ -n "$candidates" ]] && printf '%s\n' "$candidates" >&2
        return 1
    fi
    local selected
    selected="$(printf '%s\n' "$candidates")"
    validate_session_jsonl_path "$session_id" "$selected"
    printf '%s\n' "$selected"
}

log_info "phase1_verify_common.sh sourced; no run was created or resumed."
