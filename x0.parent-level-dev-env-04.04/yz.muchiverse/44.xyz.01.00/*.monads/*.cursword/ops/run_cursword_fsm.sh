#!/bin/sh
# run_cursword_fsm.sh — build + launch cursword's onboarding FSM against
# a running livedesk. House root defaults to the repo house; override
# with HOUSE=... or arg 1.
#
#   sh run_cursword_fsm.sh [house_root] [--auto]
#
# The FSM exits immediately if someone is already signed in. Otherwise
# it opens the USER menu, arms "New User...", and narrates while the
# human types (or types throwaway creds itself with --auto).
set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

HOUSE="${HOUSE:-}"
AUTO=""
for a in "$@"; do
    case "$a" in
        --auto) AUTO="--auto" ;;
        *) HOUSE="$a" ;;
    esac
done
[ -n "$HOUSE" ] || HOUSE="$(cd "$SCRIPT_DIR/../../.." && pwd)"

sh "$SCRIPT_DIR/build_cursword_fsm.sh"

PKG="$SCRIPT_DIR/../entities/cursword"
: > "$PKG/say_log.txt" 2>/dev/null || true
echo "house: $HOUSE"
echo "watch: tail -f \"$PKG/say_log.txt\"   (narration)  /  \"$PKG/cursword_fsm.state\""
exec "$SCRIPT_DIR/+x/cursword_fsm.+x" "$HOUSE" $AUTO
