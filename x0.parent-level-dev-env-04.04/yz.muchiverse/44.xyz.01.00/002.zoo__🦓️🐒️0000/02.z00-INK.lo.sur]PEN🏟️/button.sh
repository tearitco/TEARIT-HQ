#!/bin/bash
# button.sh - launcher for zoo_0000, same verb convention as
# mutaclsym/1.TPMOS's own button.sh (c/compile, r/run, k/kill...).
#
# zoo_0000 is a sandbox: the real xlector active-target standard (see
# dox/xlector-standard.md) and the cross-game pet import/export shim
# (see dox/pet-import-export-standard.md), explored in isolation from
# mutaclsym so neither has to be bolted onto a bigger game to prove out.
# No title screen, no hero/player piece - boots straight into xlector
# control of the zoo (pal/main_loop.pal has no title phase at all,
# unlike mutaclsym's own main loop).
ACTION="${1:-help}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# REAL PATH BREAK, FOUND AND FIXED 2026-07-20 (project moved from
# z0.egg-pals+plats-8.01/z0.zoo_0000 into yz.muchiverse/2.muchi-verse/
# zoo_0000/, see !.HANDOFF-move-projects-into-muchiverse-2026-07-20.txt):
# ops/pet_export.c's own resolve_exchange_root() defaults to "one level
# above PRISC_PROJECT_ROOT" - before this move that correctly resolved
# to the real, shared z0.egg-pals+plats-8.01/exchange/ (also used by
# z0.muchi-pals-refx-13.00, the OTHER real side of the pet-export/
# import shim - NOT moved yet as of this fix, still a live agent
# session in it, see the handoff doc). Left on the default, THIS
# project's own new "one level up" would silently resolve to a
# different, empty yz.muchiverse/2.muchi-verse/exchange/ instead -
# not an error, just silently disconnected from the other side of the
# exchange. Pinned explicitly here to keep working exactly as before
# until muchi-pals-refx-13.00 ALSO moves (at which point exchange/
# should move too, to become a real sibling under 2.muchi-verse/ again,
# and this override should be REMOVED, reverting to the plain default).
export PRISC_EXCHANGE_ROOT="/home/no/Desktop/🤖️🪤️🏠️/🚽️🧻️/🚽️🥡️-00.00/ZEST-10.18/x0.parent-level-dev-env-03.01/z0.egg-pals+plats-8.01/exchange"

