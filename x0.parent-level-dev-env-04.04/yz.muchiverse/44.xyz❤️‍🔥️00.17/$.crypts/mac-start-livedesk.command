#!/bin/bash
# mac-start-livedesk.command — Mac livedesk start (mirror of win-start-livedesk.ps1).
# Double-clickable in Finder (.command); also runnable: sh mac-start-livedesk.command
#
# Does NOT call crypt_autostart.+x — its process scan reads /proc, which does
# not exist on macOS (same reason the Windows leg bypassed crypt_autostart.exe).
# Parses autostart.pdl LAUNCH rows and execs each binary directly.
# No '*.' -> '_.' aliasing here: APFS stores star-names natively (Windows-only
# problem); binaries keep their .+x names.
set -u
CRYPTS="$(cd "$(dirname "$0")" && pwd)"
HOUSE="$(cd "$CRYPTS/.." && pwd)"
PDL="$CRYPTS/autostart.pdl"
cd "$HOUSE" || exit 1

# Finder-spawned processes have no DISPLAY; XQuartz's launchd socket is the
# normal live value, plain :0 is the classic fallback.
if [ -z "${DISPLAY:-}" ]; then
    SOCKET=$(compgen -G "/private/tmp/com.apple.launchd.*/org.xquartz:0" 2>/dev/null | head -n 1)
    export DISPLAY="${SOCKET:-:0}"
fi

KILL_PAT="khtpm_strip_parser|khtpm_taskbar_manager_main|khtpm_hq_render|tp_desktop_window_rgb|tp_desktop_window"
pkill -f "$KILL_PAT" 2>/dev/null
sleep 0.4
# Stale pidfile from a previous session would make each entity's
# ensure_taskbar_running() see a dead pid, fail its /proc sweep (no /proc
# on macOS), and spawn its OWN strip — six duplicate taskbars live
# (found 2026-08-22). Clear it; the fresh tool-bar's manager rewrites it.
rm -f "$HOUSE/#.desktop/livedesk_taskbar.pid"
# Stale-CLOSE poison (found live 2026-08-22): button.sh quit relays a
# CLOSE line into every REGISTERED entity's <pal>/interact_relay.txt;
# if that entity was already dead (pkill'd first) or unregistered, the
# command just SITS there — and the next entity to bind that package
# reads it on its first main-loop poll and exits(0) with zero output
# ("silent death", one different victim per launch as poisons got
# consumed one at a time). crypt_autostart's kill-all-then-relaunch has
# the same fresh-start semantics on Linux; mirror it here by clearing
# the relays (truncate, keep the files — entities expect them to exist).
find "$HOUSE/xyzfs/users" -name interact_relay.txt -type f -exec sh -c ': > "$1"' _ {} \; 2>/dev/null

[ -f "$PDL" ] || { echo "MISSING $PDL"; exit 1; }

launched=0
LOGDIR="$HOUSE/tmp/livedesk-mac"
mkdir -p "$LOGDIR"
PIDFILE="$HOUSE/#.desktop/livedesk_taskbar.pid"
# "|| [ -n "$line" ]": autostart.pdl has no trailing newline (live
# 2026-08-22) — bare `read` silently drops a final unterminated line,
# which skipped the last entity (ava) entirely.
while IFS= read -r line || [ -n "$line" ]; do
    # CRLF tolerance — matches crypt_autostart.c's strcspn(line,"\r\n"):
    # autostart.pdl picks up Windows \r\n on win-trips and bash's read
    # keeps the \r (found live 2026-08-22: entities exited silently
    # because their pal-path arg ended in a carriage return).
    line="${line%$'\r'}"
    case "$line" in LAUNCH\ *|LAUNCH$'\t'*) ;; *) continue ;; esac
    rest="${line#*|}"
    name="$(printf '%s' "$rest" | cut -d'|' -f1 | tr -d '[:space:]')"
    val="${rest#*|}"
    case "$name|$val" in
        *strip_parser*|tool-bar|tool-bar$'\t'*)
            ;;
        *)
            # Entities wait for the tool-bar to publish its pidfile first:
            # crypt_autostart paces launches with the same intent, and on
            # Linux ensure_taskbar_running()'s /proc fallback covers any
            # race — macOS has no /proc, so the pidfile must be live
            # before the first entity starts or it spawns a duplicate.
            for i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 \
                     21 22 23 24 25 26 27 28 29 30; do
                [ -f "$PIDFILE" ] || { sleep 0.1; continue; }
                tpid=$(head -n 1 "$PIDFILE" | tr -dc '0-9')
                [ -n "$tpid" ] && kill -0 "$tpid" 2>/dev/null && break
                sleep 0.1
            done
            ;;
    esac
    # tokenize single-quoted fields: 'bin' 'arg1' 'arg2'
    eval "set -- $val"
    [ $# -lt 1 ] && continue
    bin_rel="$1"
    [ -x "$bin_rel" ] || { echo "SKIP (not built yet): $bin_rel"; continue; }
    shift
    # Detach with own log (mirrors Start-Process on Windows): a desktop
    # process never exits, and inheriting the caller's stdout would hang
    # any script that waits on this one.
    nohup "$bin_rel" "$@" >"$LOGDIR/$name.log" 2>&1 &
    sleep 0.2
    launched=$((launched+1))
done < "$PDL"

echo "launched $launched livedesk component(s) (DISPLAY=$DISPLAY)"
