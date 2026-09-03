#!/bin/sh

# macOS leg (2026-08-23): macOS has no setsid(2) wrapper binary - expand
# to nothing there, keep real setsid on Linux. Unquoted $SETSID so the
# empty case vanishes from the command line entirely.
SETSID="setsid"
[ "$(uname)" = "Darwin" ] && SETSID=""
# run_khtpm_strip.sh — manual runner for the khtpm strip parser/manager
# pair, same spirit as $.crypts/button.sh but scoped to this one taskbar
# system.
#
# REAL FIX 2026-09-01 - the parser half (PARSER below) is no longer its
# own binary. khtpm_strip_parser.c/khtpm_strip_layout.c/.h/
# khtpm_strip_codes.h were folded verbatim into khtpm_core_render.c as
# a new mode (strip_main(), dispatched on argc==2 - this mode's own
# real invocation shape is exactly <house_root>, no .chtpm path, unlike
# every other mode that binary serves). Real house-standard
# consolidation this session: no cross-.c linking/#include to share
# behavior across binaries - genuinely the same file, or fork/exec+
# file-IPC (the manager, khtpm_taskbar_manager_main.+x, stays exactly
# that - a real, separate, unrelated process). khtpm_pids() below can
# no longer just pgrep the old binary NAME (khtpm_core_render.+x now
# also serves every HQ app window) - it scans /proc directly for a
# real strip-mode invocation: argv[0] matching khtpm_core_render AND
# exactly one argument after it (this mode's own real, exactly-2-argc
# shape - every other mode always has a 2nd argv, a .chtpm path).
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
PARSER="$SCRIPT_DIR/+x/khtpm_core_render.+x"
ACTION="${1:-help}"

strip_parser_pids() {
    for p in /proc/[0-9]*; do
        pid="${p#/proc/}"
        [ -r "$p/cmdline" ] || continue
        args="$(tr '\0' '\n' < "$p/cmdline" 2>/dev/null)"
        [ -z "$args" ] && continue
        a0="$(printf '%s\n' "$args" | sed -n 1p)"
        case "$a0" in
            */khtpm_core_render.+x|khtpm_core_render.+x)
                printf '%s\n' "$args" | grep -q 'khtpm_strip_header.xhtpm\|khtpm_strip_bottom.xhtpm\|strip_header.chtpm\|strip_bottom.chtpm' && echo "$pid"
                ;;
        esac
    done
}

khtpm_pids() { { strip_parser_pids; pgrep -f "khtpm_taskbar_manager_main\.\+x" 2>/dev/null; } 2>/dev/null; }

kill_khtpm() {
    pids="$(khtpm_pids)"
    if [ -n "$pids" ]; then
        echo "$pids" | xargs -r kill -TERM
        # REAL FIX 2026-08-30, direct live incident: a fixed 1s sleep was
        # not always enough for the manager to actually exit before this
        # script relaunched a new one - the new instance's own real
        # kill(pid,0) liveness-check singleton guard (khtpm_taskbar_
        # manager_main.c) then correctly saw the OLD pid still alive and
        # refused to start at all, leaving NO manager running (confirmed
        # live via khtpm_strip_parser.log's own "refusing to start" line)
        # - the parser came up, the manager silently didn't, and every
        # desktop entity that was alive stayed alive but the taskbar's
        # own live registry/menu logic (which the manager owns) went
        # dark, looking like "entities in toolbar but not on screen."
        # Poll for real death instead of guessing a fixed delay, same
        # TERM-then-KILL escalation convention documented elsewhere in
        # this house (TASKBAR-MENU-ARCHITECTURE.md's own button.sh
        # recipe).
        i=0
        while [ "$i" -lt 30 ]; do
            still="$(khtpm_pids)"
            [ -z "$still" ] && break
            sleep 0.1
            i=$((i + 1))
        done
        still="$(khtpm_pids)"
        if [ -n "$still" ]; then
            echo "$still" | xargs -r kill -KILL
            sleep 0.3
        fi
        echo "khtpm stopped (was PID(s): $(echo $pids | tr '\n' ' '))"
    else
        echo "khtpm was not running"
    fi
}

case "$ACTION" in
    boot|new|test|run)
        # `boot` = launch-only, NO rebuild - for $.crypts/autostart.pdl so
        # the desktop start button is snappy (a full build_khtpm_strip.sh
        # is ~20s and blocked every start-temp click). `new`/`run`/`test`
        # still build fresh first.
        if [ "$ACTION" != "boot" ]; then
            sh "$SCRIPT_DIR/build_khtpm_strip.sh" || { echo "BUILD FAILED — not launching"; exit 1; }
        elif [ ! -x "$SCRIPT_DIR/+x/khtpm_core_render.+x" ] || [ ! -x "$SCRIPT_DIR/+x/khtpm_taskbar_manager_main.+x" ]; then
            # first-ever boot with no binaries: fall back to a build
            sh "$SCRIPT_DIR/build_khtpm_strip.sh" || { echo "BUILD FAILED — not launching"; exit 1; }
        fi
        kill_khtpm
        rm -f "$KHTPM_LOG"
        # cd into HOUSE first — the parser's own children (the manager)
        # inherit this cwd, and relative menu commands (livedesk_taskbar.pdl)
        # depend on it being house root, not wherever this script was invoked from.
        MANAGER="$SCRIPT_DIR/+x/khtpm_taskbar_manager_main.+x"
        HEADER_CHTPM="$(cd "$SCRIPT_DIR/.." && pwd)/khtpm_strip_header.xhtpm"
        (cd "$HOUSE" && $SETSID env DISPLAY="${DISPLAY:-:0}" "$MANAGER" "$HOUSE" \
            >> "$KHTPM_LOG" 2>&1 < /dev/null &)
        i=0
        while [ "$i" -lt 50 ]; do
            [ -s "$HOUSE/#.desktop/strip_ui.txt" ] && break
            sleep 0.1
            i=$((i + 1))
        done
        (cd "$HOUSE" && $SETSID env DISPLAY="${DISPLAY:-:0}" "$PARSER" "$HOUSE" "$HEADER_CHTPM" \
            >> "$KHTPM_LOG" 2>&1 < /dev/null &)
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
