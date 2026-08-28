#!/bin/sh
# palettes_menu.sh - palette CATEGORY windows (v3, 2026-08-25)
# ============================================================
# REAL TPMOS-compliant rebuild (au11-hq/TPMOS-COMPLIANCE-DEBT.md's own
# standing rule: don't patch around a missing manager, build the real
# thing, same shape as its own proven siblings - stats_hq_manager.c,
# bookmarks_manager.c). This script used to `printf` raw .chtpm XML
# directly (compose_emojis()/compose_elements()/emit_tiles_matrix()) and
# regenerate the whole window file on every launch - that's gone. The
# real split now:
#   - THIS script: file ops only (place a tile, provision a stub category
#     doc-link once). Never touches tile-matrix XML.
#   - palettes_manager.c (real, compiled, independently-testable binary,
#     `<module src="..." args="<category>"/>` in each real category's
#     chtpm, launched by the renderer): owns reading the emoji pallet /
#     chemistry CSV and publishing palettes-<category>_state.txt,
#     including sprite pre-generation.
#   - khtpm_entity_menu_render.c: reads that state file, injects real
#     <row>/<button> tile grids at runtime (dbhq_inject_palette_tiles()).
#
# Real tiles: emojis (starter grid, palettes-emojis.chtpm) and elements
# (all real compounds from chemistry_tiles_expanded CSV, palettes-
# elements.chtpm) - both STATIC, checked-in files now, never regenerated.
# Categories whose picker isn't built yet get a one-time-provisioned
# stub doc-link (palettes-stub.template.chtpm) instead of lying dead.
# fo-menu-sys pitfall #26: interactive rows are <button>, never <item>.

set -eu

SELF_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
HOUSE_DEFAULT=$(CDPATH= cd -- "$SELF_DIR/../.." && pwd)
DESIGN_DOC="$HOUSE_DEFAULT/#.ref/menu/palletes/pallette-design.txt"
TP_OPS="$HOUSE_DEFAULT/&.widgits/tile-picker/ops/+x"
STATE_DIR="$SELF_DIR/state"
DESK_DIR="$HOUSE_DEFAULT/#.desktop"

log() { mkdir -p "$SELF_DIR/audit"; printf '%s %s\n' "$(date '+%Y-%m-%d %H:%M:%S')" "$*" >> "$SELF_DIR/audit/palettes.log"; }

place() {  # place <glyph> - real desk placement, tile-picker's own chain
    _g="$1"
    mkdir -p "$STATE_DIR" "$DESK_DIR/tiles"
    "$TP_OPS/tp_set_brush.+x" "$STATE_DIR" "$_g" >/dev/null 2>&1 || true
    "$TP_OPS/tp_place_desktop.+x" "$STATE_DIR" "$DESK_DIR" >/dev/null 2>&1 || true
    log "placed glyph $_g onto $DESK_DIR/tiles/"
}

# REAL FIX 2026-08-27 (direct instruction: "flag hardcoded things...
# they are supposed to use generic .pdl read functions whenever
# possible") - this used to be a hardcoded bash `case` statement
# duplicating pallets.pdl's own real LABEL column verbatim (a real,
# pre-existing violation of this exact standard - a NEW category added
# to pallets.pdl would silently fall through to the "*) return 1" case
# here and never get a real title, even though the label data already
# existed). Now reads the real LABEL column directly, same real
# IFS='|' parse shape list_cats() (below) already uses for pallets.pdl -
# zero hardcoded category names anywhere in this function.
title_for() {
    _key="$1"
    # POSIX-sh-safe (no process substitution/bashism - this script is
    # real #!/bin/sh): capture the loop's own stdout via command
    # substitution instead of piping into the while loop, which would
    # otherwise run it in a subshell under dash and lose the result.
    _found=$(grep '^CATEGORY' "$SELF_DIR/pallets.pdl" | while IFS='|' read -r _sec _k _label _rest; do
        _k=$(printf '%s' "$_k" | sed 's/^[[:space:]]*//;s/[[:space:]]*$//')
        if [ "$_k" = "$_key" ]; then
            printf '%s' "$_label" | sed 's/^[[:space:]]*//;s/[[:space:]]*$//'
            break
        fi
    done)
    [ -n "$_found" ] || return 1
    printf 'palettes: %s' "$_found"
}

# provision_stub <key> - copies the static stub template into
# palettes-<key>.chtpm/.css ONLY if it doesn't already exist (a one-time
# scaffold, not a per-launch regeneration - same real distinction
# bookmarks' own provision_bookmarks() draws). Real categories (emojis/
# elements) are already checked-in static files and never go through
# this path.
provision_stub() {
    _k="$1"
    _t=$(title_for "$_k") || _t="palettes: $_k"
    if [ ! -f "$SELF_DIR/palettes-$_k.chtpm" ]; then
        _t_esc=$(printf '%s' "$_t" | sed 's/[&\]/\\&/g')
        _doc_esc=$(printf '%s' "$DESIGN_DOC" | sed 's/[&\]/\\&/g')
        sed \
            -e "s#__TITLE__#$_t_esc#g" \
            -e "s#__DOC__#$_doc_esc#g" \
            "$SELF_DIR/palettes-stub.template.chtpm" > "$SELF_DIR/palettes-$_k.chtpm"
    fi
    if [ ! -f "$SELF_DIR/palettes-$_k.css" ]; then
        cp "$SELF_DIR/palettes-emojis.css" "$SELF_DIR/palettes-$_k.css"
    fi
}

