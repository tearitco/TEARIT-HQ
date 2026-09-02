#!/bin/bash
# button.sh - launcher for muchi-pals, same verb convention as mutaclsym's
# button.sh (c/compile, r/run, k/kill...).
#
# Status: muchi-pals is egg-pals renamed and rebuilt onto this family's
# real chtpm-native standard (href navigation + ${piece_methods} action
# dispatch, xyzos-standards.txt sec.6/12/16/18/19) - `./button.sh run`
# launches straight into pieces/chtpm/layouts/main.chtpm, dispatched by
# ops/muchi_menu_input.c + rendered by ops/muchi_compose_frame.c, both
# invoked from the single persistent pal/main_loop_chtpm.pal module every
# screen shares. Where feasible, per-instance action logic (claim tokens,
# coin flip, toggle sleep, clean, feed) now runs as real pal/ecall
# bytecode (pal/ops_native/*.pal) instead of C - see xyzos-standards.txt
# sec.21 for the active-target indirection pattern that makes a pal op
# addressable to "whichever pet is currently selected" despite prisc+x
# having no argv-into-a-launched-script mechanism yet. The original
# plain-ASCII menu loop (menu_input.c/compose_menu.c) is kept, unmodified
# and independently runnable, under `./button.sh run-classic`. Run
# `./button.sh icons` once before opening a pet's GL window if you want
# the poop/sleep status overlays to render.
ACTION="${1:-help}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# Only system/* binaries ever get a .exe sibling - gcc appends .exe
# itself to an -o name with no dot in it (system/renderer -> both
# system/renderer and system/renderer.exe can exist side by side after
# building on different platforms at different times); ops/+x/*.+x names
# already contain a dot, so Windows builds land directly in that same
# *.+x file, never a separate *.+x.exe.
#
# This tree can carry a stale prebuilt binary from *either* platform
# after being packaged/copied around (e.g. a Windows .exe left sitting
# next to a freshly-built native Linux/Mac binary, or vice versa) - the
# previous version of this helper always preferred .exe whenever one
# existed, which "worked" for the Windows side of that but broke the
# Linux/Mac side the moment a stale .exe was also present (exactly what
# produced "Exec format error" here). Detect the actual OS instead of
# guessing from which files happen to exist.
case "$(uname -s 2>/dev/null)" in
    MINGW*|MSYS*|CYGWIN*) EXE=".exe" ;;
    *) EXE="" ;;
esac
bin_path() {
    echo "$SCRIPT_DIR/$1$EXE"
}

