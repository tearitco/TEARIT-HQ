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
    *) exit 0 ;;
esac
exit 0
