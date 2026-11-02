#!/bin/sh
# livedesk-icon-refresh.sh - redraw the installed LyfeDesk app icon in the CURRENT theme colors (#.desktop/livedesk_theme.pdl).
# Called from every start path (livedesk-launch.sh, button.sh run/reset, run_khtpm_strip.sh boot/new) so the icon follows the last color
# settings after any (re)start (owner 2026-10-06). Silent, fast (~0.1 s), safe to call often: no installed icon (install-app not run) or
# no Pillow = no-op. The temp file is renamed over the icon and the .desktop files are touched so file managers re-read it.
HOUSE="$(cd "$(dirname "$(readlink -f "$0")")/.." && pwd)"
ICON="$HOME/.local/share/icons/hicolor/256x256/apps/lyfedesk.png"
# the launcher name follows the last login ("HQ:<user>"); independent of the icon, so before its early exits
sh "$HOUSE/\$.crypts/lyfedesk-shortcut-refresh.sh" "$HOUSE" >/dev/null 2>&1
[ -f "$ICON" ] || exit 0
# `debounce` (theme-change hooks; the opacity slider fires many times while dragging): wait 0.6 s and only the LAST caller redraws.
if [ "${1:-}" = "debounce" ]; then
    TOK="$HOUSE/#.desktop/.icon_refresh.token"
    echo "$$" > "$TOK"
    sleep 0.6
    [ "$(cat "$TOK" 2>/dev/null)" = "$$" ] || exit 0
fi
command -v python3 >/dev/null 2>&1 || exit 0
# Serialize redraws and give each its own temp file: two overlapping redraws used to share "$ICON.new", so the older one's rm
# could delete the newer one's output and the icon stayed on the previous pattern until the next click (owner 2026-11-02).
# Under the lock, a caller that is no longer the newest (token changed) leaves the job to the newer one.
(
    command -v flock >/dev/null 2>&1 && flock -x 9
    if [ "${1:-}" = "debounce" ] && [ "$(cat "$TOK" 2>/dev/null)" != "$$" ]; then exit 0; fi
    NEW="$ICON.$$.new"
    python3 "$HOUSE/\$.crypts/livedesk-icon-gen.py" "$HOUSE" "$NEW" 256 >/dev/null 2>&1 && mv -f "$NEW" "$ICON"
    rm -f "$NEW"
) 9>"$HOUSE/#.desktop/.icon_refresh.lock"
touch "$HOME/.local/share/applications/lyfedesk.desktop" "$HOME/Desktop/LyfeDesk.desktop" 2>/dev/null
exit 0
