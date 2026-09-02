#!/bin/bash
# button.sh - launcher for mutaclsym, same verb convention as TPMOS's
# button.sh (c/compile, r/run, k/kill...).
#
# No ncurses anywhere: "run" starts these processes via chtpm_parser_pal -
#   - system/keyboard_input : owns the real terminal in raw mode, reads
#     keys itself, appends bare keycodes to history.txt. Runs in the
#     foreground since it's the one that needs the controlling tty.
#   - system/chtpm_parser_pal pieces/chtpm/layouts/game.chtpm : parses
#     the menu layout and spawns system/prisc+x pal/main_loop_chtpm.pal
#     as a separate persistent process (reads its own history from
#     interact_relay.txt), writes menu chrome + embedded game content
#     to pieces/display/current_frame.txt.
#   - system/renderer : cooked-mode stdout writer, polls the frame
#     pulse marker, prints current_frame.txt, logs every frame to
#     frame_history.txt for audit. Backgrounded.
#   - system/chtpm_rgb_render : font-rasterizes current_frame.txt
#     verbatim (menu chrome + embedded game content) into rgb_frame.raw,
#     the SAME path gl_mirror reads (replaces compose_rgb_frame which
#     doesn't run in chtpm mode).
#   - system/gl_mirror : a GLUT window that blits rgb_frame.raw (a plain
#     RGBA32 buffer computed by portable CPU C, zero GL calls anywhere
#     except gl_mirror.c itself - see GOVERNING CONSTRAINT in
#     2.muchi-verse/GRAND-ARCHITECTURE.md). Same "one state, two renderers"
#     mirror the ASCII path already is; this one just also happens to open
#     a window, and per direct intent it pops up automatically alongside
#     the terminal - it's a SECOND LIVE INPUT SOURCE usable simultaneously
#     with the terminal (real keyboard/arrow forwarding via its own GLUT
#     callbacks - confirmed working via direct synthetic-key testing once
#     the window has real focus). "run" launches it automatically if
#     system/gl_mirror was actually built (best-effort in scripts/build.sh -
#     some environments lack GLUT dev libs/a display, and the window failing
#     to open there is not fatal to the rest of "run"). Set NO_GL=1 to skip
#     launching it explicitly (e.g. for a headless/no-DISPLAY test run).
# "run" tracks every background PID it started and kills them (per the
# cdda-tpm-std-fast.txt rule: never leave an untracked subprocess
# running) once keyboard_input exits.
#
# "gl" below is a way to (re-)launch just the mirror window standalone
# - useful if "run" was started with NO_GL=1, or without a display
# available at the time, or after the window was closed without
# quitting the whole game. gl_mirror.c's own receipt-writing
# (pieces/display/gl_display.receipt.txt) is how its correctness gets
# verified either way, not the window itself.
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
            echo "=== mutaclsym Launcher (PAL Mode) ==="
            export PAL_LAYOUT="pieces/chtpm/layouts/game.chtpm"
        else
            echo "=== mutaclsym Launcher (C Mode) ==="
            export PAL_LAYOUT=""
        fi
        exec "$SCRIPT_DIR/system/orchestrator"
        ;;
    gl|mirror)
        cd "$SCRIPT_DIR"
        if [ ! -x "system/gl_mirror" ]; then
            echo "system/gl_mirror not built (GLUT/GL may not be available - see scripts/build.sh output)"
            exit 1
        fi
        mkdir -p pieces/display
        export PRISC_PROJECT_ROOT="$SCRIPT_DIR"
        export PRISC_PROJECT_ID="mutaclsym"
        echo "Launching gl_mirror standalone - './button.sh run' launches the game"
        echo "automatically (which includes gl_mirror if available), so you only need"
        echo "this verb to (re-)launch just the window on its own (e.g. after closing"
        echo "it without quitting the game). Make sure './button.sh run' is also running"
        echo "(or was run first) so pieces/display/rgb_frame.raw actually gets"
        echo "updated each tick. Correctness is verifiable via"
        echo "pieces/display/gl_display.receipt.txt and rgb_frame.receipt.txt"
        echo "without needing to see the window."
        ./system/gl_mirror
        ;;
    generate|gen)
        cd "$SCRIPT_DIR"
        export PRISC_PROJECT_ROOT="$SCRIPT_DIR"
        export PRISC_PROJECT_ID="mutaclsym"
        shift
        ./ops/+x/generate_map.+x "$@"
        ;;
    kill|k|stop)
        echo "=== Killing mutaclsym processes ==="
        pkill -9 -f "system/keyboard_input" 2>/dev/null
        pkill -9 -f "system/renderer" 2>/dev/null
        pkill -9 -f "system/prisc" 2>/dev/null
        pkill -9 -f "system/gl_mirror" 2>/dev/null
        pkill -9 -f "system/chtpm_parser_pal" 2>/dev/null
        pkill -9 -f "system/chtpm_rgb_render" 2>/dev/null
        pkill -9 -f "system/orchestrator" 2>/dev/null
        pkill -9 -f '\.pal$' 2>/dev/null
        pkill -9 -f "ops/+x/" 2>/dev/null
        sleep 0.2
        rm -f "$SCRIPT_DIR/pieces/system/gl_focus.lock"
        rm -f "$SCRIPT_DIR/pieces/system/quit_flag.txt"
        rm -f "$SCRIPT_DIR/pieces/os/proc_list.txt"
        echo "done"
        ;;
    check|verify)
        for b in system/prisc+x system/keyboard_input system/renderer \
                 system/chtpm_parser_pal system/chtpm_rgb_render \
                 ops/+x/move_player.+x ops/+x/end_turn.+x ops/+x/compose_frame.+x ops/+x/pickup.+x ops/+x/drop.+x ops/+x/eat.+x \
                 ops/+x/tick_monsters.+x ops/+x/craft.+x ops/+x/examine.+x ops/+x/save_game.+x \
                 ops/+x/title_input.+x ops/+x/compose_title_frame.+x ops/+x/pdl_reader.+x ops/+x/choice.+x \
                 ops/+x/compose_rgb_frame.+x ops/+x/dump_rgb_png.+x ops/+x/generate_map.+x; do
            if [ -x "$SCRIPT_DIR/$b" ]; then
                echo "OK   $b"
            else
                echo "MISSING $b"
            fi
        done
        if [ -x "$SCRIPT_DIR/system/gl_mirror" ]; then
            echo "OK   system/gl_mirror"
        else
            echo "SKIP system/gl_mirror (optional - needs GLUT/GL, see scripts/build.sh output)"
        fi
        ;;
    help|h|-h|--help)
        echo "mutaclsym button.sh"
        echo ""
        echo "Usage: ./button.sh <action> [--pal]"
        echo ""
        echo "Actions:"
        echo "  compile, c, build   - Build all binaries (prisc+x, keyboard_input, renderer, orchestrator, ops)"
        echo "  run, r, start       - Run the game (orchestrator manages all processes)"
        echo "  --pal               - Use PAL script mode (passed to run)"
        echo "  gl, mirror          - (Re-)launch just the GL/RGB mirror window standalone"
        echo "  kill, k, stop       - Kill any lingering mutaclsym processes"
        echo "  check, verify       - Verify all binaries exist"
        echo "  generate, gen <map_id> <seed> [w] [h] [link_map_id] [link_x] [link_y]"
        echo "                      - Procedurally generate a new map (authoring-time"
        echo "                        tool, deterministic per seed) and wire a"
        echo "                        bidirectional stairway back into an existing map"
        echo "                        (defaults: 80x40, linked from map_02 at (36,13))"
        echo "  help, h             - Show this help"
        echo ""
        echo "Recommended workflow:"
        echo "  1. ./button.sh compile"
        echo "  2. ./button.sh check"
        echo "  3. ./button.sh run          # C mode"
        echo "  3. ./button.sh run --pal    # PAL mode"
        ;;
    *)
        echo "Unknown action: $ACTION"
        echo "Run './button.sh help' for usage."
        exit 1
        ;;
esac
