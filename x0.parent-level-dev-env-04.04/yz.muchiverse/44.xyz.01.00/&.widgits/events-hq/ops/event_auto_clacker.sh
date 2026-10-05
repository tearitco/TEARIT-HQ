#!/bin/sh
# event_auto_clacker.sh - EVENT-MODULARITY-AND-BUILD-SPEED.md §1's
# automatic three-tier Russian-doll materializer: given one page that
# just got created/changed inside an entity's own event_pkg, ensures
# that entity has a real 🎬️ "event clacker" nested inside ITS OWN
# inventory/, and a real ⚙️ "page" nested inside the 🎬️'s own inventory/,
# matching the corrected design's core rule: "a pal and an inventory
# item are the SAME real object/directory" - placement (desk vs. inside
# another entity's inventory/) is the only difference, never a
# different storage shape.
#
# Deliberately does NOT reuse event_page_to_pal.sh's "copy a template
# pal" step for these two tiers - that tool's template arg is meant for
# an EXTERNAL, unrelated donor pal (e.g. door_civ) to borrow boilerplate
# from. Here the natural "template" would be the entity itself, and
# cp -r'ing $ENT into a new directory UNDER $ENT/inventory/ is a
# self-nesting copy (destination inside source) - so this writes its own
# minimal pal skeleton directly instead, real but intentionally lighter
# than a full entity template (no master_ledger/history churn to copy,
# there is none to inherit).
#
# Direct instruction (resolved 2026-09-28): automatic, BOTH directions -
# this script is the "new page -> auto ⚙️/🎬️" direction. The other
# direction (dropping a 🎬️/⚙️ onto a target auto-updates that target's
# live view) is separate, not-yet-built work (the drop-target handler).
#
# Usage: event_auto_clacker.sh <entity_pal_dir> <page_name> <house_root>
set -e

ENT="$1"
PAGE="$2"
HOUSE_ROOT="$3"
if [ -z "$HOUSE_ROOT" ]; then
    echo "Usage: event_auto_clacker.sh <entity_pal_dir> <page_name> <house_root>" >&2
    exit 1
fi

SRC_PAGE_DIR="$ENT/event_pkg/pages/$PAGE"
[ -d "$SRC_PAGE_DIR" ] || { echo "event_auto_clacker: no such page: $SRC_PAGE_DIR" >&2; exit 1; }

OPS="$HOUSE_ROOT/_.monads/_.livedesk-taskbar/ops/+x"

# make_minimal_pal <dir> <name> <emoji>
# Real minimal pal skeleton: pal.pdl/meta.pdl/glyph.txt + real sprite via
# the same emoji_gen_atlas/emoji_xtract pipeline event_page_to_pal.sh
# uses - a real, standalone, inspectable object, just without an
# unrelated donor pal's runtime-state files to strip back out.
make_minimal_pal() {
    _dir="$1"; _name="$2"; _emoji="$3"
    mkdir -p "$_dir"
    _hash=$(head -c 64 /dev/urandom | sha256sum | cut -d' ' -f1)
    cat > "$_dir/pal.pdl" <<EOF
PAL | name | $_name
PAL | hash | $_hash
PAL | glyph | $_emoji
EOF
    printf '%s' "$_emoji" > "$_dir/glyph.txt"
    _iid=$(echo "$_name" | tr '[:lower:]' '[:upper:]' | tr -cd 'A-Z0-9' | cut -c1-16)
    cat > "$_dir/meta.pdl" <<EOF
SECTION      | KEY                | VALUE
----------------------------------------
META         | piece_id           | $_name
STATE        | kind                 | deskpal
STATE        | glyph                | $_emoji
STATE        | instance_id          | $_iid
STATE        | event_object         | 1
METHOD       | Events (hq)          | sh -c 'exec "\$1/&.widgits/events-hq/button.sh" "\$0" "\$1"'
METHOD       | Dir                  | sh -c 'exec xdg-open "\$0"'
METHOD       | Close                | CLOSE
METHOD       | Cancel               | void
EOF
    : > "$_dir/history.txt"
    : > "$_dir/interact_relay.txt"
    : > "$_dir/last_signal.txt"
    if [ -x "$OPS/emoji_gen_atlas.+x" ] && [ -x "$OPS/emoji_xtract.+x" ]; then
        "$OPS/emoji_gen_atlas.+x" "$_emoji" "$_dir/atlas.png" >/dev/null 2>&1
        "$OPS/emoji_xtract.+x" "$_dir/atlas.png" 0 64 "$_dir/sprite.csv" >/dev/null 2>&1
    else
        echo "   WARNING: emoji_gen_atlas.+x/emoji_xtract.+x not found under $OPS - $_dir has no sprite yet" >&2
    fi
}

# copy_page_into <dest_pages_dir> <page_name>
copy_page_into() {
    _dest="$1"; _page="$2"
    mkdir -p "$_dest/$_page"
    cp -r "$SRC_PAGE_DIR/." "$_dest/$_page/"
    for f in "$_dest/$_page"/cmd_*.sh; do
        [ -f "$f" ] || continue
        sed -i "s/^# pkg=.* page=.*/# pkg=$(basename "$(dirname "$(dirname "$_dest")")") page=$_page (auto-exported by event_auto_clacker.sh, $(date '+%Y-%m-%d'))/" "$f"
    done
}

# REAL, NEW 2026-09-28 (direct instruction: "clacker should be numbered
# like pages, in case there were more than one, one would take priority
# when executing"): clackers are event_clacker_N, highest N wins at
# dispatch (see play_event.sh's own ACTIVE_CLACKER redirect - matches
# the house's existing "highest-numbered matching page wins" rule and
# real stack/push-pop semantics). This hook only ever creates/maintains
# the entity's own NATIVE clacker, always event_clacker_1 - it mirrors
# the entity's own pages for inventory/inspection/dragging purposes and
# is deliberately NEVER authoritative for dispatch by itself (that would
# make it go stale the moment events-hq edits the entity's real
# event_pkg/pages directly). Only an externally pushed-in clacker
# (event_clacker_2+, from the not-yet-built drop-target handler) ever
# becomes the active dispatch source.
CLACKER_DIR="$ENT/inventory/event_clacker_1"

# --- Tier 1: the 🎬️ clacker itself, created once, lazily -----------------
if [ ! -d "$CLACKER_DIR" ]; then
    echo "-- no event clacker yet for $ENT - creating one"
    make_minimal_pal "$CLACKER_DIR" "event_clacker_1" "🎬️"
else
    echo "-- event clacker already exists for $ENT ($CLACKER_DIR)"
fi

# --- Tier 2: the ⚙️ page, nested inside the clacker's OWN inventory ------
PAGE_ITEM_DIR="$CLACKER_DIR/inventory/$PAGE"
if [ -d "$PAGE_ITEM_DIR" ]; then
    echo "-- ⚙️ page item already exists: $PAGE_ITEM_DIR (leaving as-is, not overwriting live edits)"
    exit 0
fi

echo "-- materializing ⚙️ page item: $PAGE_ITEM_DIR"
make_minimal_pal "$PAGE_ITEM_DIR" "$PAGE" "⚙️"
copy_page_into "$PAGE_ITEM_DIR/event_pkg/pages" "$PAGE"

echo "event_auto_clacker: done ($CLACKER_DIR + $PAGE_ITEM_DIR)"
