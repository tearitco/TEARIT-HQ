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

# The game this board shows: the viewer session records it (pieces/system/real_project_root.txt), so a board of another project
# (rpg-edit-hq-pchq ...) that reuses this script drives ITS OWN inbox and maps, not piececraft-hq's. Falls back to piececraft-hq.
real_pchq() {
    R=""
    [ -n "$BV" ] && [ -f "$BV/pieces/system/real_project_root.txt" ] && R="$(head -n1 "$BV/pieces/system/real_project_root.txt" 2>/dev/null)"
    if [ -n "$R" ] && [ -d "$R/pieces/system" ]; then printf '%s\n' "$R"; else (cd "$SELF_DIR/.." && pwd); fi
}

# menu open/close is pure local state for the projector
if [ "$VERB" = "menu" ]; then
    case "$ARG" in
        file|desk) printf 'open=%s\n' "$ARG" > "$PKG_STATE/menu.txt" ;;
        *)         printf 'open=\n'          > "$PKG_STATE/menu.txt" ;;
    esac
    exit 0
fi

# Empty a copied page folder: every cell becomes the page's most common cell (the ground / sky of the page it was copied from), so the page keeps
# its size, atlas and legend but holds nothing. cells.txt (maker maps: rows of floor,wall[,depth] tokens after the "cells" line) and map.txt (glyph rows).
blank_page() {
    D="$1"
    if [ -f "$D/cells.txt" ]; then
        TOP="$(awk 'BEGIN{r=0} r==1{for(i=1;i<=NF;i++) c[$i]++} /^cells$/{r=1} END{m=0; for(k in c) if(c[k]>m){m=c[k]; t=k} print t}' "$D/cells.txt")"
        [ -n "$TOP" ] || TOP="0,0"
        awk -v t="$TOP" 'BEGIN{r=0} r==1{o=""; for(i=1;i<=NF;i++) o=o (i>1?" ":"") t; print o; next} {print} /^cells$/{r=1}' "$D/cells.txt" > "$D/cells.new" && mv "$D/cells.new" "$D/cells.txt"
    fi
    if [ -f "$D/map.txt" ]; then
        G="$(fold -w1 "$D/map.txt" | grep -v '^$' | sort | uniq -c | sort -rn | head -n1 | awk '{print $2}')"
        [ -n "$G" ] || G="_"
        awk -v g="$G" '{o=""; for(i=1;i<=length($0);i++) o=o g; print o}' "$D/map.txt" > "$D/map.new" && mv "$D/map.new" "$D/map.txt"
    fi
    [ -f "$D/events.txt" ] && : > "$D/events.txt"
    return 0
}

