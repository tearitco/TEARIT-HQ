#!/bin/sh
# khtpm_vars.sh - single source of truth for khtpm taskbar shell paths.
# SOURCED (not executed) by the shell runners: run_khtpm_strip.sh (in ops/)
# and button.sh (in $.crypts/). Fixes the old drift where the log path was
# hardcoded in two scripts and the pid file path in C plus crypt_autostart.
#
# House-root discovery walks UP from this file's own dir until it finds a
# directory containing both #.desktop/ and &.widgits/ - works no matter
# which script sources us (their $0 differs, so we must not rely on it).
#
# NOTE: the C side (khtpm_taskbar_manager.c, crypt_autostart.c) already
# derives the SAME paths from house_root at runtime - these vars exist so the
# shell runners and the C binaries agree without editing C.

khtpm_find_house() {
    _d="$1"
    while [ -n "$_d" ] && [ "$_d" != "/" ]; do
        if [ -d "$_d/#.desktop" ] && [ -d "$_d/&.widgits" ]; then
            echo "$_d"
            return 0
        fi
        _d="$(dirname "$_d")"
    done
    return 1
}

KHTPM_VARS_DIR="$(cd "$(dirname "$0")" && pwd)"
KHTPM_HOUSE="$(khtpm_find_house "$KHTPM_VARS_DIR")"
KHTPM_LOG="$KHTPM_HOUSE/#.desktop/khtpm_strip_parser.log"
KHTPM_PID="$KHTPM_HOUSE/#.desktop/livedesk_taskbar.pid"
KHTPM_STATE="$KHTPM_HOUSE/#.desktop"
