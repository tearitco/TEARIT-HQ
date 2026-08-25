#!/bin/bash
# button.sh - test-harn-ed-app: level-2 (§36.6) black-box harness for
# @.apps/text-editor-xyz (editor + file-menu widget, combined).
#
# Modular by design (direct user instruction, 2026-07-30): each
# scenario is independently runnable, neither depends on the other
# having run first or leaves state the other assumes.
ACTION="${1:-help}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

case "$ACTION" in
    compile|c|build)
        echo "test-harn-ed-app: checking dependencies..."
        APP_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
        HOUSE="$(cd "$APP_DIR/../.." && pwd)"
        EDITOR_DIR="$(ls -d "$HOUSE"/102.*editor* 2>/dev/null | head -1)"
        FM_DIR="$(ls -d "$HOUSE"/\&.widgits/file-menu* 2>/dev/null | head -1)"
        [ -x "$APP_DIR/button.sh" ] && echo "OK   $APP_DIR/button.sh" || echo "MISS $APP_DIR/button.sh"
        [ -x "$EDITOR_DIR/ops/+x/editor_widget_cmds.+x" ] && echo "OK   editor_widget_cmds.+x" || echo "MISS editor_widget_cmds.+x — run $EDITOR_DIR/button.sh compile"
        [ -x "$FM_DIR/ops/+x/fm_menu_input.+x" ] && echo "OK   fm_menu_input.+x" || echo "MISS fm_menu_input.+x — run $FM_DIR/button.sh compile"
        [ -x "$HOUSE/EMERGENCY_KILL.sh" ] || echo "WARN EMERGENCY_KILL.sh not executable — chmod +x it"
        echo "(pure bash scenarios — nothing here to compile)"
        ;;
    demo-load)
        bash "$SCRIPT_DIR/scenarios/demo_load.sh"
        ;;
    demo-save)
        bash "$SCRIPT_DIR/scenarios/demo_save.sh"
        ;;
    demo|demo-both)
        rc=0
        bash "$SCRIPT_DIR/scenarios/demo_load.sh" || rc=1
        bash "$SCRIPT_DIR/scenarios/demo_save.sh" || rc=1
        exit "$rc"
        ;;
    kill|k|stop)
        HOUSE="$(cd "$SCRIPT_DIR/../../.." && pwd)"
        sh "$HOUSE/EMERGENCY_KILL.sh"
        ;;
    help|h|-h|--help)
        echo "test-harn-ed-app — level-2 harness for @.apps/text-editor-xyz"
        echo ""
        echo "Proves editor+file-menu work together through REAL entry"
        echo "points and REAL key injection (xyzos-standards §36.6),"
        echo "never direct op calls or interact_relay.txt shortcuts."
        echo ""
        echo "Usage: ./button.sh <action>"
        echo "  compile             - Check dependencies"
        echo "  demo-load           - LOAD only (standalone, modular)"
        echo "  demo-save           - real EDIT + SAVE_AS only (standalone, modular)"
        echo "  demo | demo-both    - both, sequentially"
        echo "  kill                - EMERGENCY_KILL.sh (see PITFALL 55)"
        echo "  help                - this help"
        ;;
    *)
        echo "Unknown: $ACTION (try ./button.sh help)"
        exit 1
        ;;
esac
