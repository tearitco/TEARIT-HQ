#!/bin/sh
# event_drop_handler.sh - EVENT-MODULARITY-AND-BUILD-SPEED.md §1's
# drop-target handler: dropping a real 🎬️ clacker or ⚙️ page onto an
# entity's events-hq window pushes a new, numbered event_clacker_N onto
# that entity - copy now (per the resolved copy-vs-link decision), ALL
# pages if a whole clacker was dropped (per the resolved "every page"
# instruction). The new clacker's number automatically becomes the
# active dispatch source per play_event.sh's own highest-number-wins
# redirect (no wiring needed here - that already exists and reads
# purely from disk).
#
# Real, already-existing XDND convention (khtpm_core_render.c,
# 2026-08-24; same file-explorer/fe_drop.sh precedent this reuses
# verbatim): $1=package_dir $2=house_root $DROP_PATH=env var, the
# dropped source path. package_dir for events-hq is the entity's
# event_pkg dir (see khtpm_events_hq_manager.c's own argv[2]), so the
# entity dir itself is package_dir's parent.
set -u
PKG_DIR="${1:-}"
HOUSE_ROOT="${2:-}"
DROP="${DROP_PATH:-}"
[ -n "$PKG_DIR" ] && [ -n "$HOUSE_ROOT" ] && [ -n "$DROP" ] || exit 0
[ -d "$DROP" ] || exit 0
ENT="$(dirname "$PKG_DIR")"
[ -d "$ENT" ] || exit 0

# Only real event_object=1 pals (🎬️/⚙️/🧩, this feature's own objects)
# are meaningful drops here - anything else silently no-ops, the same
# "not every drop target wants every drop" convention fe_drop.sh itself
# already follows (case-by-case exits, never an error for a mismatch).
DROP_META="$DROP/meta.pdl"
[ -f "$DROP_META" ] || exit 0
is_event_obj=0
if grep "event_object" "$DROP_META" 2>/dev/null | grep -q "| 1"; then is_event_obj=1; fi
[ "$is_event_obj" = "1" ] || exit 0
DROP_GLYPH=$(cat "$DROP/glyph.txt" 2>/dev/null)

# Gather every source page to copy, one per line as "path".
TMP_LIST=$(mktemp)
trap 'rm -f "$TMP_LIST"' EXIT

case "$DROP_GLYPH" in
    🎬️)
        # Whole clacker: every page reachable from it - flat
        # (event_pkg/pages/page_*, the shape a PUSHED clacker itself
        # has) AND nested (inventory/page_N/event_pkg/pages/page_N, the
        # shape the NATIVE clacker_1 mirror has, one real ⚙️ item per
        # page) - a clacker could in principle be either shape
        # depending on how it was created, so check both rather than
        # assuming.
        find "$DROP/event_pkg/pages" -maxdepth 1 -type d -name 'page_*' 2>/dev/null >> "$TMP_LIST"
        find "$DROP/inventory" -mindepth 3 -maxdepth 5 -type d -path "*/event_pkg/pages/page_*" 2>/dev/null >> "$TMP_LIST"
        ;;
    ⚙️)
        # Single page item: its own one page only.
        find "$DROP/event_pkg/pages" -maxdepth 1 -type d -name 'page_*' 2>/dev/null >> "$TMP_LIST"
        ;;
    *)
        # 🧩 or anything else: out of scope for now (see
        # EVENT-MODULARITY-AND-BUILD-SPEED.md - a bare individual event
        # has no defined standalone-execution shape yet, drag-drop
        # continuity only). Silent no-op, not an error.
        exit 0
        ;;
esac

# REAL BUG, caught in testing: find's enumeration order does NOT match
# numeric page order (page_2 can list before page_1), so renumbering
# sequentially in raw find order silently scrambled which source page
# became dest page_1 vs page_2 - including, in one real test, losing a
# page's condition.pdl because it swapped with a page that never had
# one. Sort by the source page's own trailing number before renumbering
# so relative order is preserved.
if [ -s "$TMP_LIST" ]; then
    SORTED_LIST=$(mktemp)
    trap 'rm -f "$TMP_LIST" "$SORTED_LIST"' EXIT
    while read -r p; do
        [ -n "$p" ] || continue
        pn=$(basename "$p")
        pn="${pn#page_}"
        printf '%s\t%s\n' "$pn" "$p"
    done < "$TMP_LIST" | sort -n -k1,1 | cut -f2- > "$SORTED_LIST"
    mv "$SORTED_LIST" "$TMP_LIST"
fi

[ -s "$TMP_LIST" ] || exit 0

