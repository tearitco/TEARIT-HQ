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
#                    engage if needed, '5' if row != active, then RESTORE
#   desk <row>       reload the board: engage if needed, '6', then RESTORE
#
# RESTORE (2026-09-07 fix): file/desk used to engage Interact Mode and
# never turn it back off - if you clicked File/Desk while NOT already in
# Interact Mode you were left trapped in it (the renderer forwards every
# key, incl. arrows, to the game while it's on, so khtpm nav appears
# frozen/stuck on the File item). Now: only auto-engage if it was off,
# and afterward toggle it back off iff this call turned it on and it is
# still on (live re-checked, so an engine-side exit is never
# double-toggled). Board is left in a nav-usable state either way.
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

HAVE_BV=0
if [ -n "$BV" ] && [ -d "$BV" ]; then
    HAVE_BV=1
    H1="$BV/pieces/apps/player_app/history.txt"
    H2="$BV/pieces/keyboard/history.txt"
    TYPING="$BV/pieces/display/active_gui_is_typing.txt"
fi
# file-hq / load-map / open-events do not need a live board-viewer session
if [ "$HAVE_BV" != 1 ]; then
    case "$VERB" in
        file-hq|load-map|open-events) ;;
        *) exit 0 ;;
    esac
    H1="/dev/null"; H2="/dev/null"; TYPING="/dev/null"
fi

append_key() {
    printf '%s\n' "$1" >> "$H1"
    printf 'KEY_PRESSED: %s\n' "$1" >> "$H2"
}

interact_on() {
    [ -f "$TYPING" ] && [ "$(head -c 8 "$TYPING" 2>/dev/null | tr -dc 0-9)" != "" ] \
        && [ "$(head -c 8 "$TYPING" 2>/dev/null | tr -dc 0-9)" != "0" ]
}

ENGAGED=0
# Turn Interact Mode ON only if it is currently OFF. Sets ENGAGED=1 when
# THIS call did the engaging (so restore_interact knows to undo it).
engage_if_needed() {
    if interact_on; then
        ENGAGED=0
    else
        append_key 13
        ENGAGED=1
    fi
}
# Undo an engage this call made. (1) wait for our own '13' to register
# (so we don't "restore" before the engine even turned it on), (2)
# toggle OFF, (3) VERIFY it went off and retry once. The verify step
# matters: under heavy CPU load the board-viewer engine can be seconds
# behind, and the old 0.6s cap timed out -> no restore -> Interact Mode
# left ON -> khtpm nav frozen on the File item ("stuck on 3"). Waits
# are ~2s each now, and the outcome is checked, not assumed.
restore_interact() {
    [ "$ENGAGED" = 1 ] || return 0
    i=0
    while [ "$i" -lt 40 ] && ! interact_on; do sleep 0.05; i=$((i + 1)); done
    interact_on || return 0            # engine never engaged / already exited - nothing to undo
    append_key 13                      # toggle back off
    i=0
    while [ "$i" -lt 40 ] && interact_on; do sleep 0.05; i=$((i + 1)); done
    interact_on && append_key 13       # toggle was missed under load - one retry
    return 0
}

case "$VERB" in
    interact)
        append_key 13
        ;;
    file-hq)
        HOUSE="$(cd "$SELF_DIR/../../.." && pwd)"
        setsid sh "$HOUSE/&.widgits/file-explorer/button.sh" run >/tmp/pchq-file-hq.log 2>&1 < /dev/null &
        printf 'open=\n' > "$PKG_STATE/menu.txt"
        ;;
    load-map)
        HOUSE="$(cd "$SELF_DIR/../../.." && pwd)"
        PCHQ="$(cd "$SELF_DIR/.." && pwd)"
        SRC="$PCHQ/pieces/system/maps/${ARG}/map.txt"
        DST="$PCHQ/pieces/system/chunks/chunk_0_0/chunk_0_0_z0.txt"
        if [ -f "$SRC" ]; then
            cp "$SRC" "$DST"
            mkdir -p "$PCHQ/pieces/system"
            {
                echo "active_level=${ARG}"
                echo "active_board=${ARG}"
            } > "$PCHQ/pieces/system/board_config.txt"
        fi
        engage_if_needed
        append_key 54
        sleep 0.15
        restore_interact
        printf 'open=\n' > "$PKG_STATE/menu.txt"
        ;;
    open-events)
        HOUSE="$(cd "$SELF_DIR/../../.." && pwd)"
        IR="$HOUSE/@.apps/piececraft-hq/pieces/system/maps/${ARG}/events.pdl"
        [ -f "$IR" ] || IR="$HOUSE/#.ref/menu/event-guides/examples/${ARG}/pages/page_1/event.ir.pdl"
        if [ -f "$IR" ]; then
            setsid sh "$HOUSE/&.widgits/events-hq/button.sh" "$HOUSE/@.apps/piececraft-hq" "$HOUSE" >/tmp/pchq-events-hq.log 2>&1 < /dev/null &
        fi
        printf 'open=\n' > "$PKG_STATE/menu.txt"
        ;;
    file)
        engage_if_needed
        # active row: default-legacy = 1, else 0
        LVL_FILE="$(cd "$SELF_DIR/../.." >/dev/null 2>&1 && pwd)/@.apps/piececraft-hq/pieces/system/board_config.txt"
        CUR=0
        [ -f "$LVL_FILE" ] && grep -q 'active_level=default-legacy' "$LVL_FILE" && CUR=1
        [ "${ARG:-0}" != "$CUR" ] && { append_key 53; sleep 0.15; }   # '5' - FILE_MENU cycle
        restore_interact
        printf 'open=\n' > "$PKG_STATE/menu.txt"
        ;;
    desk)
        engage_if_needed
        append_key 54                                 # '6' - DESK_MENU reload
        sleep 0.15
        restore_interact
        printf 'open=\n' > "$PKG_STATE/menu.txt"
        ;;
esac
exit 0
