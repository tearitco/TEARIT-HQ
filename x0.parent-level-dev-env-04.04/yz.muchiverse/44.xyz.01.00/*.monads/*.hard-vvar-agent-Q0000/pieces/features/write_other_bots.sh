#!/bin/bash
# write_other_bots.sh - v1 write-other-bots op. Spawns a fresh bot entity
# under this monad's entities/<name>/ with meta.pdl + ledger.
# Usage: write_other_bots.sh <name> <glyph> "<note>"
. "$(cd "$(dirname "$0")/../brain" && pwd)/oplib.sh"

NAME="$1"
GLYPH="${2:-🤖}"
NOTE="$3"
[ -z "$NOTE" ] && NOTE="bot written by vvarware"

[ -z "$NAME" ] && NAME="bot_$(tr -dc 'a-z' < /dev/urandom | head -c6)"

PKG="$MONAD_DIR/entities/$NAME"
if [ -d "$PKG" ]; then
    ledger_append "WriteBots" "bot $NAME already exists" "write_other_bots.sh"
    echo "WriteBots: $NAME already exists"
    exit 0
fi

mkdir -p "$PKG"
INSTANCE_ID=$(tr -dc 'A-Z0-9' < /dev/urandom | head -c4)
printf 'x=120\ny=400\n' > "$PKG/desktop_pos.txt"
printf '%s' "$GLYPH" > "$PKG/glyph.txt"
echo "$INSTANCE_ID" > "$PKG/instance_id.txt"
printf 'battery=100\nenergy=50\ngold=5\n' > "$PKG/state.txt"
{
    echo "SECTION      | KEY                | VALUE"
    echo "----------------------------------------"
    echo "META         | piece_id           | $NAME"
    echo "STATE        | kind                 | bot"
    echo "STATE        | glyph                | $GLYPH"
    echo "STATE        | instance_id          | $INSTANCE_ID"
    echo "STATE        | note                 | $NOTE"
    echo "METHOD       | Ledger               | gedit \"$PKG/master_ledger.txt\""
    echo "METHOD       | Dir                  | xdg-open"
    echo "METHOD       | Close                | CLOSE"
    echo "METHOD       | Cancel               | void"
} > "$PKG/meta.pdl"
{
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] Created: $NAME bot written by vvarware ($NOTE) | Trigger: write_other_bots.sh"
} > "$PKG/master_ledger.txt"

ledger_append "WriteBots" "wrote new bot $NAME ($GLYPH): $NOTE" "write_other_bots.sh"
echo "WriteBots: $NAME spawned at entities/$NAME"
