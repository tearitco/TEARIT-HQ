#!/bin/sh
# Chess book verbs. Events call this. The rules live in chess_rules.py.
HERE="$(cd "$(dirname "$0")" && pwd)"
PCHQ="$(cd "$HERE/.." && pwd)"
HOUSE="$(cd "$HERE/../../.." && pwd)"
BOOK="$PCHQ/pieces/system/maps/chess"
PM="$HOUSE/#.desktop/khtpm_play_mode.state.txt"
WORLD="$PCHQ/pieces/world_01/state.txt"
INBOX="$PCHQ/pieces/system/widget_cmds/inbox.txt"
PY="$HERE/chess_rules.py"
VERB="${1:-}"
A2="${2:-}"
A3="${3:-}"

set_desk() {
    mkdir -p "$(dirname "$WORLD")" "$(dirname "$INBOX")"
    if [ -f "$WORLD" ]; then
        grep -q '^map_id=' "$WORLD" && sed -i 's/^map_id=.*/map_id=chess/' "$WORLD" || printf 'map_id=chess\n' >> "$WORLD"
        grep -q '^desk_id=' "$WORLD" && sed -i "s/^desk_id=.*/desk_id=$1/" "$WORLD" || printf 'desk_id=%s\n' "$1" >> "$WORLD"
    fi
    printf 'CONFIRM_SET_DESK:%s\n' "$1" > "$INBOX"
    printf 'turn=white\nmode=%s\nresult=none\nselected=\n' "$2" > "$BOOK/state.pdl"
}

case "$VERB" in
    start_game|start_standard)
        printf 'mode=on\n' > "$PM"
        set_desk standard player
        ;;
    start_computer)
        printf 'mode=on\n' > "$PM"
        set_desk standard computer
        ;;
    start_player)
        printf 'mode=on\n' > "$PM"
        set_desk standard player
        python3 "$PY" offer
        ;;
    start_king_pawn)
        printf 'mode=on\n' > "$PM"
        set_desk king_pawn computer
        ;;
    start_endgame)
        printf 'mode=on\n' > "$PM"
        set_desk endgame computer
        ;;
    stop_game)
        printf 'mode=off\n' > "$PM"
        ;;
    show_elo)
        python3 "$PY" show_elo
        ;;
    select)
        python3 "$PY" select "$A2" "$A3"
        ;;
    land)
        python3 "$PY" land "$A2" "$A3"
        ;;
    *)
        exit 0
        ;;
esac
