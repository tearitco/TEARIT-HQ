#!/bin/sh
# ch_item.sh — real, generic action script for chat-hai's non-composer
# controls (session switch/new/delete, pause toggle, speed cycle),
# 2026-09-01 migration onto the shared khtpm_core_render.+x. Same real
# "bake a literal request line, generic dispatch appends package_dir
# house_root" convention open-hai's own oh_write_request.sh
# established - see that file's own header comment for the full
# argv-contract rationale. A plain <item action=...> gets 2 appended
# trailing args (package_dir, house_root):
#   $1 = the real request line (NEW / SWITCH|<name> / DELETE|<name> /
#        PAUSE / SPEED)
#   $2 = package_dir (unused)
#   $3 = house_root
set -u
LINE="$1"
HOUSE_ROOT="$3"
APP_DIR="$HOUSE_ROOT/&.hq-apps/chat-hai"
STATE_DIR="$APP_DIR/state"
SESSIONS_DIR="$STATE_DIR/sessions"
ACTIVE_FILE="$SESSIONS_DIR/active.txt"
PAUSE_FILE="$STATE_DIR/paused.txt"
CONFIG_PDL="$APP_DIR/chat_hai_config.pdl"
mkdir -p "$SESSIONS_DIR"

case "$LINE" in
    NEW)
        NAME="session_$(date +%s)"
        : > "$SESSIONS_DIR/$NAME.ledger"
        printf '%s\n' "$NAME" > "$ACTIVE_FILE"
        ;;
    SWITCH\|*|SWITCH\ *)
        # SWITCH|<name> (old chat-hai.chtpm path) OR "SWITCH <name>"
        # (chat-hai.xhtpm path - keeps '|' out of the <item action=>
        # string so it can't shift fields in entity_menu_frame_*.txt).
        case "$LINE" in
            SWITCH\|*) NAME="${LINE#SWITCH|}" ;;
            *)         NAME="${LINE#SWITCH }" ;;
        esac
        [ -f "$SESSIONS_DIR/$NAME.ledger" ] && printf '%s\n' "$NAME" > "$ACTIVE_FILE"
        ;;
    DELETE\|*|DELETE\ *)
        case "$LINE" in
            DELETE\|*) NAME="${LINE#DELETE|}" ;;
            *)         NAME="${LINE#DELETE }" ;;
        esac
        CURRENT="$(cat "$ACTIVE_FILE" 2>/dev/null || echo main)"
        rm -f "$SESSIONS_DIR/$NAME.ledger"
        if [ "$CURRENT" = "$NAME" ]; then
            # real "don't leave the app with no active session" guard -
            # switch to whatever's left (lexically first), or recreate
            # "main" fresh if that was the last one.
            REMAINING="$(find "$SESSIONS_DIR" -maxdepth 1 -name '*.ledger' 2>/dev/null | sort | head -1)"
            if [ -n "$REMAINING" ]; then
                printf '%s\n' "$(basename "$REMAINING" .ledger)" > "$ACTIVE_FILE"
            else
                : > "$SESSIONS_DIR/main.ledger"
                printf 'main\n' > "$ACTIVE_FILE"
            fi
        fi
        ;;
    PAUSE)
        CUR="$(cat "$PAUSE_FILE" 2>/dev/null)"
        if [ "$CUR" = "1" ]; then printf '0\n' > "$PAUSE_FILE"; else printf '1\n' > "$PAUSE_FILE"; fi
        ;;
    SPEED)
        # real, working GUI speed control - cycles the same fixed
        # preset list the old (now-dead) C speed-toggle button used
        # (chai_activate_elem()'s own "speed-toggle" branch), writing
        # ONLY the two keys chat_hai_loop.sh's own sleep_between()/
        # sound_on() functions actually read (the old C writer also
        # wrote 5 more window-geometry keys - real, but exclusively for
        # its own now-dead custom renderer, not needed here).
        CUR="$(awk -F'|' '$1 ~ /^SECTION/ {gsub(/ /,"",$2); if ($2=="sleep_between") {gsub(/ /,"",$3); print $3; exit}}' "$CONFIG_PDL" 2>/dev/null)"
        SOUND="$(awk -F'|' '$1 ~ /^SECTION/ {gsub(/ /,"",$2); if ($2=="sound_on") {gsub(/ /,"",$3); print $3; exit}}' "$CONFIG_PDL" 2>/dev/null)"
        [ -z "$CUR" ] && CUR=6
        [ -z "$SOUND" ] && SOUND=1
        NEXT=2
        case "$CUR" in
            2) NEXT=4 ;;
            4) NEXT=6 ;;
            6) NEXT=12 ;;
            12) NEXT=20 ;;
            20) NEXT=2 ;;
        esac
        {
            echo "# chat_hai_config.pdl - live-edited by the generic .chtpm projection (ch_item.sh)"
            echo "SECTION | sleep_between | $NEXT"
            echo "SECTION | sound_on | $SOUND"
        } > "$CONFIG_PDL"
        ;;
esac
