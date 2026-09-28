#!/bin/sh
# event_page_to_pal.sh - real Track 1 of EVENT-MODULARITY-AND-BUILD-
# SPEED.md §1: exports a real, existing event page into its own,
# separate, draggable 🎬️ pal. Works identically on a brand-new event
# or a five-week-old one - there is no separate "retroactive" path,
# see that doc's own "Making this real RETROACTIVELY too" section for
# why (this IS that answer, not a different tool).
#
# House convention: a one-shot authoring/admin tool, shell is correct
# here (same real precedent as entity-cli's own open_entity_act.sh) -
# this is NOT a runtime game-loop/event-dispatch orchestrator, which is
# the actual thing PRISC-OPS-ARCHITECTURE.md's "never shell" rule
# targets.
#
# Usage:
#   event_page_to_pal.sh <source_event_pkg_dir> <source_page_name> \
#                         <template_pal_dir> <target_pals_dir> \
#                         <new_pal_name> <emoji> <house_root>
#
#   source_event_pkg_dir  the entity's own event_pkg/ (e.g. .../robot_chat_001/event_pkg)
#   source_page_name      e.g. page_1
#   template_pal_dir       any minimal existing pal to copy boilerplate from (e.g. .../pals/door_civ)
#   target_pals_dir        where the new 🎬️ pal gets created (e.g. .../home/livedesk/pals)
#   new_pal_name           the new pal's own directory/piece_id name
#   emoji                  the real UTF-8 emoji for glyph/sprite (🎬️ for this feature)
#   house_root             the house root (for locating emoji_gen_atlas.+x/emoji_xtract.+x)
set -e

SRC_PKG="$1"
SRC_PAGE="$2"
TEMPLATE="$3"
TARGET_PALS="$4"
NEW_NAME="$5"
EMOJI="$6"
HOUSE_ROOT="$7"

if [ -z "$HOUSE_ROOT" ]; then
    echo "Usage: event_page_to_pal.sh <source_event_pkg_dir> <source_page_name> <template_pal_dir> <target_pals_dir> <new_pal_name> <emoji> <house_root>" >&2
    exit 1
fi

SRC_PAGE_DIR="$SRC_PKG/pages/$SRC_PAGE"
[ -d "$SRC_PAGE_DIR" ] || { echo "event_page_to_pal: no such page: $SRC_PAGE_DIR" >&2; exit 1; }
[ -d "$TEMPLATE" ] || { echo "event_page_to_pal: no such template pal: $TEMPLATE" >&2; exit 1; }
NEW_DIR="$TARGET_PALS/$NEW_NAME"
[ -e "$NEW_DIR" ] && { echo "event_page_to_pal: $NEW_DIR already exists, refusing to overwrite" >&2; exit 1; }

echo "-- copying template skeleton ($TEMPLATE) -> $NEW_DIR"
cp -r "$TEMPLATE" "$NEW_DIR"

# Real runtime state, never copied from the template's own life - a
# fresh pal starts with a genuinely empty history, same convention
# this session's own robot_chat_001 build already used.
rm -rf "$NEW_DIR/event_pkg" "$NEW_DIR/master_ledger.txt"
: > "$NEW_DIR/history.txt"
: > "$NEW_DIR/interact_relay.txt"
: > "$NEW_DIR/last_signal.txt"
rm -f "$NEW_DIR/module_parent.pid"
# Also drop the template's own phymoji_assets copy - it's shaped like
# the TEMPLATE, not this new pal, and would silently reproduce the
# exact "still looks like the old pal" bug this house already hit once
# tonight (BUG-LOG.md / robot_chat_001's own real fix) if left in.
rm -rf "$NEW_DIR/pieces/registry/phymoji_assets"

echo "-- copying event page ($SRC_PAGE_DIR) -> $NEW_DIR/event_pkg/pages/page_1"
mkdir -p "$NEW_DIR/event_pkg/pages/page_1"
cp -r "$SRC_PAGE_DIR/." "$NEW_DIR/event_pkg/pages/page_1/"
# The compiled cmd_N.sh's own "# pkg=... page=..." header comment is
# informational only (confirmed live, both hand-authored examples this
# session resolve $ENT at runtime via $(dirname "$0")-relative logic,
# never a baked path) - rewritten here anyway so it doesn't lie to the
# next reader, not because anything executes it.
for f in "$NEW_DIR/event_pkg/pages/page_1"/cmd_*.sh; do
    [ -f "$f" ] || continue
    sed -i "s/^# pkg=.* page=.*/# pkg=$NEW_NAME page=page_1 (exported by event_page_to_pal.sh from $SRC_PAGE, $(date '+%Y-%m-%d'))/" "$f"
done
for f in "$NEW_DIR/event_pkg/pages/page_1"/*.pdl; do
    [ -f "$f" ] || continue
    sed -i "s/^META         | piece_id           | .*/META         | piece_id           | $NEW_NAME/" "$f"
done

echo "-- writing pal.pdl / meta.pdl / glyph.txt"
NEW_HASH=$(head -c 64 /dev/urandom | sha256sum | cut -d' ' -f1)
cat > "$NEW_DIR/pal.pdl" <<EOF
PAL | name | $NEW_NAME
PAL | hash | $NEW_HASH
PAL | glyph | $EMOJI
EOF
printf '%s' "$EMOJI" > "$NEW_DIR/glyph.txt"
INSTANCE_ID=$(echo "$NEW_NAME" | tr '[:lower:]' '[:upper:]' | tr -cd 'A-Z0-9' | cut -c1-16)
cat > "$NEW_DIR/meta.pdl" <<EOF
SECTION      | KEY                | VALUE
----------------------------------------
META         | piece_id           | $NEW_NAME
STATE        | kind                 | deskpal
STATE        | glyph                | $EMOJI
STATE        | instance_id          | $INSTANCE_ID
METHOD       | Events (hq)          | sh -c 'exec "\$1/&.widgits/events-hq/button.sh" "\$0" "\$1"'
METHOD       | Dir                  | sh -c 'exec xdg-open "\$0"'
METHOD       | Close                | CLOSE
METHOD       | Cancel               | void
EOF

echo "-- generating real sprite (emoji_gen_atlas + emoji_xtract)"
OPS="$HOUSE_ROOT/*.monads/*.livedesk-taskbar/ops/+x"
if [ -x "$OPS/emoji_gen_atlas.+x" ] && [ -x "$OPS/emoji_xtract.+x" ]; then
    "$OPS/emoji_gen_atlas.+x" "$EMOJI" "$NEW_DIR/atlas.png" >/dev/null 2>&1
    "$OPS/emoji_xtract.+x" "$NEW_DIR/atlas.png" 0 64 "$NEW_DIR/sprite.csv" >/dev/null 2>&1
    echo "   ok: $NEW_DIR/sprite.csv / atlas.png regenerated for $EMOJI"
else
    echo "   WARNING: emoji_gen_atlas.+x/emoji_xtract.+x not found under $OPS - $NEW_DIR keeps the template's own sprite, will look wrong until regenerated by hand" >&2
fi

echo "event_page_to_pal: created $NEW_DIR (source: $SRC_PAGE_DIR)"
