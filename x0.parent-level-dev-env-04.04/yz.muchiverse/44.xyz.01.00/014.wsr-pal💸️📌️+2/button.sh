#!/bin/bash
# button.sh - launcher for wsr-pal.
#
# "run" is the REAL, interactive, human-playable game - same 3-process
# shape (keyboard_input owns the tty / prisc+x dispatches / renderer
# draws) already proven in muchi-pal-chat's own button.sh. Per direct
# instruction ("human testability above all things"), this is now the
# PRIMARY way to use this project - the old single-corp-ORB CLI test
# actions (tick/step/choose/reset) are renamed test-* below, kept for
# quick non-interactive op-level testing, not the main entry point
# anymore.
PAL_MODE=0
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# Parse --pal flag from any position, find the action verb
ACTION="help"
for arg in "$@"; do
    case "$arg" in
        --pal) PAL_MODE=1 ;;
        *) ACTION="$arg" ;;
    esac
done

case "$ACTION" in
    compile|c|build)
        bash "$SCRIPT_DIR/scripts/build.sh"
        gcc -o system/orchestrator system/orchestrator.c 2>/dev/null && echo "OK   system/orchestrator" || echo "SKIP system/orchestrator"
        ;;
    run|r|start)
        cd "$SCRIPT_DIR"
        if [ ! -x "system/orchestrator" ]; then
            echo "Compiling orchestrator..."
            gcc -o system/orchestrator system/orchestrator.c 2>/dev/null
        fi
        if [ "$PAL_MODE" -eq 1 ]; then
            echo "=== wsr-pal Launcher (PAL Mode) ==="
            export PAL_LAYOUT="pieces/chtpm/layouts/wsr_main_menu.chtpm"
        else
            echo "=== wsr-pal Launcher (C Mode) ==="
            export PAL_LAYOUT=""
        fi
        exec "$SCRIPT_DIR/system/orchestrator"
        ;;
    kill|k|stop)
        pkill -f "system/keyboard_input" 2>/dev/null
        pkill -f "system/renderer" 2>/dev/null
        pkill -f "system/prisc\+x" 2>/dev/null
        pkill -f "system/chtpm_parser_pal" 2>/dev/null
        pkill -f "system/chtpm_rgb_render" 2>/dev/null
        pkill -f "system/gl_mirror" 2>/dev/null
        bash "$SCRIPT_DIR/pieces/os/kill_all.sh"
        echo "done"
        ;;
    sim-key)
        # Test the SAME playable interface a human uses, non-
        # interactively - per direct instruction: write a keycode to
        # history.txt (what keyboard_input.c would write), run
        # main_loop.pal for real (backgrounded - it loops forever by
        # design, same as a real session; there's no "process one tick
        # and return" variant, that WOULD be a side-channel shortcut),
        # then read current_frame.txt (what the renderer would draw).
        # Usage: sim-key <keycode> [wait_seconds, default 1]
        cd "$SCRIPT_DIR"
        export PRISC_PROJECT_ROOT="$SCRIPT_DIR"
        export PRISC_PROJECT_ID="wsr-pal"
        mkdir -p pieces/apps/player_app pieces/display
        echo "$2" >> pieces/apps/player_app/history.txt
        ./system/prisc+x pal/main_loop.pal >/tmp/wsr_simkey.log 2>&1 &
        PID=$!
        sleep "${3:-1}"
        kill "$PID" 2>/dev/null
        wait "$PID" 2>/dev/null
        ;;
    new-game)
        cd "$SCRIPT_DIR"
        export PRISC_PROJECT_ROOT="$SCRIPT_DIR"
        # Real bug, found TWICE (2026-07-16): first a hardcoded key 51
        # under an older 5-item menu, then a hardcoded key 53 that broke
        # again the moment Trade/Management/Derivatives rows were added
        # and pushed New Game to position 8. Hardcoding a row number
        # here fights the entire point of piece.pdl-driven menus (adding
        # a menu option shouldn't require finding and fixing every
        # hardcoded digit elsewhere) - fixed for real this time by
        # resolving NEW_GAME's row number FROM the piece.pdl itself, the
        # same source of truth the menu rendering already uses.
        new_game_row=$(grep -n '^METHOD' projects/wsr-pal/pieces/wsr_main_menu/piece.pdl | grep 'START_WIZARD:new_game' | head -1 | cut -d: -f1)
        if [ -z "$new_game_row" ]; then
            echo "Could not find New Game row in wsr_main_menu/piece.pdl - aborting." >&2
            exit 1
        fi
        # wsr_menu_input.c derives "which screen" from
        # pieces/display/current_layout.txt (xyzos-standards.txt §18), not
        # from any field this action sets directly - force it to name
        # wsr_main_menu here so a direct, non-chtpm CLI invocation
        # resolves the same piece.pdl new_game_row was just computed
        # against, regardless of whatever a previous "run" session left
        # this file pointing at.
        mkdir -p pieces/display
        echo "pieces/chtpm/layouts/wsr_main_menu.chtpm" > pieces/display/current_layout.txt
        ./ops/+x/wsr_menu_input.+x "$((48 + new_game_row))"
        ./ops/+x/wsr_menu_input.+x 10
        echo "new game started (world reset from pieces_template)"
        ;;
    tick-all|ta)
        cd "$SCRIPT_DIR"
        bash scripts/ensure_entities.sh
        bash scripts/tick_all.sh "${2:-1}"
        ;;
    test-tick|test-run)
        cd "$SCRIPT_DIR"
        bash scripts/ensure_entities.sh
        export PRISC_PROJECT_ROOT="$SCRIPT_DIR"
        export PRISC_PROJECT_ID="wsr-pal"
        ./system/prisc+x pal/single_tick.pal
        cat projects/wsr-pal/pieces/corp_ORB/state.txt
        ;;
    test-choose)
        cd "$SCRIPT_DIR"
        export PRISC_PROJECT_ROOT="$SCRIPT_DIR"
        ./ops/+x/corp_set_human_decision.+x corp_ORB "$2"
        ;;
    reset)
        cd "$SCRIPT_DIR"
        printf "current_state=0\ndecision_mode=1\ncash=183.44\nstock_price=167.70\nbook_value=1740.05\nshares_outstanding=11.24\nmarket_cap=1884.31\ndebt_to_equity=0.08\nrisk_bias=9\nshares_held=0\npending_action=\nlast_action=\nhuman_decision=\n" > projects/wsr-pal/pieces/corp_ORB/state.txt
        echo "corp_ORB reset (real seed data: Orbital Express / ORB)."
        ;;
    check|verify)
        for b in system/prisc+x system/keyboard_input system/renderer \
                 ops/+x/corp_tick_idle.+x ops/+x/corp_decide.+x ops/+x/corp_trade.+x \
                 ops/+x/wsr_menu_input.+x ops/+x/wsr_compose_frame.+x \
                 ops/+x/connect_op.+x ops/+x/json_parser.+x; do
            if [ -x "$SCRIPT_DIR/$b" ]; then echo "OK   $b"; else echo "MISSING $b"; fi
        done
        ;;
    help|h|-h|--help)
        echo "wsr-pal button.sh"
        echo ""
        echo "Usage: ./button.sh <action> [--pal]"
        echo "  compile, c, build   - Build prisc+x + orchestrator + ops"
        echo "  run, r              - THE REAL PLAYABLE GAME (orchestrator manages all processes)"
        echo "  --pal               - Use PAL script mode (passed to run)"
        echo "  kill, k, stop       - Kill any lingering wsr-pal processes"
        echo "  sim-key <code>      - Test the playable interface non-interactively (see header comment)"
        echo "  new-game            - Reset the world to its initial 57-entity state"
        echo "  tick-all [rounds]   - Advance every entity N rounds, non-interactive batch"
        echo "  test-tick/test-choose/reset - old single-corp-ORB CLI test conveniences"
        echo "  check, verify       - Verify all binaries exist"
        echo "  help, h             - Show this help"
        ;;
    *)
        echo "Unknown action: $ACTION"
        exit 1
        ;;
esac