launch_cat() {  # launch_cat <house> <key>
    _h="$1"; _k="$2"
    _CHTPM="$SELF_DIR/palettes-$_k.chtpm"
    if [ ! -f "$_CHTPM" ]; then
        provision_stub "$_k"
    fi
    for p in /proc/[0-9]*; do
        [ "$p" = "/proc/$$" ] && continue
        _cl=$(cat "$p/cmdline" 2>/dev/null | tr '\0' ' ') || continue
        case "$_cl" in *"$_CHTPM"*) kill "${p#/proc/}" 2>/dev/null || true ;; esac
    done
    sleep 0.2
    BIN=$(echo "$_h"/*.monads/*.livedesk-taskbar/ops/+x/khtpm_entity_menu_render.+x)
    if [ ! -x "$BIN" ]; then
        (cd "$_h/*.monads/*.livedesk-taskbar/ops" && sh build_entity_menu.sh) || true
    fi
    [ -x "$BIN" ] || { echo "palettes: renderer missing: $BIN" >&2; exit 1; }
    MGRBIN="$_h/*.monads/*.livedesk-taskbar/ops/+x/palettes_manager.+x"
    if [ ! -x "$MGRBIN" ]; then
        (cd "$_h/*.monads/*.livedesk-taskbar/ops" && sh build_palettes_manager.sh) || true
    fi
    log "category window launched (real manager, TPMOS-compliant): $_k"
    exec "$BIN" "$_h" "$_CHTPM"
}

list_cats() {
    grep '^CATEGORY' "$SELF_DIR/pallets.pdl" | while IFS='|' read -r _ k _rest; do
        k=$(printf '%s' "$k" | sed 's/^[[:space:]]*//;s/[[:space:]]*$//')
        [ -n "$k" ] && printf '%s\n' "$k"
    done
}

# set_rmmv <field> <pkg_dir> <value> - real 2026-08-27/28 (tile-picker
# UI pass): writes rmmv_active.txt into the SAME live package dir
# palettes_manager.c's own publish_rmmv() reads it from (passed as a
# literal onclick arg by the renderer, which already has g_package_dir -
# no new IPC channel needed). "tab" (a real sheet LETTER, A-E - 2026-08-28
# rename from "category", since a tab click only specifies a LETTER, the
# concrete a1/a2/... category is resolved manager-side from whatever
# real file backs it) and "tileset" (a real filename PREFIX, e.g.
# "World"/"Inside" - 2026-08-28: no longer a registry key, matches a
# real "<tileset>_<suffix>.png" file directly). Merges the OTHER
# field's current value in so clicking a tab doesn't stomp the active
# tileset choice and vice versa - a plain overwrite would silently
# reset whichever field the user isn't currently touching.
set_rmmv() {
    _field="$1"; _pkg="$2"; _val="$3"
    _f="$_pkg/rmmv_active.txt"
    mkdir -p "$_pkg"
    if [ "$_field" = "tab" ]; then _other_field="tileset"; else _other_field="tab"; fi
    _other_val=""
    if [ -f "$_f" ]; then
        _other_val=$(grep "^${_other_field}=" "$_f" | head -1 | cut -d= -f2-)
    fi
    {
        [ -n "$_other_val" ] && printf '%s=%s\n' "$_other_field" "$_other_val"
        printf '%s=%s\n' "$_field" "$_val"
    } > "$_f"
    log "rmmv active $_field set to '$_val' in $_pkg"
}

case "${1:-}" in
    place)             shift; place "$@"; exit 0 ;;
    list)              list_cats; exit 0 ;;
    set-rmmv-tab)      shift; set_rmmv tab "$1" "$2"; exit 0 ;;
    set-rmmv-tileset)  shift; set_rmmv tileset "$1" "$2"; exit 0 ;;
esac

# Arg forms:
#   palettes_menu.sh <category>            (what the tb PDL rows use -
#                           house defaults to this script's own root)
#   palettes_menu.sh <house_root> <category>
HOUSE="$HOUSE_DEFAULT"
KEY="${1:-}"
if [ -n "$KEY" ] && ! title_for "$KEY" >/dev/null 2>&1; then
    HOUSE="$KEY"
    KEY="${2:-}"
fi
[ -n "$KEY" ] || { echo "usage: palettes_menu.sh [<house_root>] <category>" >&2; exit 1; }
launch_cat "$HOUSE" "$KEY"
