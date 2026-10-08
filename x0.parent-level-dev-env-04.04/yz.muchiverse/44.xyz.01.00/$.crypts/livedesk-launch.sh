#!/bin/sh
# macOS has no setsid binary (same convention as run_khtpm_strip.sh): expand to nothing there, keep real setsid on Linux.
SETSID="setsid"
[ "$(uname)" = "Darwin" ] && SETSID=""
# livedesk-launch.sh - the app launcher behind the "Livedesk" desktop/Linux-apps entry (made by `sh button.sh install-app`).
#
# Owner 2026-10-06: the old start-temp button is a bare ELF (no icon, no app entry). This is the same start - build step, then
# `button.sh run` - as a normal XDG app: Terminal=false, a PNG icon, and it can sit in the Linux app grid and on the desktop.
# House root is found from THIS file's own location (no absolute path is stored anywhere).
#
# It only starts livedesk (quit-current + autostart.pdl, same as the old button). Environment is made GUI-safe: DISPLAY and
# XAUTHORITY fall back to the session's when the launcher (file manager / app grid) did not pass them. The run is detached
# with setsid so the entities do not die with this process; a log is kept at #.desktop/livedesk_launch.log.
SELF="$(readlink -f "$0")"
CRYPTS="$(dirname "$SELF")"
HOUSE="$(dirname "$CRYPTS")"
[ -d "$HOUSE/#.desktop" ] || { echo "livedesk-launch: house root not found from $SELF" >&2; exit 1; }
[ -n "${DISPLAY:-}" ] || export DISPLAY=:0
[ -n "${XAUTHORITY:-}" ] || { [ -r "$HOME/.Xauthority" ] && export XAUTHORITY="$HOME/.Xauthority"; }
cd "$HOUSE" || exit 1
LOG="$HOUSE/#.desktop/livedesk_launch.log"
# Every press: redraw the app icon in the CURRENT theme colors (see livedesk-icon-refresh.sh; a no-op without an installed icon / Pillow).
sh "$CRYPTS/livedesk-icon-refresh.sh" </dev/null
OPS="$HOUSE/_.monads/_.livedesk-taskbar/ops"
# The loading strip starts FIRST (before any compile) so a compile is shown on it too; the build step is pinned to it (marker + rewritten
# binaries), then it follows the manager and the bottom bar's own redraws to "Ready". (Owner 2026-10-06: the loading animation was
# pinned to the wrong thing - it only began after the build, so a compile showed nothing at the bottom.) Binary rebuilt if missing/stale.
if [ ! -x "$OPS/+x/livedesk_splash.+x" ] || [ "$OPS/livedesk_splash.c" -nt "$OPS/+x/livedesk_splash.+x" ]; then
    _sx="$(pkg-config --cflags --libs x11 xft 2>/dev/null)"; [ -n "$_sx" ] || _sx="-I/usr/include/freetype2 -lX11 -lXft"
    mkdir -p "$OPS/+x"; ${CC:-gcc} -std=c11 -O2 -o "$OPS/+x/livedesk_splash.+x" "$OPS/livedesk_splash.c" $_sx >/dev/null 2>&1 || true
fi
rm -f "$HOUSE/#.desktop/boot_build_failed.txt" "$HOUSE/#.desktop/dock_stack/draw_stamp.txt"
[ -x "$OPS/+x/livedesk_splash.+x" ] && $SETSID nohup "$OPS/+x/livedesk_splash.+x" "$HOUSE" "$OPS/+x" --boot >/dev/null 2>&1 < /dev/null &
{
    echo "== $(date '+%F %T') livedesk-launch (pid $$) DISPLAY=$DISPLAY"
    # the same hash-gated build the old button ran (no-op < 1 s; a stale gate compiles the core renderer ~20 s). No centered
    # "Building livedesk" window: the bottom strip above shows it.
    ( cd "$OPS" && bash build_khtpm_strip.sh >/dev/null 2>&1 )
    echo "build step done $(date +%T)"
} >> "$LOG" 2>&1
if [ -f "$OPS/+x/.build_failed.txt" ]; then
    echo "BUILD FAILED (marker still present) - not starting livedesk; see $OPS/+x/build_error.log" >> "$LOG"
    : > "$HOUSE/#.desktop/boot_build_failed.txt"
    exit 1
fi
$SETSID nohup sh "$CRYPTS/button.sh" run >> "$LOG" 2>&1 < /dev/null &
exit 0