HAVE_BV=0
if [ -n "$BV" ] && [ -d "$BV" ]; then
    HAVE_BV=1
      # 2026-10-02, real live catch ("In: interact mode won't trigger /
      # drive interact mode"). This pointed at player_app/history.txt,
      # which is the EXACT dead end pchq_board_projector.c was already
      # fixed away from on 2026-09-04. Proof from the live session:
      #   board_viewer.chtpm:3  <interact src="pieces/apps/player_app/
      #                                    interact_relay.txt" />
      #   interact_relay.txt  size=0    <- nothing ever arrived
      #   history.txt         [13 13 13 13 ...]  <- 8 keys into a void
      # So every In: click (and every camera key) appended into a file no
      # consumer reads, while the real relay target sat empty. The
      # projector's bv_h1 already points at interact_relay.txt; this must
      # agree or the toolbar can never drive interact mode.
      H1="$BV/pieces/apps/player_app/interact_relay.txt"
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
    autotick)
        # The clock tab of the toolbar: play / pause the game clock of THIS board's world (the same flag the pc-hq ']' key flips, world_01 autotick_enabled;
        # pc_clock_daemon runs the day while it is 1). Starting it resets the baseline so a pause does not count as elapsed time.
        W="$(real_pchq)/pieces/world_01/state.txt"
        [ -f "$W" ] || exit 0
        if grep -q '^autotick_enabled=1' "$W"; then NEW=0; else NEW=1; fi
        sed -i "s/^autotick_enabled=.*/autotick_enabled=$NEW/" "$W"
        [ "$NEW" = 1 ] && sed -i 's/^autotick_last_real_ms=.*/autotick_last_real_ms=0/' "$W"
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
        PCHQ="$(real_pchq)"
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
        PCHQ="$(real_pchq)"
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
        # REAL FIX 2026-09-15 (2), direct live follow-up ("what if we
        # made a default desk 2... so i could validate desk switching
        # works") - this used to send key 54 ('6' - DESK_MENU reload),
        # which pc_menu_input.c has NO handler for at all (confirmed by
        # direct code read, same as file-hq's own header comment found
        # for key 54's OTHER dead caller) - real, permanently dead code,
        # never worked. Real fix: same inbox-command mechanism
        # file-hq/load-map already use, with a new CONFIRM_SET_DESK:
        # command (pc_menu_input.c). ARG is the desk_id
        # (pchq-board.xhtpm's repeat now passes ${d.id}, not a bare row
        # index).
        PCHQ="$(real_pchq)"
        [ -n "$ARG" ] || exit 0
        mkdir -p "$PCHQ/pieces/system/widget_cmds"
        printf 'CONFIRM_SET_DESK:%s\n' "$ARG" > "$PCHQ/pieces/system/widget_cmds/inbox.txt"
        printf 'open=\n' > "$PKG_STATE/menu.txt"
        ;;
    newpage)
        # "+ new page": an EMPTY page (desk) in the current book, then switch to it. A copy of the current page's folder (atlas, legend,
        # extrusion) with the cells cleared, the map blanked, no events; listed in the book's game.pdl (n_desks + desk_N rows).
        PCHQ="$(real_pchq)"
        WS="$PCHQ/pieces/world_01/state.txt"
        MAPID="$(sed -n 's/^map_id=//p' "$WS" | head -n1)"; CUR="$(sed -n 's/^desk_id=//p' "$WS" | head -n1)"
        BOOK="$PCHQ/pieces/system/maps/$MAPID"
        [ -n "$MAPID" ] && [ -f "$BOOK/game.pdl" ] || exit 0
        [ -d "$BOOK/$CUR" ] || CUR="$(sed -n 's/^GAME *| *desk_1_id *| *//p' "$BOOK/game.pdl" | head -n1)"
        [ -d "$BOOK/$CUR" ] || exit 0
        N=2; while [ -e "$BOOK/page_$N" ]; do N=$((N + 1)); done
        NEW="page_$N"
        # A new page has its own map and touch events, but the party roster
        # remains part of the editor chrome. The old bar mixes on-click party
        # rows with page-specific player-touch rows, so preserve only the
        # former. Also remove both halves of the touch-event representation:
        # leaving events.pdl while deleting bar.txt creates an event definition
        # that the touch runner cannot discover.
        cp -r "$BOOK/$CUR" "$BOOK/$NEW" && rm -f "$BOOK/$NEW/bar.txt" "$BOOK/$NEW/events.pdl"
        if [ -f "$BOOK/$CUR/bar.txt" ]; then
            awk -F '\t' 'NF >= 5 && $5 == "on-click"' "$BOOK/$CUR/bar.txt" > "$BOOK/$NEW/bar.txt"
            [ -s "$BOOK/$NEW/bar.txt" ] || rm -f "$BOOK/$NEW/bar.txt"
        fi
        blank_page "$BOOK/$NEW"
        COUNT="$(sed -n 's/^GAME *| *n_desks *| *//p' "$BOOK/game.pdl" | head -n1)"; COUNT=$((COUNT + 1))
        sed -i "s/^\(GAME *| *n_desks *| *\).*/\1$COUNT/" "$BOOK/game.pdl"
        printf 'GAME         | desk_%s_id          | %s\nGAME         | desk_%s_label       | %s\n' "$COUNT" "$NEW" "$COUNT" "$NEW" >> "$BOOK/game.pdl"
        mkdir -p "$PCHQ/pieces/system/widget_cmds"
        printf 'CONFIRM_SET_DESK:%s\n' "$NEW" >> "$PCHQ/pieces/system/widget_cmds/inbox.txt"
        printf 'open=\n' > "$PKG_STATE/menu.txt"
        ;;
    newbook)
        # "+ new book": an EMPTY book (map) with one empty page "start", then switch to it (the same inbox command the File menu uses).
        PCHQ="$(real_pchq)"
        WS="$PCHQ/pieces/world_01/state.txt"
        MAPID="$(sed -n 's/^map_id=//p' "$WS" | head -n1)"; CUR="$(sed -n 's/^desk_id=//p' "$WS" | head -n1)"
        SRC="$PCHQ/pieces/system/maps/$MAPID"
        [ -n "$MAPID" ] && [ -d "$SRC/$CUR" ] || exit 0
        N=2; while [ -e "$PCHQ/pieces/system/maps/book_$N" ]; do N=$((N + 1)); done
        NB="book_$N"; DST="$PCHQ/pieces/system/maps/$NB"
        mkdir -p "$DST" && cp -r "$SRC/$CUR" "$DST/start" && rm -f "$DST/start/bar.txt"
        blank_page "$DST/start"
        sed -e "s/^\(GAME *| *label *| *\).*/\1$NB/" -e "s/^\(GAME *| *n_desks *| *\).*/\11/" -e '/desk_[0-9]*_\(id\|label\)/d' "$SRC/game.pdl" > "$DST/game.pdl"
        printf 'GAME         | desk_1_id          | start\nGAME         | desk_1_label       | start\n' >> "$DST/game.pdl"
        mkdir -p "$PCHQ/pieces/system/widget_cmds"
        printf 'CONFIRM_START_MAP:%s\n' "$NB" >> "$PCHQ/pieces/system/widget_cmds/inbox.txt"
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
            start|play)
                # Same verb as the Start Game event command (registry
                # start_game -> doom_event.sh). New Game in RPG Maker.
                WORLD_STATE="$PCHQ/pieces/world_01/state.txt"
                MAP_ID=""; DESK_ID=""
                [ -f "$WORLD_STATE" ] && MAP_ID="$(sed -n 's/^map_id=//p' "$WORLD_STATE" | head -1)"
                [ -f "$WORLD_STATE" ] && DESK_ID="$(sed -n 's/^desk_id=//p' "$WORLD_STATE" | head -1)"
                sh "$PCHQ/ops/doom_event.sh" start_game "${MAP_ID:-doom}" "${DESK_ID:-title}"
                ;;
            synch)
                # Snapshot the desk's current book and page onto this
                # board. A later change on the desk does not move the
                # board until Synch is clicked again.
                sh "$PCHQ/ops/pc_synch_request.sh" taskbar
                ;;
            playtest)
                # play-test (owner 2026-10-06): the game runs AND editing is allowed. The same file gets an optional SECOND line `playtest=1` next to `mode=on`;
                # older readers only test the first line, so they see plain ON. From off or plain play -> play-test; from play-test -> plain play.
                # (toggle/stop rewrite the file with `mode=...` only, which clears it.) Mirrors the taskbar's livedesk:playtest-toggle.
                if grep -q 'playtest=1' "$PM" 2>/dev/null && grep -q '^mode=on' "$PM" 2>/dev/null; then
                    printf 'mode=on\n' > "$PM"
                else
                    printf 'mode=on\nplaytest=1\n' > "$PM"
                fi
                ;;
            stop)
                # Same verb as the Stop Game event command.
                sh "$PCHQ/ops/doom_event.sh" stop_game
                ;;
            save:*|load:*)
                # Player-menu slots 1-16. save:3 / load:3.
                kind=${SUB%%:*}
                num=${SUB#*:}
                if [ "$kind" = save ]; then
                    sh "$PCHQ/ops/doom_event.sh" save_slot "$num"
                else
                    sh "$PCHQ/ops/doom_event.sh" load_slot "$num"
                fi
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