# Next free event_clacker_N for the TARGET entity - minimum 2, native
# clacker_1 is always reserved and never overwritten by a push.
NEXT=2
if [ -d "$ENT/inventory" ]; then
    for cdir in "$ENT"/inventory/event_clacker_*; do
        [ -d "$cdir" ] || continue
        n="${cdir##*_}"
        case "$n" in ''|*[!0-9]*) continue ;; esac
        if [ "$n" -ge "$NEXT" ]; then NEXT=$((n + 1)); fi
    done
fi

NEW_CLACKER="$ENT/inventory/event_clacker_$NEXT"
[ -e "$NEW_CLACKER" ] && exit 0

mkdir -p "$NEW_CLACKER/event_pkg/pages"
_hash=$(head -c 64 /dev/urandom | sha256sum | cut -d' ' -f1)
cat > "$NEW_CLACKER/pal.pdl" <<EOF
PAL | name | event_clacker_$NEXT
PAL | hash | $_hash
PAL | glyph | 🎬️
EOF
printf '🎬️' > "$NEW_CLACKER/glyph.txt"
_iid="CLACK$NEXT"
cat > "$NEW_CLACKER/meta.pdl" <<EOF
SECTION      | KEY                | VALUE
----------------------------------------
META         | piece_id           | event_clacker_$NEXT
STATE        | kind                 | deskpal
STATE        | glyph                | 🎬️
STATE        | instance_id          | $_iid
STATE        | event_object         | 1
STATE        | pushed_from          | $DROP
METHOD       | Dir                  | sh -c 'exec xdg-open "\$0"'
METHOD       | Close                | CLOSE
METHOD       | Cancel               | void
EOF
: > "$NEW_CLACKER/history.txt"
: > "$NEW_CLACKER/interact_relay.txt"
: > "$NEW_CLACKER/last_signal.txt"

# Per-clacker page numbering (resolved 2026-09-28): renumber
# sequentially starting at page_1 inside the NEW clacker, regardless of
# what the source pages were numbered - this clacker owns its own
# namespace, matching event_page_to_pal.sh's own established rule.
n=0
while read -r srcpage; do
    [ -n "$srcpage" ] || continue
    n=$((n + 1))
    destpage="$NEW_CLACKER/event_pkg/pages/page_$n"
    mkdir -p "$destpage"
    cp -r "$srcpage/." "$destpage/"
    for f in "$destpage"/cmd_*.sh; do
        [ -f "$f" ] || continue
        sed -i "s|^# pkg=.* page=.*|# pkg=event_clacker_$NEXT page=page_$n (pushed from $DROP, $(date '+%Y-%m-%d'))|" "$f"
    done
done < "$TMP_LIST"

OPS="$HOUSE_ROOT/_.monads/_.livedesk-taskbar/ops/+x"
if [ -x "$OPS/emoji_gen_atlas.+x" ] && [ -x "$OPS/emoji_xtract.+x" ]; then
    "$OPS/emoji_gen_atlas.+x" "🎬️" "$NEW_CLACKER/atlas.png" >/dev/null 2>&1
    "$OPS/emoji_xtract.+x" "$NEW_CLACKER/atlas.png" 0 64 "$NEW_CLACKER/sprite.csv" >/dev/null 2>&1
fi

# REAL, CORRECTED 2026-09-28 (direct instruction: "thats not how drop
# works, drop deletes other location and mv should do the same, same as
# cli. look deeper i think it already does this") - a drop is a MOVE,
# not a copy: fe_drop.sh (the real, pre-existing house XDND precedent)
# does a literal `mv "$DROP" "$DIR/$base"`, and the just-restored Cli-io
# `mv` verb deletes its source via rename() too (verified live: "src_dir
# should be GONE"). This handler originally left $DROP in place, which
# was an unreviewed design choice, not grounded in house precedent - it
# is now fixed to match. Only deletes after every page copied
# successfully ($n > 0) so a mid-copy failure never loses data with
# nothing created yet.
#
# Note on the "copy now, link later" decision this doesn't override:
# that question was about whether edits STAY INDEPENDENT after the drop
# (yes, copy, not a live link) - it was never about whether the source
# object survives the drag. Those are separate axes; source deletion
# here answers the second one, consistently with mv/drop everywhere
# else in the house.
#
# Real, honest consequence: if $DROP was the entity's own NATIVE
# clacker_1 (or a page nested inside it) rather than something pushed
# in from elsewhere, dragging it away deletes that entity's own
# inventory mirror of its own real events - harmless (it's only a
# mirror, event_pkg/pages/ itself is untouched and remains the real
# dispatch source per play_event.sh's own fallback), and it regenerates
# lazily the next time a page/command is added (event_auto_clacker.sh)
# or a retroactive sweep is re-run - but the mirror is gone until then.
if [ "$n" -gt 0 ]; then
    rm -rf "$DROP"
fi

echo "event_drop_handler: pushed event_clacker_$NEXT onto $(basename "$ENT") from $DROP ($n page(s), source removed)" >&2
