#!/bin/sh
# pchq_board_action.sh <bv_session> <verb> [arg]
#
# The action= side of pchq-board.xhtpm. Ports run_pchq_board_mode()'s
# input forwarding: everything the toolbar / File / Desk rows do is
# appending the right byte(s) to the board-viewer session's own two
# history files (never reimplementing the engine's key handling).
#
#   interact         engage/toggle Interact Mode          -> 13
#   menu file|desk|close                                  -> state/menu.txt only
#   file <row>       pick File row (default-pdl / default-legacy)
#                    engage if needed (13) then '5' if row != active
#   desk <row>       reload the board: engage if needed then '6'
#
# The renderer's dispatch() appends '<pkg_dir>' '<house_root>' as two
# trailing args - ignored here.
set -u
BV="${1:-}"
VERB="${2:-}"
ARG="${3:-}"

SELF_DIR="$(cd "$(dirname "$0")" && pwd)"
PKG_STATE="$(cd "$SELF_DIR/.." && pwd)/state"
mkdir -p "$PKG_STATE"

# menu open/close is pure local state for the projector
if [ "$VERB" = "menu" ]; then
    case "$ARG" in
        file|desk) printf 'open=%s\n' "$ARG" > "$PKG_STATE/menu.txt" ;;
        *)         printf 'open=\n'          > "$PKG_STATE/menu.txt" ;;
    esac
    exit 0
fi

[ -n "$BV" ] && [ -d "$BV" ] || exit 0
H1="$BV/pieces/apps/player_app/history.txt"
H2="$BV/pieces/keyboard/history.txt"
TYPING="$BV/pieces/display/active_gui_is_typing.txt"

append_key() {
    printf '%s\n' "$1" >> "$H1"
    printf 'KEY_PRESSED: %s\n' "$1" >> "$H2"
}

interact_on() {
    [ -f "$TYPING" ] && [ "$(head -c 8 "$TYPING" 2>/dev/null | tr -dc 0-9)" != "" ] \
        && [ "$(head -c 8 "$TYPING" 2>/dev/null | tr -dc 0-9)" != "0" ]
}

case "$VERB" in
    interact)
        append_key 13
        ;;
    file)
        interact_on || append_key 13
        # active row: default-legacy = 1, else 0
        LVL_FILE="$(cd "$SELF_DIR/../.." >/dev/null 2>&1 && pwd)/@.apps/piececraft-hq/pieces/system/board_config.txt"
        CUR=0
        [ -f "$LVL_FILE" ] && grep -q 'active_level=default-legacy' "$LVL_FILE" && CUR=1
        [ "${ARG:-0}" != "$CUR" ] && append_key 53   # '5' - FILE_MENU cycle
        printf 'open=\n' > "$PKG_STATE/menu.txt"
        ;;
    desk)
        interact_on || append_key 13
        append_key 54                                 # '6' - DESK_MENU reload
        printf 'open=\n' > "$PKG_STATE/menu.txt"
        ;;
esac
exit 0
