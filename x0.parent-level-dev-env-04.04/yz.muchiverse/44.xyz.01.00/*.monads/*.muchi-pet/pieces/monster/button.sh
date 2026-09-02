#!/bin/bash
# button.sh - GENERIC MUCHI_RANCHER monster start script, real file-
# mediated monster selection (direct instruction: "id rather try a
# different monster going forward tho, can we have a .pdl or w/e that
# decides which will open?"). Unlike the earlier one-script-per-monster
# approach (m1_ninjadragon's own hardcoded button.sh), THIS script reads
# which monster to spawn from "$MUCHI_RANCHER/active_monster.pdl"'s own
# real "STATE | selected | <name>" row, and looks up that monster's real
# glyph from "$MUCHI_RANCHER/roster.pdl" (same SECTION|KEY|VALUE parse
# convention as every other .pdl in this house) - so switching monsters
# going forward is just editing one real text file, not writing new code.
GRID_X=27
GRID_Y=5

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
MUCHI_RANCHER="$(cd "$SCRIPT_DIR/../.." && pwd)"
HOUSE_ROOT="$(cd "$MUCHI_RANCHER/../.." && pwd)"
TP="$HOUSE_ROOT/&.widgits/tile-picker"
DESK="$MUCHI_RANCHER/entities"
ACTION="${1:-run}"

read_active_monster() {
    awk -F'\\|' '$1 ~ /STATE/ { gsub(/^[ \t]+|[ \t]+$/, "", $2); if ($2 == "selected") { gsub(/^[ \t]+|[ \t]+$/, "", $3); print $3; exit } }' \
        "$MUCHI_RANCHER/active_monster.pdl"
}

read_glyph() {
    local name="$1"
    awk -F'\\|' -v n="$name" '$1 ~ /MONSTER/ { gsub(/^[ \t]+|[ \t]+$/, "", $2); if ($2 == n) { gsub(/^[ \t]+|[ \t]+$/, "", $3); print $3; exit } }' \
        "$MUCHI_RANCHER/roster.pdl"
}

NAME="$(read_active_monster)"
if [ -z "$NAME" ]; then
    echo "MISSING: no STATE | selected | <name> row in $MUCHI_RANCHER/active_monster.pdl"
    exit 1
fi
GLYPH="$(read_glyph "$NAME")"
if [ -z "$GLYPH" ]; then
    echo "MISSING: no MONSTER | $NAME | <glyph> row in $MUCHI_RANCHER/roster.pdl"
    exit 1
fi
PKG="$DESK/$NAME"

ensure_package() {
    if [ ! -f "$MUCHI_RANCHER/entities/$NAME/sprite.csv" ]; then
        echo "MISSING: $MUCHI_RANCHER/entities/$NAME/sprite.csv (extract it first via mr_monster_extract.+x)"
        exit 1
    fi
    mkdir -p "$PKG"
    if [ ! -f "$PKG/instance_id.txt" ]; then
        INSTANCE_ID=$(tr -dc 'A-Z0-9' < /dev/urandom | head -c4)
        echo "$INSTANCE_ID" > "$PKG/instance_id.txt"
    fi
    INSTANCE_ID=$(cat "$PKG/instance_id.txt" 2>/dev/null || echo "0000")
    echo "$GLYPH" > "$PKG/glyph.txt"
    # meta.pdl is regenerated every run (not "only if missing") so that
    # switching active_monster.pdl to a NEW monster name picks up that
    # monster's own real glyph/kind/footprint immediately, without stale
    # leftover rows from whichever monster was active before. STATE rows
    # the player/RL-agent may have started writing (money, stats, week
    # counter - once those real files exist per work item 4) live in
    # their OWN separate state file, not meta.pdl, so this stays safe.
    {
        echo "SECTION      | KEY                | VALUE"
        echo "----------------------------------------"
        echo "META         | piece_id           | $NAME"
        echo "STATE        | kind                 | monster"
        echo "STATE        | glyph                | $GLYPH"
        echo "STATE        | instance_id          | $INSTANCE_ID"
        echo "STATE        | footprint_tiles      | 2"
        echo "METHOD       | Ledger               | gedit \"$PKG/master_ledger.txt\""
        echo "METHOD       | Events (ez)          | \"$MUCHI_RANCHER/ops/open_event_ez.sh\""
        echo "METHOD       | Dir                  | xdg-open"
        echo "METHOD       | Close                | CLOSE"
        echo "METHOD       | Cancel               | void"
    } > "$PKG/meta.pdl"
    if [ ! -f "$PKG/desktop_pos.txt" ]; then
        local px=$((GRID_X * 80))
        local py=$((GRID_Y * 80))
        printf 'x=%d\ny=%d\n' "$px" "$py" > "$PKG/desktop_pos.txt"
    fi
    if [ ! -f "$PKG/master_ledger.txt" ]; then
        echo "[$(date '+%Y-%m-%d %H:%M:%S')] Created: $NAME entity initialized | Trigger: active_monster.pdl selection" >> "$PKG/master_ledger.txt"
    fi
}

case "$ACTION" in
    run|r|start)
        ensure_package
        if [ -x "$TP/ops/+x/tp_desktop_window.+x" ]; then
            setsid nohup "$TP/ops/+x/tp_desktop_window.+x" "$PKG" >/dev/null 2>&1 < /dev/null &
            disown
            echo "$NAME spawned: $PKG"
        else
            echo "MISSING: $TP/ops/+x/tp_desktop_window.+x (run tile-picker's own 'button.sh compile' first)"
        fi
        ;;
    kill|k|stop)
        # escape regex specials — monad paths contain literal '*' globs
        local re
        re=$(printf '%s' "tp_desktop_window.+x $PKG" | sed 's/[][{}.*+?^$|\\]/\\&/g')
        pkill -f "$re" 2>/dev/null
        echo "done"
        ;;
    check|verify)
        echo "active monster: $NAME ($GLYPH)"
        [ -x "$TP/ops/+x/tp_desktop_window.+x" ] && echo "OK   tp_desktop_window.+x" || echo "MISSING tp_desktop_window.+x"
        [ -f "$MUCHI_RANCHER/entities/$NAME/sprite.csv" ] && echo "OK   sprite.csv" || echo "MISSING sprite.csv"
        ;;
    help|h|-h|--help|*)
        echo "generic MUCHI_RANCHER monster launcher - reads active_monster.pdl for which monster to spawn"
        echo "run | kill | check"
        ;;
esac