case "$ACTION" in
    compile|c|build)
        bash "$SCRIPT_DIR/scripts/build.sh"
        ;;
    run|r|start)
        cd "$SCRIPT_DIR"
        mkdir -p pieces/system pieces/display pieces/apps/player_app
        : > pieces/apps/player_app/history.txt
        # REAL PRE-EXISTING BUG FIX (found while diagnosing a chtpm-mode
        # report, but this affects "run" too): system/renderer.c's own
        # loop is `while (!quit_requested())`, checking whether
        # pieces/system/quit_flag.txt is non-empty - written by
        # keyboard_input.c on EVERY exit. Without resetting it here, any
        # session ever quit via 'q' leaves this file non-empty forever,
        # so the NEXT "run" (or "chtpm") launch's own renderer sees
        # quit_requested()==true before its own loop even starts - one
        # frame prints, then it exits immediately, looking exactly like
        # an unresponsive session even though everything underneath is
        # working correctly. This action never reset the file at all -
        # unlike mutaclsym's/muchipal-editor-0.0's/wsr-pal's/egg-pals'
        # own "run" actions, which already do.
        : > pieces/system/quit_flag.txt

        export PRISC_PROJECT_ROOT="$SCRIPT_DIR"
        export PRISC_PROJECT_ID="zoo_0000"

        ./system/renderer &
        RENDERER_PID=$!
        ./system/prisc+x pal/main_loop.pal >/dev/null 2>&1 &
        PRISC_PID=$!

        # Auto-launch the GL/RGB mirror of the LEVEL ITSELF (map + xlector -
        # the whole zoo, not any individual pet), same "run launches it
        # automatically, best-effort" behavior as mutaclsym's own gl_mirror
        # in its own button.sh run. Per-pet desktop windows are a SEPARATE
        # concern, owned by a different project (see dox/
        # pet-import-export-standard.md's own note on this split) - not
        # launched from here.
        GL_PID=""
        if [ -z "$NO_GL" ] && [ -x "system/gl_mirror" ]; then
            ./system/gl_mirror >/tmp/zoo_0000_gl_mirror.log 2>&1 &
            GL_PID=$!
            echo "GL/RGB mirror window launching (pid $GL_PID) - shows the whole" >&2
            echo "zoo level (map + xlector), same style as mutaclsym's own GL" >&2
            echo "mirror. If no window appears, see /tmp/zoo_0000_gl_mirror.log" >&2
        fi

        trap 'kill "$RENDERER_PID" "$PRISC_PID" $GL_PID 2>/dev/null' EXIT INT TERM

        ./system/keyboard_input

        kill "$RENDERER_PID" "$PRISC_PID" $GL_PID 2>/dev/null
        ;;
    chtpm|menu)
        # Real interact+module pattern (see chtpm-to-pal-layout-plan.txt
        # §8 and xyzos-standards.txt §7) - a SEPARATE entry point from
        # "run" above, not a replacement (least-impact: "run" is
        # already-proven real gameplay, untouched). pieces/chtpm/layouts/
        # zoo.chtpm's own <module>${module_path}</module> tag makes
        # chtpm_parser_pal ITSELF launch system/prisc+x pal/main_loop.pal
        # as a separate, parallel, persistent process the instant the
        # layout is parsed - zoo_0000's own real, already-working game
        # loop, completely unmodified, requiring NO separate launch line
        # here (unlike muchipal-editor-0.0's own title->projects
        # handoff, which needed a manual bash-side orchestration since
        # that's a one-time PHASE TRANSITION, not an always-embedded
        # module). system/renderer already works unmodified too - it
        # just polls pieces/display/current_frame.txt, which
        # chtpm_parser_pal now writes (menu chrome + the embedded
        # ${game_map}), same as it always polls regardless of writer.
        cd "$SCRIPT_DIR"
        mkdir -p pieces/system pieces/display pieces/apps/player_app pieces/keyboard
        : > pieces/apps/player_app/history.txt
        : > pieces/apps/player_app/interact_relay.txt
        : > pieces/keyboard/history.txt
        : > pieces/display/pending_command.txt
        # REAL BUG FIX - see "run" action's own comment above on
        # quit_flag.txt (same fix, same reason, needed here too).
        : > pieces/system/quit_flag.txt
        cat > pieces/apps/player_app/state.txt << 'EOSTATE'
