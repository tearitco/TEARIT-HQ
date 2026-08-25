#!/bin/sh

# macOS leg (2026-08-23): macOS has no setsid(2) wrapper binary - expand
# to nothing there, keep real setsid on Linux. Unquoted $SETSID so the
# empty case vanishes from the command line entirely.
SETSID="setsid"
[ "$(uname)" = "Darwin" ] && SETSID=""
# run_khtpm_strip.sh — manual runner for the khtpm strip parser/manager
# pair (khtpm_strip_parser.c + khtpm_taskbar_manager_main.c), same spirit
# as $.crypts/button.sh but scoped to this one taskbar system.
#
# Encapsulates the exact safe kill/build/launch/verify cycle used by hand
# all through the 2026-08-11 build-out session: never trust a bare exit
# code for a backgrounded X11 GUI launch, always confirm real PIDs via
# pgrep.
#
# 2026-08-11: legacy tp_taskbar.c retired (archived to
# *.monads/*.livedesk-taskbar/ops/LEGACY-ARCHIVE-20260811.zip, originals
# deleted). Binaries dropped their "_test" suffix now that khtpm is the
# real, only taskbar. The `legacy`/`restore` action this script used to
# have (relaunch tp_taskbar.c as a fallback) is gone — there is no legacy
# binary left to relaunch.

set -u
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# Shared path vars: KHTPM_HOUSE, KHTPM_LOG, KHTPM_PID (single source of truth
# for the log/pid paths - see khtpm_vars.sh's own header comment).
. "$SCRIPT_DIR/khtpm_vars.sh"
HOUSE="$KHTPM_HOUSE"
PARSER="$SCRIPT_DIR/+x/khtpm_strip_parser.+x"
ACTION="${1:-help}"

khtpm_pids() { pgrep -f "khtpm_strip_parser\.\+x|khtpm_taskbar_manager_main\.\+x" 2>/dev/null; }

kill_khtpm() {
    pids="$(khtpm_pids)"
    if [ -n "$pids" ]; then
        echo "$pids" | xargs -r kill -TERM
        sleep 1
        echo "khtpm stopped (was PID(s): $(echo $pids | tr '\n' ' '))"
    else
        echo "khtpm was not running"
    fi
}

case "$ACTION" in
    new|test|run)
        # Always build fresh first — a stale binary was a real source of
        # confusion this session (old process still running old code
        # while a rebuilt binary sat unused on disk).
        sh "$SCRIPT_DIR/build_khtpm_strip.sh" || { echo "BUILD FAILED — not launching"; exit 1; }
        kill_khtpm
        rm -f "$KHTPM_LOG"
        # cd into HOUSE first — the parser's own children (the manager)
        # inherit this cwd, and relative menu commands (livedesk_taskbar.pdl)
        # depend on it being house root, not wherever this script was invoked from.
        (cd "$HOUSE" && $SETSID env DISPLAY="${DISPLAY:-:0}" "$PARSER" "$HOUSE" \
            > "$KHTPM_LOG" 2>&1 < /dev/null &)
        sleep 2
        pids="$(khtpm_pids)"
        if [ -n "$pids" ]; then
            echo "OK — khtpm running, PID(s): $(echo $pids | tr '\n' ' ')"
            echo "log: $KHTPM_LOG"
        else
            echo "FAILED to launch — check the log:"
            cat "$HOUSE/#.desktop/khtpm_strip_parser.log" 2>/dev/null
            exit 1
        fi
        ;;
    stop)
        kill_khtpm
        ;;
    status)
        pids="$(khtpm_pids)"
        if [ -n "$pids" ]; then
            echo "khtpm: RUNNING (PID(s): $(echo $pids | tr '\n' ' '))"
        else
            echo "khtpm: stopped"
        fi
        ;;
    build)
        sh "$SCRIPT_DIR/build_khtpm_strip.sh"
        ;;
    help|h|-h|--help|*)
        cat <<EOF
run_khtpm_strip.sh — manual runner for the khtpm strip parser/manager

  sh run_khtpm_strip.sh new       # build fresh, kill any running khtpm, launch
  sh run_khtpm_strip.sh stop      # kill khtpm
  sh run_khtpm_strip.sh status    # show whether khtpm is running
  sh run_khtpm_strip.sh build     # build_khtpm_strip.sh only, no process changes

Always ends with a real pgrep-confirmed PID, never a bare exit code.
EOF
        ;;
esac
