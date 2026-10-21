#!/bin/sh
# lyfedesk-shortcut-refresh.sh [house] - make the desktop shortcut and the app-grid entry read "HQ:<user>" (owner 2026-10-15), where <user> is the
# user of the LAST LOGIN (current_login.txt `current_user_id=`; "guest" when logged out). Called after every login and logout
# (userpal_login.c / userpal_logout.c), on every start (livedesk-icon-refresh.sh) and by `button.sh install-app`, so the name follows the user.
# Safe by construction:
#  - only a launcher whose Exec line points into THIS house is touched (a test house, or a launcher installed for another checkout, is left alone);
#  - the file is rewritten IN PLACE (same inode), so the desktop's "Allow Launching" trust flag survives; nothing is rewritten when the name is already right;
#  - the user id is reduced to letters, digits and . _ - before it goes into the file.
# Test overrides: LYFEDESK_APPS_DIR (default ~/.local/share/applications), LYFEDESK_DESKTOP_DIR (default ~/Desktop).
HOUSE="${1:-$(cd "$(dirname "$0")/.." && pwd)}"
APPS="${LYFEDESK_APPS_DIR:-$HOME/.local/share/applications}"
DESK="${LYFEDESK_DESKTOP_DIR:-$HOME/Desktop}"
LOGIN="$(find "$HOUSE" -maxdepth 3 -name current_login.txt -path '*00.login-signup*' 2>/dev/null | head -1)"
U=""
[ -n "$LOGIN" ] && U="$(sed -n 's/^current_user_id=//p' "$LOGIN" | head -1 | tr -d '\r' | tr -cd 'A-Za-z0-9._-')"
[ -n "$U" ] || U=guest
NAME="HQ:$U"
changed=0
for f in "$APPS/lyfedesk.desktop" "$DESK/LyfeDesk.desktop"; do
    [ -f "$f" ] || continue
    grep -qF "$HOUSE/" "$f" || continue          # not this house's launcher
    grep -qx "Name=$NAME" "$f" && continue       # already right
    T="$(mktemp 2>/dev/null)" || continue
    if sed "s|^Name=.*|Name=$NAME|" "$f" > "$T" && [ -s "$T" ]; then cat "$T" > "$f"; changed=1; fi
    rm -f "$T"
done
[ "$changed" = 1 ] && command -v update-desktop-database >/dev/null 2>&1 && update-desktop-database "$APPS" 2>/dev/null
exit 0
