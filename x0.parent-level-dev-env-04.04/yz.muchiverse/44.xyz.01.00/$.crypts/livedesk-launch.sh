#!/bin/sh
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
# Every press: re-read the CURRENT theme colors (#.desktop/livedesk_theme.pdl) and redraw the app icon in them, so the icon always
# follows the last color settings (owner 2026-10-06). Done first and cheaply (~0.1 s); a missing Pillow keeps the previous icon.
# The icon you see after this press has this press's colors; the .desktop file is touched so file managers re-read it.
_ICON="$HOME/.local/share/icons/hicolor/256x256/apps/livedesk.png"
if [ -f "$_ICON" ] && command -v python3 >/dev/null 2>&1; then
    python3 "$CRYPTS/livedesk-icon-gen.py" "$HOUSE" "$_ICON.new" 256 >/dev/null 2>&1 && mv -f "$_ICON.new" "$_ICON"
    rm -f "$_ICON.new"
    touch "$HOME/.local/share/applications/livedesk.desktop" "$HOME/Desktop/Livedesk.desktop" 2>/dev/null
fi
{
    echo "== $(date '+%F %T') livedesk-launch (pid $$) DISPLAY=$DISPLAY"
    # same cheap hash-gated build step the old button ran (a no-op takes well under a second)
    ( cd "$HOUSE/_.monads/_.livedesk-taskbar/ops" && LIVEDESK_START_SPLASH=1 bash build_khtpm_strip.sh >/dev/null 2>&1 )
    echo "build step done $(date +%T)"
} >> "$LOG" 2>&1
setsid nohup sh "$CRYPTS/button.sh" run >> "$LOG" 2>&1 < /dev/null &
exit 0
