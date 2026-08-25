#!/bin/bash
# button.sh - independent start script for asa alone. Same "every desk-
# created event/entity gets its own button.sh" convention documented in
# &.widgits/event-editor/EVENT_EDITOR_FOR_AOMO_AND_HIKIKOMORAI.md §0.5.
NAME="asa"
GLYPH="👨"
# Grid cell (GRID_CELL_PX=80, same shared desktop grid every live entity
# uses - see TILE_PICKER_DESIGN.md §2.3): asa spawns at column 3, ava
# (its own button.sh) spawns at column 6 - direct instruction ("they
# start in same place, space them out").
GRID_X=3
GRID_Y=3

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
HOUSE_ROOT="$(cd "$SCRIPT_DIR/../../../.." && pwd)"
TP="$HOUSE_ROOT/*.monads/*.livedesk-taskbar/ops"
DESK="$HOUSE_ROOT/#.desktop"
ACTION="${1:-run}"
PKG="$DESK/entities/$NAME"

ensure_package() {
    mkdir -p "$PKG"
    if [ ! -f "$PKG/instance_id.txt" ]; then
        INSTANCE_ID=$(tr -dc 'A-Z0-9' < /dev/urandom | head -c4)
        echo "$INSTANCE_ID" > "$PKG/instance_id.txt"
    fi
    INSTANCE_ID=$(cat "$PKG/instance_id.txt" 2>/dev/null || echo "0000")
    echo "$GLYPH" > "$PKG/glyph.txt"
    # REAL FIX 2026-08-04, direct instruction ("entity methods aren't
    # hardcoded, they should be read from a .txt/.pdl that user can
    # customize"): METHOD rows are NOT hardcoded in this script - they
    # live in $NAME's own real, user-editable methods.pdl (right next to
    # this button.sh). This function only SEEDS the live package's
    # meta.pdl from methods.pdl the FIRST time the package doesn't exist
    # yet - once meta.pdl is real, later runs never touch it again, so
    # any customization (editing methods.pdl and re-seeding, or editing
    # the live meta.pdl directly) genuinely persists across restarts
    # instead of being silently overwritten every run (the real bug this
    # replaces - the previous version wrote a hardcoded heredoc into
    # meta.pdl on EVERY run, clobbering any edit).
    if [ ! -f "$PKG/meta.pdl" ]; then
        {
            echo "SECTION      | KEY                | VALUE"
            echo "----------------------------------------"
            echo "META         | piece_id           | $NAME"
            echo "STATE        | kind                 | deskpal"
            echo "STATE        | glyph                | $GLYPH"
            echo "STATE        | created_at           | $(date +%s)"
            echo "STATE        | instance_id          | $INSTANCE_ID"
            if [ -f "$SCRIPT_DIR/methods.pdl" ]; then
                grep "^METHOD" "$SCRIPT_DIR/methods.pdl"
            else
                echo "METHOD       | Close                | CLOSE"
                echo "METHOD       | Cancel               | CANCEL"
            fi
        } > "$PKG/meta.pdl"
    fi
    # Grid-aligned spawn point (see GRID_X/GRID_Y above) - real emoji
    # texture, same emoji_gen_atlas -> emoji_xtract pipeline
    # tp_place_desktop.c itself uses (TILE_PICKER_DESIGN.md §2.4).
    if [ ! -f "$PKG/desktop_pos.txt" ]; then
        local px=$((GRID_X * 80))
        local py=$((GRID_Y * 80))
        printf 'x=%d\ny=%d\n' "$px" "$py" > "$PKG/desktop_pos.txt"
    fi
    if [ ! -f "$PKG/sprite.csv" ] && [ -x "$TP/+x/emoji_gen_atlas.+x" ] && [ -x "$TP/+x/emoji_xtract.+x" ]; then
        "$TP/+x/emoji_gen_atlas.+x" "$GLYPH" "$PKG/atlas.png" >/dev/null 2>&1
        "$TP/+x/emoji_xtract.+x" "$PKG/atlas.png" 0 64 "$PKG/sprite.csv" >/dev/null 2>&1
    fi
    # REAL, NEW 2026-08-04, direct instruction ("allow user editing of
    # asset... place asset in asset folder of entity, .pal specifies
    # emoji other than default, or path of asset"): $NAME's own real,
    # user-editable asset.pal (right next to this button.sh) - copied
    # into the live package ONCE (same seed-don't-clobber rule as
    # meta.pdl above), where tp_desktop_window.c's own
    # apply_asset_override() reads it at every window startup. Editing
    # this file's own asset_path=/glyph= line and relaunching the window
    # picks up the change. See &.widgits/tile-picker/ops/
    # tp_desktop_window.c and tp_asset_to_sprite.c.
    if [ ! -f "$PKG/asset.pal" ] && [ -f "$SCRIPT_DIR/asset.pal" ]; then
        cp "$SCRIPT_DIR/asset.pal" "$PKG/asset.pal"
    fi
}

read_auto_open() {
    [ -f "$SCRIPT_DIR/config.pdl" ] || return 1
    awk -F'|' '
        $1 ~ /^SECTION[[:space:]]*$/ {
            gsub(/[[:space:]]+/, "", $2);
            gsub(/[[:space:]]+/, "", $3);
            if ($2 == "auto-open-folder") { print $3; exit }
        }
    ' "$SCRIPT_DIR/config.pdl" 2>/dev/null || return 1
}

case "$ACTION" in
    run|r|start)
        ensure_package
        if [ -x "$TP/+x/tp_desktop_window_rgb.+x" ]; then
            setsid nohup "$TP/+x/tp_desktop_window_rgb.+x" "$PKG" >/dev/null 2>&1 < /dev/null &
            [ -n "$BASH_VERSION" ] && disown
            echo "$NAME spawned: $PKG"
        else
            echo "MISSING: $TP/+x/tp_desktop_window_rgb.+x (run *.monads/*.livedesk-taskbar/ops/build_khtpm_strip.sh first)"
        fi
        if [ "$(read_auto_open 2>/dev/null)" = "1" ] && command -v xdg-open >/dev/null 2>&1; then
            (xdg-open "$SCRIPT_DIR" >/dev/null 2>&1 &)
        fi
        ;;
    kill|k|stop)
        pkill -f "tp_desktop_window.+x $PKG" 2>/dev/null
        echo "done"
        ;;
    check|verify)
        [ -x "$TP/+x/tp_desktop_window_rgb.+x" ] && echo "OK   tp_desktop_window_rgb.+x" || echo "MISSING tp_desktop_window_rgb.+x"
        [ -d "$SCRIPT_DIR" ] && echo "OK   $SCRIPT_DIR" || echo "MISSING $SCRIPT_DIR"
        ;;
    help|h|-h|--help|*)
        echo "$NAME's own independent start script — run | kill | check"
        ;;
esac
