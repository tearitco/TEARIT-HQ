#!/bin/bash
# button.sh - independent start script for dog alone. Same real
# pattern as @.apps/asa-&-ava/pieces/asa/button.sh (reuse, don't
# reinvent) - see &.widgits/event-editor/EVENT_EDITOR_FOR_AOMO_AND_HIKIKOMORAI.md §0.5.
NAME="chicken"
GLYPH="🐔"
GRID_X=15
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
    if [ ! -f "$PKG/meta.pdl" ]; then
        {
            echo "SECTION      | KEY                | VALUE"
            echo "----------------------------------------"
            echo "META         | piece_id           | $NAME"
            echo "STATE        | kind                 | pet"
            echo "STATE        | glyph                | $GLYPH"
            echo "STATE        | created_at           | $(date +%s)"
            echo "STATE        | instance_id          | $INSTANCE_ID"
            if [ -f "$SCRIPT_DIR/methods.pdl" ]; then
                grep "^METHOD" "$SCRIPT_DIR/methods.pdl"
            else
                echo "METHOD       | Close                | CLOSE"
                echo "METHOD       | Cancel               | void"
            fi
        } > "$PKG/meta.pdl"
    fi
    if [ ! -f "$PKG/desktop_pos.txt" ]; then
        local px=$((GRID_X * 80))
        local py=$((GRID_Y * 80))
        printf 'x=%d\ny=%d\n' "$px" "$py" > "$PKG/desktop_pos.txt"
    fi
    if [ ! -f "$PKG/sprite.csv" ] && [ -x "$TP/+x/emoji_gen_atlas.+x" ] && [ -x "$TP/+x/emoji_xtract.+x" ]; then
        "$TP/+x/emoji_gen_atlas.+x" "$GLYPH" "$PKG/atlas.png" >/dev/null 2>&1
        "$TP/+x/emoji_xtract.+x" "$PKG/atlas.png" 0 64 "$PKG/sprite.csv" >/dev/null 2>&1
    fi
    if [ ! -f "$PKG/asset.pal" ] && [ -f "$SCRIPT_DIR/asset.pal" ]; then
        cp "$SCRIPT_DIR/asset.pal" "$PKG/asset.pal"
    fi
}

case "$ACTION" in
    run|r|start)
        ensure_package
        if [ -x "$TP/+x/khtpm_core_render.+x" ]; then
            setsid nohup "$TP/+x/khtpm_core_render.+x" "$PKG" >/dev/null 2>&1 < /dev/null &
            disown
            echo "$NAME spawned: $PKG"
        else
            echo "MISSING: $TP/+x/khtpm_core_render.+x (run *.monads/*.livedesk-taskbar/ops/build_khtpm_strip.sh first)"
        fi
        ;;
    kill|k|stop)
        pkill -f "khtpm_core_render.+x $PKG" 2>/dev/null
        echo "done"
        ;;
    check|verify)
        [ -x "$TP/+x/khtpm_core_render.+x" ] && echo "OK   khtpm_core_render.+x" || echo "MISSING khtpm_core_render.+x"
        [ -d "$SCRIPT_DIR" ] && echo "OK   $SCRIPT_DIR" || echo "MISSING $SCRIPT_DIR"
        ;;
    help|h|-h|--help|*)
        echo "$NAME's own independent start script — run | kill | check"
        ;;
esac
