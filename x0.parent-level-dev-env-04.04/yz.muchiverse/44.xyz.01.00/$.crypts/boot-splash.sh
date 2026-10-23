#!/bin/sh
# boot-splash.sh - start the "Loading LyfeDesk" strip NOW, before anything else slow happens (owner 2026-10-23: "i should see loading bar
# immediately before anything else happens or it will feel slow/broken"). Every start path calls this as its FIRST step: the desktop
# launcher (livedesk-launch.sh) and button.sh run / start / restart / reset. Singleton: if a strip is already up it is left alone (a second
# start would restart the bar from zero).
#   boot-splash.sh [label]          start the strip (label = who called, written to the trace)
#   boot-splash.sh --trace <text>   append one timestamped line to #.desktop/boot_trace.txt and exit
# #.desktop/boot_trace.txt is the start-up evidence file of THIS script's callers (`<epoch_ms> <text>`); unlike boot_timeline.txt it is not
# reset when the new boot begins, so the time between the press and the first strip is visible in it. Capped at ~300 lines.
# Needs a display (DISPLAY falls back to :0, XAUTHORITY to ~/.Xauthority, as the launcher does); LIVEDESK_NO_SPLASH=1 disables the strip.
# Never fails the caller: always exits 0.
SELF="$(readlink -f "$0")"
CRYPTS="$(dirname "$SELF")"
HOUSE="$(dirname "$CRYPTS")"
DESK="$HOUSE/#.desktop"
OPS="$HOUSE/_.monads/_.livedesk-taskbar/ops"
TR="$DESK/boot_trace.txt"
trace() { echo "$(date +%s%3N) $*" >> "$TR" 2>/dev/null; }
if [ "${1:-}" = "--trace" ]; then shift; trace "$*"; exit 0; fi
LABEL="${1:-unknown}"
[ -d "$DESK" ] || exit 0
[ -z "${LIVEDESK_NO_SPLASH:-}" ] || exit 0
if [ -f "$TR" ] && [ "$(wc -l < "$TR" 2>/dev/null || echo 0)" -gt 300 ]; then tail -n 100 "$TR" > "$TR.tmp" 2>/dev/null && mv "$TR.tmp" "$TR" 2>/dev/null; fi
[ -n "${DISPLAY:-}" ] || export DISPLAY=:0
[ -n "${XAUTHORITY:-}" ] || { [ -r "$HOME/.Xauthority" ] && export XAUTHORITY="$HOME/.Xauthority"; }
SETSID="setsid"
[ "$(uname)" = "Darwin" ] && SETSID=""
BIN="$OPS/+x/livedesk_splash.+x"
PIDF="$DESK/livedesk_boot_splash.pid"
if [ ! -x "$BIN" ]; then
    trace "boot-splash($LABEL): no strip binary yet ($BIN); the build step compiles it"
    exit 0
fi
P="$(cat "$PIDF" 2>/dev/null)"
if [ -n "$P" ] && grep -q livedesk_splash "/proc/$P/comm" 2>/dev/null; then
    trace "boot-splash($LABEL): strip already running (pid $P), left alone"
    exit 0
fi
rm -f "$DESK/boot_build_failed.txt" "$DESK/dock_stack/draw_stamp.txt"
$SETSID nohup "$BIN" "$HOUSE" "$OPS/+x" --boot >/dev/null 2>&1 < /dev/null &
trace "boot-splash($LABEL): strip started (pid $!)"
exit 0
