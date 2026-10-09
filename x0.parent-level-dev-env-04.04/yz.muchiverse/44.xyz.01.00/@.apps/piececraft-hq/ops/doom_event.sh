#!/bin/sh
# doom_event.sh <verb> [book] [desk]
# Start and stop are the same Play Mode flag the Player menu writes.
# The other verbs are the Doom pages (health, ammo, key, exit, touch).
VERB="${1:-}"
BOOK="${2:-doom}"
DESK="${3:-e1m1_hangar}"
HERE="$(cd "$(dirname "$0")" && pwd)"
PCHQ="$(cd "$HERE/.." && pwd)"
HOUSE="$(cd "$HERE/../../.." && pwd)"
PM="$HOUSE/#.desktop/khtpm_play_mode.state.txt"
ST="$PCHQ/pieces/system/maps/doom/state.pdl"
INBOX="$PCHQ/pieces/system/widget_cmds/inbox.txt"
mkdir -p "$(dirname "$PM")" "$(dirname "$INBOX")" "$(dirname "$ST")"
[ -f "$ST" ] || printf 'hp=100\nammo=50\nkeys=0\nbattle=0\n' > "$ST"

add_key() {
    k="$1"
    cur=$(sed -n "s/^$k=//p" "$ST" | head -1)
    cur=${cur:-0}
    n=$((cur + $2))
    if grep -q "^$k=" "$ST"; then
        sed -i "s/^$k=.*/$k=$n/" "$ST"
    else
        printf '%s=%s\n' "$k" "$n" >> "$ST"
    fi
}

case "$VERB" in
    start_game|player_start)
        printf 'mode=on\n' > "$PM"
        printf 'hp=100\nammo=50\nkeys=0\nbattle=0\n' > "$ST"
        printf 'CONFIRM_START_MAP:%s\n' "$BOOK" > "$INBOX"
        ;;
    stop_game)
        printf 'mode=off\n' > "$PM"
        ;;
    change_hp) add_key hp 10 ;;
    change_ammo) add_key ammo 10 ;;
    change_items) add_key keys 1 ;;
    start_battle) add_key battle 1 ;;
    next_level)
        case "$DESK" in
            e1m1_hangar) nxt=e1m2_nukage ;;
            e1m2_nukage) nxt=e1m3_toxin ;;
            *) nxt=e1m1_hangar ;;
        esac
        printf 'CONFIRM_SET_DESK:%s\n' "$nxt" > "$INBOX"
        ;;
    save_slot|load_slot)
        # Slot 1-16. Same directory the taskbar Player menu uses
        # (xyzfs/users/<uuid>/home/livedesk/savegames). book.pdl is the
        # part this house restores: book, desk, play flag, and that
        # book's state.pdl. The taskbar's game_slot_op hash is a separate
        # audit and does not restore entities.
        n="$BOOK"
        case "$n" in
            ''|*[!0-9]*) exit 2 ;;
        esac
        [ "$n" -ge 1 ] && [ "$n" -le 16 ] || exit 2
        uuid=""
        login=$(ls -d "$HOUSE"/0.user-pal*/00.login-signup/current_login.txt 2>/dev/null | head -1)
        [ -n "$login" ] && uuid=$(sed -n 's/^current_user_uuid=//p' "$login" | head -1)
        if [ -z "$uuid" ]; then
            for d in "$HOUSE"/xyzfs/users/*/home/livedesk; do
                [ -d "$d" ] || continue
                uuid=$(basename "$(dirname "$(dirname "$d")")")
                break
            done
        fi
        if [ -n "$uuid" ]; then
            SG="$HOUSE/xyzfs/users/$uuid/home/livedesk/savegames"
        else
            SG="$HOUSE/#.desktop/savegames"
        fi
        slot="$SG/slot_$(printf '%02d' "$n")"
        WORLD="$PCHQ/pieces/world_01/state.txt"
        if [ "$VERB" = save_slot ]; then
            mkdir -p "$slot"
            map=$(sed -n 's/^map_id=//p' "$WORLD" 2>/dev/null | head -1)
            desk=$(sed -n 's/^desk_id=//p' "$WORLD" 2>/dev/null | head -1)
            mode=off
            [ -f "$PM" ] && grep -q '^mode=on' "$PM" && mode=on
            {
                printf 'map_id=%s\n' "${map:-doom}"
                printf 'desk_id=%s\n' "${desk:-title}"
                printf 'mode=%s\n' "$mode"
            } > "$slot/book.pdl"
            bdir="$PCHQ/pieces/system/maps/${map:-doom}"
            [ -f "$bdir/state.pdl" ] && cp "$bdir/state.pdl" "$slot/state.pdl"
            printf 'SLOT | n | %s\nSLOT | saved_at | %s\n' "$n" "$(date '+%Y-%m-%d %H:%M:%S')" > "$slot/meta.pdl"
        else
            [ -f "$slot/book.pdl" ] || exit 1
            map=$(sed -n 's/^map_id=//p' "$slot/book.pdl" | head -1)
            desk=$(sed -n 's/^desk_id=//p' "$slot/book.pdl" | head -1)
            mode=$(sed -n 's/^mode=//p' "$slot/book.pdl" | head -1)
            [ -n "$map" ] && [ -f "$WORLD" ] && sed -i "s/^map_id=.*/map_id=$map/" "$WORLD"
            [ -n "$desk" ] && [ -f "$WORLD" ] && sed -i "s/^desk_id=.*/desk_id=$desk/" "$WORLD"
            printf 'mode=%s\n' "${mode:-on}" > "$PM"
            if [ -f "$slot/state.pdl" ] && [ -n "$map" ]; then
                mkdir -p "$PCHQ/pieces/system/maps/$map"
                cp "$slot/state.pdl" "$PCHQ/pieces/system/maps/$map/state.pdl"
            fi
            # One inbox line. The reader keeps the first command only.
            # map_id is already updated, and SET_DESK reloads that book
            # on the saved desk (map:<id>:<desk>).
            if [ -n "$desk" ]; then
                printf 'CONFIRM_SET_DESK:%s\n' "$desk" > "$INBOX"
            else
                printf 'CONFIRM_START_MAP:%s\n' "${map:-doom}" > "$INBOX"
            fi
        fi
        ;;
    *) exit 0 ;;
esac
exit 0
