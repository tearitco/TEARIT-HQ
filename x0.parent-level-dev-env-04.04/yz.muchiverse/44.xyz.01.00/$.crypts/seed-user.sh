#!/bin/bash
# seed-user.sh - give the CURRENT desk user the starter files they need on a fresh clone (Q007 step 6, USER-DATA-BRANCHES-DESIGN.md 3c).
#
# xyzfs/users is not tracked any more, so a fresh clone has no user folder. xyzfs/_seed/ (tracked) holds the starter content; this copies it into
# xyzfs/users/<current_user_uuid>/ (uuid from the tracked login file current_login.txt). Rules, all deliberate:
#   - NEVER overwrites: a file that already exists is left alone (copy-if-missing). Safe to run any time; a no-op on a real desk.
#   - Identity and runtime are never seeded (no pal.pdl / entity_uid.txt / histories / phones): those are created per install by the manager, so two installs
#     can never share an entity uid or phone number.
#   - Only the current user. Other users are created by the login flow as before.
# Usage: seed-user.sh [-q]      (run by `button.sh build`; `button.sh seed-user` by hand)
set -u
QUIET=0; [ "${1:-}" = "-q" ] && QUIET=1
HOUSE="$(cd "$(dirname "$0")/.." && pwd)"
SEED="$HOUSE/xyzfs/_seed"
[ -d "$SEED" ] || exit 0
LOGIN="$(find "$HOUSE" -maxdepth 3 -name current_login.txt -path '*00.login-signup*' 2>/dev/null | head -1)"
[ -n "$LOGIN" ] || { [ "$QUIET" = 1 ] || echo "seed-user: no current_login.txt found, nothing to do"; exit 0; }
UUID="$(sed -n 's/^current_user_uuid=//p' "$LOGIN" | head -1 | tr -d '\r')"
case "$UUID" in ""|*/*|*..*) [ "$QUIET" = 1 ] || echo "seed-user: no usable current_user_uuid in $LOGIN"; exit 0 ;; esac
DEST="$HOUSE/xyzfs/users/$UUID/home"
n=0
while IFS= read -r -d '' f; do
    rel="${f#$SEED/}"
    [ -e "$DEST/$rel" ] && continue
    mkdir -p "$DEST/$(dirname "$rel")" && cp -p "$f" "$DEST/$rel" && n=$((n+1))
done < <(find "$SEED" -type f -print0)
[ "$QUIET" = 1 ] && [ "$n" = 0 ] && exit 0
echo "seed-user: user ${UUID:0:8}: $n starter files copied (existing files untouched)"
