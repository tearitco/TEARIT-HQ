#!/bin/bash
# button.sh - hard-vvar-agent-Q0000 (vvarware) launcher. House-standard
# monad entry point (matches *.monads/*.book-stack/button.sh and
# *.monads/*.muchi-pet/button.sh).
#
#   run|start|brain  - start the brain loop + open the self entity window
#   window|w         - same as run
#   headless         - start the brain loop only
#   tool-chat        - chat with the tooled llama path
#   read|ledger      - open the master ledger
#   kill|k|stop      - stop the brain loop
#   check|verify     - verify binaries/paths/model
#   help             - this help
ACTION="${1:-help}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
HOUSE_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"

BRAIN="$SCRIPT_DIR/pieces/brain/run.sh"
LEDGER="$SCRIPT_DIR/pieces/brain/master_ledger.txt"
STATE="$SCRIPT_DIR/pieces/brain/state.txt"
TPWIN="$HOUSE_DIR/*.monads/*.livedesk-taskbar/ops/+x/tp_desktop_window_rgb.+x"
ENTITY_DIR="$SCRIPT_DIR/entities/self"
ASSET_DIR="$ENTITY_DIR/assets"
# REAL 2026-08-08: robot portrait source moved off the Desktop into the
# shared #.ASSETS_IN_USE folder (user is cleaning up /home/no/Desktop/).
ROBOT_SRC="/home/no/Desktop/🤖️🪤️🏠️/#.ASSETS_IN_USE/_.qoo+.png"
ROBOT_DST="$ASSET_DIR/robot.png"

brain_up() {
    # escape regex specials - monad paths contain literal '*' globs
    local re
    re=$(printf '%s' "$BRAIN" | sed 's/[][{}.*+?^$|\\]/\\&/g')
    pgrep -f "$re" >/dev/null 2>&1
}

start_brain() {
    if brain_up; then
        echo "vvarware brain already running"
        return 0
    fi
    setsid nohup bash "$BRAIN" >/dev/null 2>&1 &
    disown
    sleep 1
    echo "vvarware brain started"
}

ensure_self_entity() {
    mkdir -p "$ASSET_DIR"
    if [ -f "$ROBOT_SRC" ]; then
        cp "$ROBOT_SRC" "$ROBOT_DST"
    fi
    if [ ! -f "$ENTITY_DIR/glyph.txt" ]; then
        printf '%s' '🤖' > "$ENTITY_DIR/glyph.txt"
    fi
    cat > "$ENTITY_DIR/asset.pal" <<EOF
asset_path=robot.png
glyph=🤖
EOF
    if [ ! -f "$ENTITY_DIR/meta.pdl" ]; then
        {
            echo "SECTION      | KEY                | VALUE"
            echo "----------------------------------------"
            echo "META         | piece_id           | self"
            echo "STATE        | kind               | agent"
            echo "STATE        | glyph              | 🤖"
            grep '^METHOD' "$SCRIPT_DIR/methods.pdl"
        } > "$ENTITY_DIR/meta.pdl"
    fi
    while IFS= read -r line; do
        case "$line" in
            METHOD*)
                key="$(printf '%s' "$line" | awk -F'|' '{gsub(/^[ \t]+|[ \t]+$/, "", $2); print $2}')"
                if ! grep -q "^METHOD[[:space:]]*|[[:space:]]*$key[[:space:]]*|" "$ENTITY_DIR/meta.pdl" 2>/dev/null; then
                    printf '%s\n' "$line" >> "$ENTITY_DIR/meta.pdl"
                fi
                ;;
        esac
    done < "$SCRIPT_DIR/methods.pdl"
    if [ ! -f "$ENTITY_DIR/desktop_pos.txt" ]; then
        printf 'x=60\ny=80\n' > "$ENTITY_DIR/desktop_pos.txt"
    fi
}

case "$ACTION" in
    run|r|start|brain|b|window|w)
        start_brain
        ensure_self_entity
        if [ -x "$TPWIN" ]; then
            setsid nohup "$TPWIN" "$ENTITY_DIR" >/dev/null 2>&1 &
            disown
            echo "vvarware window started"
        else
            echo "MISSING tp_desktop_window binary: $TPWIN"
        fi
        ;;
    headless)
        start_brain
        ;;
    tool-chat)
        shift || true
        ensure_self_entity
        bash "$SCRIPT_DIR/pieces/features/tool_chat.sh" "$@"
        ;;
    read|ledger|log)
        if [ -f "$LEDGER" ]; then
            exec gedit "$LEDGER"
        else
            echo "no ledger yet: $LEDGER"
        fi
        ;;
    kill|k|stop)
        local re
        re=$(printf '%s' "$BRAIN" | sed 's/[][{}.*+?^$|\\]/\\&/g')
        pkill -f "$re" 2>/dev/null
        echo "done"
        ;;
    check|verify)
        echo "vvarware monad check"
        [ -f "$BRAIN" ] && echo "OK   brain loop" || echo "MISSING brain loop"
        for f in battle move learn build_self charge buy_batteries chat tool_chat write_other_bots; do
            [ -f "$SCRIPT_DIR/pieces/features/$f.sh" ] && echo "OK   feature $f" || echo "MISSING feature $f"
        done
        for o in json_parser cmd_exec file_ops list_dir; do
            [ -x "$SCRIPT_DIR/ops/+x/$o.+x" ] && echo "OK   op $o" || echo "MISSING op $o"
        done
        echo "--- model ---"
        model="$(grep '^brain_model=' "$STATE" 2>/dev/null | cut -d= -f2)"
        echo "brain_model: ${model:-unset}"
        echo "brain_url:   $(grep '^brain_url=' "$STATE" 2>/dev/null | cut -d= -f2)"
        echo "--- ledger ---"
        [ -f "$LEDGER" ] && wc -l < "$LEDGER" | xargs echo "  lines:" || echo "  no ledger"
        ;;
    help|h|-h|--help|*)
        cat <<'EOF'
vvarware (hard-vvar-agent-Q0000) launcher

  sh button.sh run        # brain loop + the self entity desktop window
  sh button.sh headless   # brain loop only
  sh button.sh read       # open the master ledger
  sh button.sh kill       # stop the brain loop
  sh button.sh check      # verify binaries/paths/model
  sh button.sh help       # this help
EOF
        ;;
esac
