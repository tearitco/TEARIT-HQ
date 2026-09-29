#!/bin/sh
# event_auto_puzzle.sh - EVENT-MODULARITY-AND-BUILD-SPEED.md §1's tier 3:
# every individual command/event appended to a page also becomes a real
# 🧩 "puzzle piece" nested inside that page's own ⚙️ item's inventory/.
# Direct instruction: "all events go in here (in a page) as puzzle
# pieces, just for logical continuity of drag and drop. they dont need
# to show up on pals" - a 🧩 is real and inspectable (own pal.pdl/
# meta.pdl/glyph), but is NOT rewired to be independently re-executable
# on its own (that would mean replicating khtpm_events_hq_manager.c's
# own expand_template() command-registry compiler in shell) - out of
# scope per the direct instruction's own framing (drag-drop continuity,
# not standalone execution). It carries a copy of its one NODE line and
# its cmd_N.sh wrapper (if the command type produced one) for reference/
# inspection.
#
# Ensures tier 1 (🎬️) and tier 2 (⚙️) exist first via
# event_auto_clacker.sh (idempotent, safe to call every time - "no
# structural difference between new/old").
#
# Usage: event_auto_puzzle.sh <entity_pal_dir> <page_name> <cmd_id> <house_root>
set -e

ENT="$1"
PAGE="$2"
CMD_ID="$3"
HOUSE_ROOT="$4"
if [ -z "$HOUSE_ROOT" ]; then
    echo "Usage: event_auto_puzzle.sh <entity_pal_dir> <page_name> <cmd_id> <house_root>" >&2
    exit 1
fi

OPS_DIR="$(cd "$(dirname "$0")" && pwd)"
SRC_PAGE_DIR="$ENT/event_pkg/pages/$PAGE"
[ -d "$SRC_PAGE_DIR" ] || { echo "event_auto_puzzle: no such page: $SRC_PAGE_DIR" >&2; exit 1; }

# Tier 1 + 2 first (idempotent - skips instantly if already present).
sh "$OPS_DIR/event_auto_clacker.sh" "$ENT" "$PAGE" "$HOUSE_ROOT" >/dev/null

PIECE_DIR="$ENT/inventory/event_clacker_1/inventory/$PAGE/inventory/cmd_$CMD_ID"
if [ -d "$PIECE_DIR" ]; then
    echo "-- 🧩 puzzle piece already exists: $PIECE_DIR (leaving as-is)"
    exit 0
fi

# Pull out this one NODE line (the real source of truth for the
# command's type+params) from the page's own event.ir.pdl.
IR="$SRC_PAGE_DIR/event.ir.pdl"
NODE_LINE=$(grep "id=$CMD_ID " "$IR" 2>/dev/null | grep "^NODE" | head -1)
if [ -z "$NODE_LINE" ]; then
    echo "event_auto_puzzle: no NODE id=$CMD_ID found in $IR" >&2
    exit 1
fi

OPS="$HOUSE_ROOT/_.monads/_.livedesk-taskbar/ops/+x"
mkdir -p "$PIECE_DIR"
_hash=$(head -c 64 /dev/urandom | sha256sum | cut -d' ' -f1)
cat > "$PIECE_DIR/pal.pdl" <<EOF
PAL | name | cmd_$CMD_ID
PAL | hash | $_hash
PAL | glyph | 🧩
EOF
printf '🧩' > "$PIECE_DIR/glyph.txt"
_iid="CMD$CMD_ID"
cat > "$PIECE_DIR/meta.pdl" <<EOF
SECTION      | KEY                | VALUE
----------------------------------------
META         | piece_id           | cmd_$CMD_ID
STATE        | kind                 | deskpal
STATE        | glyph                | 🧩
STATE        | instance_id          | $_iid
STATE        | event_object         | 1
METHOD       | Dir                  | sh -c 'exec xdg-open "\$0"'
METHOD       | Close                | CLOSE
METHOD       | Cancel               | void
EOF
: > "$PIECE_DIR/history.txt"
: > "$PIECE_DIR/interact_relay.txt"
: > "$PIECE_DIR/last_signal.txt"
echo "$NODE_LINE" > "$PIECE_DIR/event_node.pdl"
SRC_CMD="$SRC_PAGE_DIR/cmd_$CMD_ID.sh"
if [ -f "$SRC_CMD" ]; then
    cp "$SRC_CMD" "$PIECE_DIR/cmd_$CMD_ID.sh"
    chmod +x "$PIECE_DIR/cmd_$CMD_ID.sh"
fi
if [ -x "$OPS/emoji_gen_atlas.+x" ] && [ -x "$OPS/emoji_xtract.+x" ]; then
    "$OPS/emoji_gen_atlas.+x" "🧩" "$PIECE_DIR/atlas.png" >/dev/null 2>&1
    "$OPS/emoji_xtract.+x" "$PIECE_DIR/atlas.png" 0 64 "$PIECE_DIR/sprite.csv" >/dev/null 2>&1
fi

echo "event_auto_puzzle: done ($PIECE_DIR)"
