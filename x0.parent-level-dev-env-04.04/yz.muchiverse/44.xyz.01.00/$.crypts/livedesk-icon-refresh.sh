#!/bin/sh
# livedesk-icon-refresh.sh - redraw the installed Livedesk app icon in the CURRENT theme colors (#.desktop/livedesk_theme.pdl).
# Called from every start path (livedesk-launch.sh, button.sh run/reset, run_khtpm_strip.sh boot/new) so the icon follows the last color
# settings after any (re)start (owner 2026-10-06). Silent, fast (~0.1 s), safe to call often: no installed icon (install-app not run) or
# no Pillow = no-op. The temp file is renamed over the icon and the .desktop files are touched so file managers re-read it.
HOUSE="$(cd "$(dirname "$(readlink -f "$0")")/.." && pwd)"
ICON="$HOME/.local/share/icons/hicolor/256x256/apps/livedesk.png"
[ -f "$ICON" ] || exit 0
# `debounce` (theme-change hooks; the opacity slider fires many times while dragging): wait 0.6 s and only the LAST caller redraws.
if [ "${1:-}" = "debounce" ]; then
    TOK="$HOUSE/#.desktop/.icon_refresh.token"
    echo "$$" > "$TOK"
    sleep 0.6
    [ "$(cat "$TOK" 2>/dev/null)" = "$$" ] || exit 0
fi
command -v python3 >/dev/null 2>&1 || exit 0
python3 "$HOUSE/\$.crypts/livedesk-icon-gen.py" "$HOUSE" "$ICON.new" 256 >/dev/null 2>&1 && mv -f "$ICON.new" "$ICON"
rm -f "$ICON.new"
touch "$HOME/.local/share/applications/livedesk.desktop" "$HOME/Desktop/Livedesk.desktop" 2>/dev/null
exit 0