case "$ACTION" in
    compile|c|build)
        bash "$SCRIPT_DIR/scripts/build.sh"
        gcc -o "$SCRIPT_DIR/system/orchestrator" "$SCRIPT_DIR/system/orchestrator.c" 2>/dev/null \
            && echo "OK   system/orchestrator" || echo "SKIP system/orchestrator"
        ;;
    check|verify)
        for b in system/prisc+x system/emoji_gen_atlas system/emoji_xtract system/egg_window \
                 system/keyboard_input system/renderer system/chtpm_parser_pal system/orchestrator \
                 ops/+x/generate_egg.+x ops/+x/claim_tokens.+x ops/+x/coin_flip.+x \
                 ops/+x/buy_egg.+x ops/+x/hatch_egg.+x ops/+x/menu_input.+x ops/+x/compose_menu.+x \
                 ops/+x/tick_pets.+x ops/+x/feed_pet.+x ops/+x/clean_pet.+x ops/+x/toggle_sleep.+x \
                 ops/+x/train_pet.+x ops/+x/export_card.+x ops/+x/destroy_card.+x \
                 ops/+x/list_processes.+x ops/+x/muchi_menu_input.+x ops/+x/muchi_compose_frame.+x; do
            # ops/+x/*.+x names already have a dot, so they're never
            # renamed to *.+x.exe even on Windows - only system/* gets a
            # platform-dependent suffix (see bin_path's own comment above).
            case "$b" in
                system/*) check_path="$SCRIPT_DIR/$b$EXE" ;;
                *) check_path="$SCRIPT_DIR/$b" ;;
            esac
            if [ -x "$check_path" ]; then
                echo "OK   $b"
            else
                echo "MISSING $b"
            fi
        done
        ;;
    demo|d)
        cd "$SCRIPT_DIR"
        export PRISC_PROJECT_ROOT="$SCRIPT_DIR"
        echo "=== Minting an egg for user_01 (bypasses the store's token cost) ==="
        EGG_ID=$(./ops/+x/generate_egg.+x user_01)
        echo "Minted: $EGG_ID"
        echo "--- state.txt ---"
        cat "pieces/world_01/map_lobby/$EGG_ID/state.txt"
        echo "--- user_01 inventory.txt ---"
        cat "pieces/world_01/map_lobby/user_01/inventory.txt"
        ;;
    run|r|start|chtpm|menu)
        # Real href + ${piece_methods} chtpm-native flow (xyzos-standards.txt
        # sec.6/12/16/18/19) - this IS the primary way to play muchi-pals
        # now, same shape as pal-chain's/wsr-pal's own real button.sh "run"
        # action, matching real chtpm/wraith-alpha precedent directly
        # rather than the earlier interact+module/"Control Pets" wrapper
        # (sec.16 retired that pattern for pure discrete-menu games like
        # this one - see pieces/chtpm/layouts/main.chtpm and this
        # project's own pal/main_loop_chtpm.pal). `chtpm`/`menu` are kept
        # as plain aliases for the same action, not a separate mode.
        #
        # SESSION ISOLATION (dox/03-session-isolation.md - a real, live-
        # caught bug from a sibling project: two concurrent `run`
        # invocations sharing one fixed set of ephemeral UI/input files
        # cross-contaminated each other's keystrokes and state). Every
        # invocation gets its own private, throwaway directory holding
        # ONLY that session's ephemeral UI state, symlinked back to the
        # real project root for everything static (system/ops/pal/chtmp
        # layouts/piece.pdl) and for the real persistent game data
        # (pieces/world_01/ - tokens/pet stats, shared and mutated
        # together by every session on purpose). Deleted on exit,
        # nothing to ever accumulate or manually clean up. Do NOT "fix"
        # collisions by killing whatever's already running instead -
        # directly rejected by the user ("we only want to kill them if
        # the user is done running them") - this isolation is the real
        # fix.
        cd "$SCRIPT_DIR"
        if [ ! -x "$SCRIPT_DIR/system/orchestrator" ]; then
            echo "Compiling orchestrator..."
            gcc -o "$SCRIPT_DIR/system/orchestrator" "$SCRIPT_DIR/system/orchestrator.c" 2>/dev/null
        fi
        SESSION_ID="$(date +%s)-$$"
        SESSION_DIR="$SCRIPT_DIR/pieces/sessions/$SESSION_ID"
        mkdir -p "$SESSION_DIR/pieces/system" "$SESSION_DIR/pieces/display" \
                 "$SESSION_DIR/pieces/apps/player_app" "$SESSION_DIR/pieces/keyboard" \
                 "$SESSION_DIR/pieces/os" "$SESSION_DIR/projects/muchi-pals/manager"
        # pieces/os/proc_list.txt (written by orchestrator at runtime) stays
        # REAL and session-local on purpose - see kill_all.sh's own header
        # for why a shared/symlinked proc_list.txt would let one session's
        # cleanup kill another session's tracked PIDs. kill_all.sh itself
        # is a static script, safe to symlink like everything else below.
        # No symlinks — C processes resolve shared/persistent files via PRISC_PROJECT_ROOT env var

        cd "$SESSION_DIR"
        : > pieces/apps/player_app/history.txt
        : > pieces/apps/player_app/interact_relay.txt
        : > pieces/keyboard/history.txt
        : > pieces/display/pending_command.txt
        : > pieces/display/muchi_screen_changed.txt
        # xyzos-standards.txt sec.16.4: navigation-position state must be
        # reset on EVERY launch, never just a separate "new game" action -
        # these are pure session/UI-position files, never real game
        # progress (tokens/pet stats live in pieces/world_01/ instead,
        # untouched here on purpose).
        : > pieces/system/pets_screen_state.txt
        : > pieces/system/last_message.txt
        : > pieces/system/active_target.txt
        : > projects/muchi-pals/manager/gui_state.txt
        # REAL BUG FIX (xyzos-standards.txt sec.8.7 / this file's own prior
        # "chtpm" action already had this): system/renderer.c's own loop
        # is `while (!quit_requested())`, checking whether
        # pieces/system/quit_flag.txt is non-empty - written by
        # keyboard_input.c on EVERY exit. Without resetting it here, a
        # session ever quit via 'q' before leaves this file non-empty, so
        # the NEXT launch's own renderer sees quit_requested()==true
        # before its own loop even starts - one frame prints, then exits.
        : > pieces/system/quit_flag.txt
        # xyzos-standards.txt sec.16.1: project_id/active_target_id must be
        # seeded before the FIRST frame, not just on the first keypress,
        # or ${piece_methods} renders empty on first launch.
        cat > pieces/apps/player_app/state.txt << 'EOSTATE'
module_path=system/prisc+x pal/main_loop_chtpm.pal
project_id=muchi-pals
active_target_id=main
EOSTATE

        export PRISC_PROJECT_ROOT="$SCRIPT_DIR"
        export PRISC_PROJECT_ID="muchi-pals"
        # Shared exchange directory for cross-game pet import/export
        export PRISC_EXCHANGE_ROOT="/home/no/Desktop/🤖️🪤️🏠️/🥡️🪜️/🪜️-00.00/NNEST-11.12/x0.parent-level-dev-env-04.03/yz.muchiverse/44.xyz❤️‍🔥️00.07/exchange"
        # spawn_egg_window's OWN detached child (system/egg_window) is
        # DESIGNED to outlive this whole session (self-ticking pets keep
        # running after the menu quits - dox/01-architecture.md's own
        # "Step 7"). If it inherited PRISC_PROJECT_ROOT=$SESSION_DIR, its
        # every file access would break the instant this session's own
        # trap below deletes that directory. spawn_egg_window overrides
        # PRISC_PROJECT_ROOT to this stable, real path specifically for
        # that one child process before exec - see its own comment.
        export PRISC_REAL_PROJECT_ROOT="$SCRIPT_DIR"

        # system/orchestrator.c launches+tracks renderer and chtpm_parser_pal
        # itself (pieces/os/proc_list.txt, session-scoped 3-layer kill - see
        # that file's own header for why it runs in the BACKGROUND here
        # instead of as the foreground process the way mutaclsym's own
        # button.sh execs it: keyboard_input below must stay the foreground
        # command for this project's real, working exit-on-'q' UX).
        export PAL_LAYOUT="pieces/chtpm/layouts/main.chtpm"
        "$(bin_path system/orchestrator)" 2>pieces/system/orchestrator.log &
        ORCH_PID=$!

        # dox/03-session-isolation.md's "pkill trap gotcha": system/prisc+x
        # is always launched with the SAME relative argv
        # (pal/main_loop_chtpm.pal) across every session, so a plain
        # `pkill -f "system/prisc\+x"` cannot tell sessions apart by argv
        # alone - it would kill every OTHER concurrent session's own
        # module the instant this one exits. Match each candidate's own
        # real cwd against THIS session's directory instead - cwd is
        # genuinely unique per session, argv text is not. (orchestrator
        # doesn't manage this process - chtpm_parser_pal launches it - so
        # it stays a separate, explicit cleanup step here.)
        kill_own_module() {
            local pid cwd
            for pid in $(pgrep -f "system/prisc\+x" 2>/dev/null); do
                cwd="$(readlink -f "/proc/$pid/cwd" 2>/dev/null)"
                [ "$cwd" = "$SESSION_DIR" ] && kill -9 "$pid" 2>/dev/null
            done
        }
        # Signaling ORCH_PID triggers its own full cascade cleanup
        # (renderer + chtpm_parser_pal + a session-scoped kill_all.sh
        # sweep) - no need to track/kill those two PIDs from bash anymore.
        trap 'kill "$ORCH_PID" 2>/dev/null; kill_own_module; rm -rf "$SESSION_DIR"' EXIT INT TERM

        # keyboard_input MUST run as a plain foreground command here, NOT
        # backgrounded (`&`) - REVERTED, real user-reported regression:
        # backgrounding it (an earlier pass in this same session, to make
        # `kill -INT <bash-pid>` reach this trap promptly) broke real
        # terminal input/arrow-key decoding entirely, because a
        # backgrounded process reading the controlling tty loses proper
        # foreground process-group status under job control. Matches
        # pal-forum's own real, working button.sh exactly (`./system/
        # keyboard_input`, plain foreground, no `&`) - confirmed by
        # direct comparison, not guessed. The narrower gap this reverts
        # (`kill -INT` sent directly to bash's own pid, bypassing the
        # terminal, doesn't reach this trap while blocked here) is a
        # real, still-open, FAMILY-WIDE characteristic - pal-forum's own
        # button.sh has the identical shape - not a muchi-pals-specific
        # regression, and not worth breaking real keyboard input to close.
        # A genuine terminal Ctrl+C is unaffected: it signals the whole
        # foreground process group at once, so keyboard_input's own
        # SIGINT handler (system/keyboard_input.c's handle_signal())
        # fires directly and exits, letting this foreground command
        # return naturally - same clean path pressing 'q' already uses,
        # verified working via direct fifo-based testing.
        "$(bin_path system/keyboard_input)"

        kill "$ORCH_PID" 2>/dev/null
        kill_own_module
        ;;
    run-classic|classic)
        # The ORIGINAL, pre-chtpm plain-ASCII menu loop (menu_input.c/
        # compose_menu.c, no href/${piece_methods}) - kept, working,
        # unmodified, not the primary way to play anymore (see "run"
        # above) but still real and independently runnable. Same
        # session-isolation treatment as "run" above (dox/
        # 03-session-isolation.md) - a smaller ephemeral-state list here
        # (quit_flag.txt/history.txt/session.pid only).
        cd "$SCRIPT_DIR"
        SESSION_ID="$(date +%s)-$$"
        SESSION_DIR="$SCRIPT_DIR/pieces/sessions/$SESSION_ID"
        mkdir -p "$SESSION_DIR/pieces/system" "$SESSION_DIR/pieces/display" \
                 "$SESSION_DIR/pieces/apps/player_app"
        # No symlinks — C processes resolve shared/persistent files via PRISC_PROJECT_ROOT env var

        cd "$SESSION_DIR"
        : > pieces/system/quit_flag.txt
        : > pieces/apps/player_app/history.txt

        export PRISC_PROJECT_ROOT="$SCRIPT_DIR"
        export PRISC_PROJECT_ID="muchi-pals"
        # spawn_egg_window's OWN detached child (system/egg_window) is
        # DESIGNED to outlive this whole session (self-ticking pets keep
        # running after the menu quits - dox/01-architecture.md's own
        # "Step 7"). If it inherited PRISC_PROJECT_ROOT=$SESSION_DIR, its
        # every file access would break the instant this session's own
        # trap below deletes that directory. spawn_egg_window overrides
        # PRISC_PROJECT_ROOT to this stable, real path specifically for
        # that one child process before exec - see its own comment.
        export PRISC_REAL_PROJECT_ROOT="$SCRIPT_DIR"

        "$(bin_path system/renderer)" &
        RENDERER_PID=$!
        "$(bin_path system/prisc+x)" pal/main_loop.pal >/dev/null 2>&1 &
        PRISC_PID=$!
        # Session marker so a floating egg_window (spawned via
        # menu_input.c's spawn_egg_window) can tell when this session
        # ends and close itself instead of being left behind frozen.
        echo "$PRISC_PID" > pieces/system/session.pid
        kill_own_module() {
            local pid cwd
            for pid in $(pgrep -f "system/prisc\+x" 2>/dev/null); do
                cwd="$(readlink -f "/proc/$pid/cwd" 2>/dev/null)"
                [ "$cwd" = "$SESSION_DIR" ] && kill -9 "$pid" 2>/dev/null
            done
        }
        trap 'kill "$RENDERER_PID" "$PRISC_PID" 2>/dev/null; kill_own_module; rm -rf "$SESSION_DIR"' EXIT INT TERM

        # Plain foreground command - see "run" above's own comment for
        # why (reverted a real regression: backgrounding it broke actual
        # terminal/arrow-key input).
        "$(bin_path system/keyboard_input)"

        kill "$RENDERER_PID" "$PRISC_PID" 2>/dev/null
        kill_own_module
        ;;
    icons)
        bash "$SCRIPT_DIR/scripts/gen_icons.sh"
        ;;
    kill|k|stop)
        echo "=== Killing muchi-pals processes ==="
        # pkill isn't installed by default under MSYS2/Git-for-Windows -
        # fall back to matching `ps` output and killing by PID directly
        # when it's missing (still works fine on Linux/Mac if some
        # minimal image happens not to ship pkill either).
        # REAL BUG FIX (found live in muchipal-editor-0.0's own
        # button.sh this same session - see
        # feedback_pkill_relative_path_gotcha.md): run launches every
        # binary via a RELATIVE path (./system/foo, after
        # cd "$SCRIPT_DIR"), so its recorded command line never contains
        # $SCRIPT_DIR at all - matching against the absolute path here
        # (as this action used to) silently never found the process,
        # letting it leak forever. Match the bare relative substring
        # instead.
        kill_matching() {
            if command -v pkill >/dev/null 2>&1; then
                pkill -f "$1" 2>/dev/null
            else
                ps 2>/dev/null | grep -F "$1" | grep -v grep | awk '{print $1}' | while read -r pid; do
                    kill "$pid" 2>/dev/null
                done
            fi
        }
        kill_matching "system/keyboard_input"
        kill_matching "system/renderer"
        kill_matching "system/prisc+x"
        kill_matching "system/chtpm_parser_pal"
        kill_matching "system/orchestrator"
        # Per direct instruction ("maybe it should run a kill_all.sh
        # script like 1.tpmos does") after a real orphaned-process
        # incident this session - delegate to the shared, surgical,
        # SIGKILL-based 2.muchi-verse/kill_all.sh (modeled on real
        # 1.TPMOS's own pieces/os/kill_all.sh) too, on platforms where
        # bash+pkill are available (harmless no-op otherwise).
        if command -v pkill >/dev/null 2>&1; then
            bash "$SCRIPT_DIR/../../../yz.muchiverse/2.muchi-verse/kill_all.sh" 2>/dev/null
            # This project's own kill_all.sh (mass-refactor 2026-07-26,
            # pieces/os/ convention shared with the 101 standards) - no
            # session dir argument = global sweep across every session,
            # the correct scope for this manual "kill everything" verb.
            bash "$SCRIPT_DIR/pieces/os/kill_all.sh" 2>/dev/null
        fi
        echo "done"
        ;;
    killpets|kp)
        # Deliberately separate from `kill` above: that one only ever
        # touched the terminal-session processes, never the floating pet
        # windows (which are meant to outlive a single menu action) - see
        # scripts/kill_pets.sh's own header for why this exists as a
        # dedicated escape hatch instead of folding it into `kill`.
        sh "$SCRIPT_DIR/scripts/kill_pets.sh"
        ;;
    help|h|-h|--help)
        echo "muchi-pals button.sh"
        echo ""
        echo "Usage: ./button.sh <action>"
        echo ""
        echo "Actions:"
        echo "  compile, c, build   - Build all binaries"
        echo "  check, verify       - Verify all binaries exist"
        echo "  demo, d             - Mint one test egg end to end and print its state"
        echo "  run, r, start       - Play muchi-pals (real chtpm href/piece_methods UI -"
        echo "                        User/Faucet/Store/Pets/Processes)"
        echo "  chtpm, menu         - Alias for run (same real chtpm flow, not a separate mode)"
        echo "  run-classic, classic - The original plain-ASCII menu loop, kept for reference"
        echo "  icons               - Pre-render the poop/sleep status-overlay icons (once)"
        echo "  kill, k, stop       - Kill any lingering muchi-pals processes"
        echo "  killpets, kp        - Emergency: force-close every floating pet window"
        echo "  help, h             - Show this help"
        ;;
    *)
        echo "Unknown action: $ACTION"
        echo "Run './button.sh help' for usage."
        exit 1
        ;;
esac
