#!/bin/bash
# button.sh - launcher for tomom hand-tuning dashboard
#
# Uses the shared khtpm_core_render.+x renderer with the tomom_dashboard.chtpm
# layout. The tomom_dashboard.py manager process generates the dynamic data
# projection file (tomom_dashboard_data.txt) that the .chtpm layout references.
#
# The .pal module (tomom_dashboard_module.pal) runs in prisc+x VM, refreshing
# the data projection and dispatching key events from interact_relay.txt.

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ACTION="${1:-help}"

case "$ACTION" in
    compile|c|build)
        echo "Compiling tomom_dashboard.py check..."
        python3 -m py_compile "$SCRIPT_DIR/harnec/tomom_dashboard.py" && echo "OK tomom_dashboard.py" || echo "FAIL tomom_dashboard.py"
        python3 -m py_compile "$SCRIPT_DIR/harnec/concept_bank_bridge.py" && echo "OK concept_bank_bridge.py" || echo "FAIL concept_bank_bridge.py"
        echo "tomom_dashboard.chtpm is plain text — no compilation needed"
        ;;
    run|r)
        echo "Starting tomom hand-tuning dashboard..."
        python3 "$SCRIPT_DIR/harnec/tomom_dashboard.py" --refresh
        echo "Dashboard data written to pieces/chtpm/layouts/tomom_dashboard_data.txt"
        # Render via shared house renderer
        HOUSE_ROOT="$(cd "$SCRIPT_DIR/../../../../../../.." && pwd)"
        RENDERER="$HOUSE_ROOT/x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00/_.monads/_.livedesk-taskbar/ops/+x/khtpm_core_render.+x"
        if [ -x "$RENDERER" ]; then
            echo "Rendering via shared khtpm_core_render..."
            "$RENDERER" "$HOUSE_ROOT" "$SCRIPT_DIR/pieces/chtpm/layouts/tomom_dashboard.chtpm" 0 0 &
        else
            echo "Note: khtpm_core_render.+x not found at $RENDERER"
        fi
        ;;
    watch|w)
        python3 "$SCRIPT_DIR/harnec/tomom_dashboard.py" --watch
        ;;
    handtune|ht)
        python3 "$SCRIPT_DIR/harnec/run_handtune.py" \
            --curriculum "${2:-Mathematics}" \
            --prompt "${3:-Explain quadratic equations}" \
            --behavior "${4:-Boost equations and variables weights}"
        ;;
    kill|k)
        pkill -f "tomom_dashboard.py --watch" 2>/dev/null
        pkill -f "tomom_dashboard.py --generate" 2>/dev/null
        echo "Stopped tomom dashboard processes"
        ;;
    *)
        echo "Usage: $0 {compile|run|watch|handtune|kill}"
        echo "  compile  - verify Python scripts compile"
        echo "  run      - generate dashboard data and render"
        echo "  watch    - live-updating dashboard (refreshes every 5s)"
        echo "  handtune - run full hand-tuning cycle for a curriculum"
        echo "  kill     - stop running dashboard processes"
        ;;
esac