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
        # REAL FIX 2026-09-15 (direct live report: "pc-hq opens files
        # which currently suggest 2 different files that don't actually
        # load 'maps+logic', that will need to be fixed" - confirmed by
        # direct code read: the OLD version of this verb copied the
        # picked map.txt to chunk_0_0_z0.txt and wrote board_config.txt,
        # then injected key 54 - but pc_menu_input.c has NO handler for
        # key 54 at all, and nothing anywhere reads chunk_0_0_z0.txt or
        # board_config.txt back. Both files were pure dead writes.
        #
        # The one real, working map-load path is pc_menu_input.c's own
        # CONFIRM_START_MAP:<map_id> inbox command (EVENT-TRIGGER-LAYER-
        # PLAN.md §3 Step 3), which calls pc_generate_chunk.+x with a
        # real `map:<id>` argv - confirmed by direct code read, and
        # explicitly left "no menu button wired up yet" in its own
        # comment. This verb is that wiring. Real, honest behavior
        # difference from what the old dead code implied: this starts a
        # genuinely NEW world generated from the picked map (same as
        # the New Game map-select flow), not a live in-place swap of
        # the current session's chunk data - no live-reload mechanism
        # exists in the engine at all, so this doesn't pretend to have
        # one. Drained on the very next tick regardless of a keypress -
        # pc_menu_input.+x is polled every tick with key=0 when idle and
        # always drains the inbox first (confirmed by direct code read),
        # so no engage_if_needed/append_key/restore_interact dance is
        # needed here at all.
        HOUSE="$(cd "$SELF_DIR/../../.." && pwd)"
        PCHQ="$(cd "$SELF_DIR/.." && pwd)"
        printf 'open=\n' > "$PKG_STATE/menu.txt"
        PICK="$(sh "$HOUSE/&.widgits/file-explorer/fe-pick.sh" LOAD "$PCHQ/pieces/system/maps")"
        [ -n "$PICK" ] || exit 0
        # REAL FIX 2026-09-15, direct live instruction ("file = dir
        # desk = map... without desk there is no map") - a project dir
        # (what the user actually picks in File Explorer) no longer
        # holds map.txt directly; the real map data now lives one level
        # deeper, under a real desk subdir (v1: always desk1). Handle
        # every real way a pick can land: the project dir itself, a
        # desk subdir picked directly, or map.txt picked directly deep
        # inside a desk - in every case, walk back up to the real
        # PROJECT dir (the one with game.pdl) before deriving NAME, so
        # this never accidentally names a desk ("desk1") as if it were
        # the project.
        case "$PICK" in
            */map.txt) PROJDIR="$(dirname "$(dirname "$PICK")")" ;;
            */desk1)   PROJDIR="$(dirname "$PICK")" ;;
            *)         PROJDIR="$PICK" ;;
        esac
        [ -f "$PROJDIR/desk1/map.txt" ] || exit 0
        NAME="$(basename "$PROJDIR")"
        mkdir -p "$PCHQ/pieces/system/widget_cmds"
        printf 'CONFIRM_START_MAP:%s\n' "$NAME" > "$PCHQ/pieces/system/widget_cmds/inbox.txt"
        ;;
    load-map)
        # Same real fix as file-hq above - see its own comment for the
        # full story (dead chunk_0_0_z0.txt/board_config.txt writes and
        # an unhandled key 54 replaced with the one real, working
        # CONFIRM_START_MAP inbox command).
        PCHQ="$(cd "$SELF_DIR/.." && pwd)"
        mkdir -p "$PCHQ/pieces/system/widget_cmds"
        printf 'CONFIRM_START_MAP:%s\n' "$ARG" > "$PCHQ/pieces/system/widget_cmds/inbox.txt"
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
    player)
        # REAL FIX 2026-09-15, direct live correction ("i think it
        # thinks player means 'player of entity' it actually is player
        # control for game start stop... its the same yes, start stop
        # for game mode... how player in tb should play all entities /
        # events and common events in 'DESK'") - toggles the SAME real,
        # house-wide Play Mode flag PLAY-MODE-ENTITY-HARNESS-DESIGN.md
        # already defines and the desktop taskbar's own "8.player"
        # menu already writes (khtpm_taskbar_manager.c's khtpm_save_
        # play_mode() / khtpm_core_render.c's desktop_load_play_mode(),
        # same file, same `mode=on|off` shape) - that doc's own §2
        # explicitly says "pc-hq has no Play button yet... needs to be
        # added as part of this work". This IS that button. NOT the
        # clock/tick daemon (a separate, always-running thing per the
        # same live correction) - purely the shared on/off flag; what
        # pc-hq's own entities DO differently while it's on (real
        # Play-Mode-only context menus, per that design doc's §4) is
        # real, separate, not-yet-built follow-up work, out of scope
        # for this toggle wiring itself.
        HOUSE="$(cd "$SELF_DIR/../../.." && pwd)"
        PCHQ="$(cd "$SELF_DIR/.." && pwd)"
        PM="$HOUSE/#.desktop/khtpm_play_mode.state.txt"
        mkdir -p "$(dirname "$PM")"
        # REAL, NEW 2026-09-15, direct live report ("player should have
        # dropdown that says play, reset, stop, notes-db, cancel") -
        # ARG picks the sub-action; empty ARG (a bare click, no
        # dropdown) keeps the original toggle behavior for backward
        # compat with the hascanvas variant, which has no dropdown
        # support at all (see that file's own header comment).
        SUB="${ARG:-toggle}"
        case "$SUB" in
            toggle)
                CUR=off
                [ -f "$PM" ] && grep -q 'mode=on' "$PM" && CUR=on
                NEXT=on
                [ "$CUR" = on ] && NEXT=off
                printf 'mode=%s\n' "$NEXT" > "$PM"
                ;;
            stop)
                # Explicit force-off, distinct from toggle - "make sure
                # it's definitely stopped" without needing to read the
                # current label first.
                printf 'mode=off\n' > "$PM"
                ;;
            reset)
                # Real reuse of the one real, working map-load path
                # (file-hq/load-map's own fix, same date) - re-issues
                # CONFIRM_START_MAP for whatever map is CURRENTLY
                # active (world_01/state.txt's own map_id, the real
                # field pc_generate_chunk.c writes and reads - NOT
                # board_config.txt, confirmed dead). A real "restart
                # this same map fresh" action, not a guess.
                WORLD_STATE="$PCHQ/pieces/world_01/state.txt"
                MAP_ID=""
                [ -f "$WORLD_STATE" ] && MAP_ID="$(sed -n 's/^map_id=//p' "$WORLD_STATE" | head -1)"
                if [ -n "$MAP_ID" ]; then
                    mkdir -p "$PCHQ/pieces/system/widget_cmds"
                    printf 'CONFIRM_START_MAP:%s\n' "$MAP_ID" > "$PCHQ/pieces/system/widget_cmds/inbox.txt"
                fi
                ;;
            notes)
                # REAL, NEW 2026-09-15 - "it just opens a text file in
                # dir of project that lets user take notes" (direct
                # live answer). text-edit-hq has no file-argv (single-
                # instance, always launches fresh - confirmed by
                # reading its own button.sh) so this ensures the real
                # notes file exists, then launches the editor; the user
                # opens it themselves from there.
                NOTES="$PCHQ/notes.txt"
                [ -f "$NOTES" ] || : > "$NOTES"
                setsid sh "$HOUSE/@.apps/text-edit-hq/button.sh" run >/tmp/pchq-notes-texted.log 2>&1 < /dev/null &
                ;;
        esac
        ;;
esac
exit 0