module_path=system/prisc+x pal/main_loop_chtpm.pal
EOSTATE

        export PRISC_PROJECT_ROOT="$SCRIPT_DIR"
        export PRISC_PROJECT_ID="zoo_0000"

        ./system/renderer &
        RENDERER_PID=$!
        ./system/chtpm_parser_pal pieces/chtpm/layouts/zoo.chtpm >/dev/null 2>&1 &
        CHTPM_PID=$!

        # chtpm_rgb_render: real wraith_rgb_daemon.c equivalent (see
        # that file's own header comment in shared-ops/) - font-
        # rasterizes current_frame.txt verbatim (chrome AND embedded
        # game content) into rgb_frame.raw, the SAME path gl_mirror
        # already reads. Replaces the module's own compose_rgb_frame
        # call for chtpm mode specifically (removed from
        # main_loop_chtpm.pal - see that file's own comment).
        RGB_PID=""
        if [ -x "system/chtpm_rgb_render" ]; then
            ./system/chtpm_rgb_render >/tmp/zoo_0000_chtpm_rgb_render.log 2>&1 &
            RGB_PID=$!
        fi

        GL_PID=""
        if [ -z "$NO_GL" ] && [ -x "system/gl_mirror" ]; then
            ./system/gl_mirror >/tmp/zoo_0000_gl_mirror.log 2>&1 &
            GL_PID=$!
        fi

        # chtpm_parser_pal has no SIGTERM handler of its own (only
        # SIGINT triggers its own cleanup_module()), so a plain `kill`
        # here would leave its own spawned module (system/prisc+x)
        # orphaned - kill both explicitly, matching the relative-path
        # pkill fix below.
        trap 'kill "$RENDERER_PID" "$CHTPM_PID" $RGB_PID $GL_PID 2>/dev/null; pkill -f "system/prisc\+x" 2>/dev/null' EXIT INT TERM

        ./system/keyboard_input

        kill "$RENDERER_PID" "$CHTPM_PID" $RGB_PID $GL_PID 2>/dev/null
        pkill -f "system/prisc\+x" 2>/dev/null
        ;;
    import)
        cd "$SCRIPT_DIR"
        export PRISC_PROJECT_ROOT="$SCRIPT_DIR"
        shift
        ./ops/+x/pet_import.+x "$@"
        ;;
    window|win)
        cd "$SCRIPT_DIR"
        if [ ! -x "system/zoo_window" ]; then
            echo "system/zoo_window not built (X11/GLX may not be available - see scripts/build.sh output)"
            exit 1
        fi
        if [ -z "$2" ]; then
            echo "Usage: ./button.sh window <piece_id>   (e.g. pet_rex, pet_mochi, xlector)"
            exit 1
        fi
        export PRISC_PROJECT_ROOT="$SCRIPT_DIR"
        ./system/zoo_window "$2" &
        echo "zoo_window launched for '$2' (pid $!) - drag it with the left mouse"
        echo "button, right-click for Close, any keypress also closes it. It"
        echo "mirrors/writes the SAME pos_x/pos_y the terminal session uses, so"
        echo "moving it there or here shows up in both places."
        ;;
    kill|k|stop)
        echo "=== Killing zoo_0000 processes ==="
        # REAL BUG FIX (found live in muchipal-editor-0.0's own button.sh
        # this same session - see feedback_pkill_relative_path_gotcha.md):
        # run/chtpm launch every binary via a RELATIVE path (./system/foo,
        # after cd "$SCRIPT_DIR"), so its recorded command line never
        # contains $SCRIPT_DIR at all - matching against the absolute
        # path here silently never found the process, letting it leak
        # forever. Match the bare relative substring instead - pkill -f
        # matches anywhere in the command line, so this catches it
        # regardless of launch-time cwd.
        pkill -f "system/keyboard_input" 2>/dev/null
        pkill -f "system/renderer" 2>/dev/null
        pkill -f "system/prisc\+x" 2>/dev/null
        pkill -f "system/gl_mirror" 2>/dev/null
        pkill -f "system/zoo_window" 2>/dev/null
        pkill -f "system/chtpm_parser_pal" 2>/dev/null
        pkill -f "system/chtpm_rgb_render" 2>/dev/null
        # Per direct instruction ("maybe it should run a kill_all.sh
        # script like 1.tpmos does") after a real orphaned-process
        # incident this session - delegate to the shared, surgical,
        # SIGKILL-based 2.muchi-verse/kill_all.sh (modeled on real
        # 1.TPMOS's own pieces/os/kill_all.sh) as the authoritative
        # cleanup, not just this project's own local pass above.
        bash "$SCRIPT_DIR/../../yz.muchiverse/2.muchi-verse/kill_all.sh"
        sleep 0.2
        echo "done"
        ;;
    check|verify)
        for b in system/prisc+x system/keyboard_input system/renderer \
                 system/chtpm_parser_pal system/chtpm_rgb_render \
                 ops/+x/xlector_input.+x ops/+x/move_entity.+x ops/+x/tick_pets.+x \
                 ops/+x/compose_frame.+x ops/+x/pdl_reader.+x ops/+x/feed_pet.+x \
                 ops/+x/pet_pet.+x ops/+x/play_pet.+x ops/+x/pet_export.+x ops/+x/pet_import.+x; do
            if [ -x "$SCRIPT_DIR/$b" ]; then
                echo "OK   $b"
            else
                echo "MISSING $b"
            fi
        done
        if [ -x "$SCRIPT_DIR/system/zoo_window" ]; then
            echo "OK   system/zoo_window"
        else
            echo "SKIP system/zoo_window (optional - needs X11/GLX, see scripts/build.sh output)"
        fi
        ;;
    help|h|-h|--help)
        echo "zoo_0000 button.sh"
        echo ""
        echo "Usage: ./button.sh <action>"
        echo ""
        echo "Actions:"
        echo "  compile, c, build   - Build all binaries"
        echo "  run, r, start       - Run the sandbox (keyboard_input + prisc+x + renderer)"
        echo "  chtpm, menu         - Real chtpm menu shell around the SAME game loop,"
        echo "                        via the interact+module pattern (see"
        echo "                        chtpm-to-pal-layout-plan.txt §8 / xyzos-standards.txt §7)"
        echo "  window, win <id>    - Launch a real desktop GL window for a piece"
        echo "                        (drag-to-grid-snap, see dox/xlector-standard.md /"
        echo "                        dox/pet-import-export-standard.md)"
        echo "  import [piece_id]   - Import pet(s) from ../exchange/ (all, or one by name)"
        echo "  kill, k, stop       - Kill any lingering zoo_0000 processes"
        echo "  check, verify       - Verify all binaries exist"
        echo "  help, h             - Show this help"
        ;;
    *)
        echo "Unknown action: $ACTION"
        echo "Run './button.sh help' for usage."
        exit 1
        ;;
esac
