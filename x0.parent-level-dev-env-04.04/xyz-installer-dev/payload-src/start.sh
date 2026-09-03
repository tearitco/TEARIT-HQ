#!/bin/sh
# start.sh — per-install launcher for the tearit-hq minimal desktop.
#
# Scoped to THIS install root only (matches parser cmdline + argv[1] ==
# this root). Deliberately NOT run_khtpm_strip.sh's global `pgrep -f
# khtpm_core_render` kill — that would sweep other installs / the dev
# tree sharing the same X display (design doc 04 §7 / §10.5).
#
#   sh start.sh            # launch (default)
#   sh start.sh stop
#   sh start.sh restart
#   sh start.sh status
#   sh start.sh uninstall  # stop, remove the launcher, delete this dir
#   sh start.sh dir        # open this install folder in a file manager

set -u
ROOT="$(cd "$(dirname "$0")" && pwd)"
PARSER="$ROOT/*.monads/*.livedesk-taskbar/ops/+x/khtpm_core_render.+x"
LOG="$ROOT/#.desktop/tearit-hq.log"
ACTION="${1:-start}"
SETSID="setsid"; [ "$(uname)" = "Darwin" ] && SETSID=""

# PIDs of a strip-mode parser (argv0 ~ khtpm_core_render.+x, argv1 == ROOT)
our_pids() {
    for p in /proc/[0-9]*; do
        [ -r "$p/cmdline" ] || continue
        a0=$(tr '\0' '\n' < "$p/cmdline" 2>/dev/null | sed -n 1p)
        a1=$(tr '\0' '\n' < "$p/cmdline" 2>/dev/null | sed -n 2p)
        a2=$(tr '\0' '\n' < "$p/cmdline" 2>/dev/null | sed -n 3p)
        case "$a0" in */khtpm_core_render.+x|khtpm_core_render.+x) ;; *) continue ;; esac
        [ -n "$a1" ] && [ -z "$a2" ] && [ "$a1" = "$ROOT" ] && echo "${p#/proc/}"
    done
}

stop_ours() {
    pids="$(our_pids)"
    if [ -z "$pids" ]; then echo "tearit-hq: not running"; return; fi
    echo "$pids" | xargs -r kill -TERM
    i=0
    while [ "$i" -lt 30 ]; do
        [ -z "$(our_pids)" ] && break
        sleep 0.1; i=$((i + 1))
    done
    [ -n "$(our_pids)" ] && echo "$(our_pids)" | xargs -r kill -KILL
    echo "tearit-hq: stopped ($pids)"
}

case "$ACTION" in
    start)
        if [ -n "$(our_pids)" ]; then
            echo "tearit-hq: already running (PID(s): $(our_pids | tr '\n' ' '))"
            exit 0
        fi
        [ -x "$PARSER" ] || { echo "FATAL: $PARSER not built — run: sh bootstrap.sh"; exit 1; }
        : > "$LOG"
        ( cd "$ROOT" && $SETSID env DISPLAY="${DISPLAY:-:0}" "$PARSER" "$ROOT" \
            > "$LOG" 2>&1 < /dev/null & )
        sleep 2
        if [ -n "$(our_pids)" ]; then
            echo "tearit-hq: running (PID(s): $(our_pids | tr '\n' ' '))"
            echo "log: $LOG"
        else
            echo "tearit-hq: FAILED to launch — log:"; cat "$LOG" 2>/dev/null; exit 1
        fi
        ;;
    stop)    stop_ours ;;
    restart) stop_ours; sleep 0.3; sh "$0" start ;;
    status)
        p="$(our_pids)"
        [ -n "$p" ] && echo "tearit-hq: RUNNING ($(echo $p | tr '\n' ' '))" || echo "tearit-hq: stopped"
        ;;
    dir|open)
        if command -v xdg-open >/dev/null 2>&1; then xdg-open "$ROOT" >/dev/null 2>&1 &
        elif command -v open >/dev/null 2>&1; then open "$ROOT" &
        else echo "$ROOT"; fi
        ;;
    uninstall|remove)
        # Remove everything this install put on the machine: the running
        # desktop, any ~/.local/bin (etc.) launcher pointing here, and
        # this directory. Ask first unless `-y` / YES=1.
        stop_ours
        removed_launcher=0
        for d in "$HOME/.local/bin" "$HOME/bin" "/usr/local/bin"; do
            [ -d "$d" ] || continue
            for f in "$d"/*; do
                [ -f "$f" ] || continue
                if grep -qF "$ROOT/start.sh" "$f" 2>/dev/null; then
                    rm -f "$f" && { echo "removed launcher: $f"; removed_launcher=1; }
                fi
            done
        done
        [ "$removed_launcher" = 0 ] && echo "no launcher found pointing here (ok)"
        if [ "${2:-}" != "-y" ] && [ "${YES:-0}" != "1" ]; then
            printf 'delete the whole install dir?  %s  [y/N] ' "$ROOT"
            read ans
            case "$ans" in y|Y|yes|YES) ;; *) echo "kept $ROOT (launcher already removed)"; exit 0 ;; esac
        fi
        cd /
        rm -rf "$ROOT"
        echo "uninstalled: $ROOT removed"
        ;;
    *) echo "usage: sh start.sh {start|stop|restart|status|dir|uninstall [-y]}"; exit 1 ;;
esac
