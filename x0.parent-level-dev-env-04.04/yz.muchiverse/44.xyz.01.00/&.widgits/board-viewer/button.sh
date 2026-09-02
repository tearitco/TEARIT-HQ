#!/bin/bash
# button.sh - board-viewer widget (shared, focus-adaptive board/map
# viewer for civ-txt/tactics-txt). Modeled directly on
# &.widgits/file-menu/button.sh (the house's own real, proven widget
# launcher) - two launch profiles (app=ASCII, widget=GL), session
# isolation, real gl_mirror.c from 014.wsr-pal (NOT mutaclysm's own
# copy - see @.apps/BOARD_WIDGET_ARCHITECTURE.md §1a).
#
# REAL LEDGER REGISTRATION (added 2026-08-03, direct user correction:
# "all mechanics for the game should be paired with the viewer at app
# runtime, the way fm-widget is with text-editor-xyz - if civ/tactics
# don't do this, they did it wrong"). Previously omitted entirely - see
# git history / PIECECRAFT_XYZ_DESIGN.md for the full writeup - because
# file-menu's own ledger_append.c hard-exits(1) with no active login
# (0.user-pal👤️/00.login-signup/current_login.txt's own current_xyzfs
# empty). This project's own ops/ledger_append.c/ledger_peers.c are
# real ported copies with a "default" xyzfs bucket fallback instead of
# that hard error (direct instruction: "login can be optional, but use
# 'default'") - same real mechanism, works with or without a login.
# Every OPEN_BOARD_WIDGET press from civ-txt/tactics-txt now discovers
# an already-running board-viewer session via this ledger FIRST and
# reuses it (by overwriting its own bv_state.txt directly - every ops/
# bv_*.c re-reads that file fresh every frame, so no cmd-bus is needed
# just to refocus) instead of always spawning a redundant duplicate GL
# window, which is what happened before this fix.
ACTION="${1:-help}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# House root - two levels up (board-viewer is directly under &.widgits/),
# same computation every project's own button.sh uses (see civ-txt's
# own button.sh comment) - needed now for real ledger registration
# (ops/ledger_append.c/ledger_peers.c, added 2026-08-03, see that
# file's own header comment for why this widget didn't call it before).
HOUSE_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"

run_widget_session() {
    local FOCUS_PROJECT_ROOT="${1:-}"
    local PROFILE="${RUN_PROFILE:-}"
    if [ -z "$PROFILE" ]; then
        if [ -z "$DISPLAY" ]; then
            PROFILE="app"
        else
            PROFILE="widget"
        fi
    fi
    SESSION_ID="$(date +%s)-$$"
    SESSION_DIR="$SCRIPT_DIR/pieces/sessions/$SESSION_ID"
    mkdir -p "$SESSION_DIR/pieces/system" "$SESSION_DIR/pieces/display" \
             "$SESSION_DIR/pieces/apps/player_app" "$SESSION_DIR/pieces/keyboard" \
             "$SESSION_DIR/projects/board-viewer/manager"

    # Copy-based session isolation (2026-08-20, see sim-smell-fix.md's
    # "THE SOLUTION" section) - symlinks break on Windows and were
    # eliminated house-wide. PRISC_PROJECT_ROOT stays session-scoped
    # (unchanged below), so plain copies are all that's needed here -
    # unlike civ-txt/piececraft-xyz, board-viewer has NO persistent
    # state files (config.txt/data/) symlinked in this block, only
    # code, so no persist_session_state()/EXIT-trap copy-back is
    # needed either.
    cp -r "$SCRIPT_DIR/system" "$SESSION_DIR/system"
    cp -r "$SCRIPT_DIR/ops" "$SESSION_DIR/ops"
    cp -r "$SCRIPT_DIR/pal" "$SESSION_DIR/pal"
    cp -r "$SCRIPT_DIR/pieces/chtpm" "$SESSION_DIR/pieces/chtpm"
    cp -r "$SCRIPT_DIR/pieces/registry" "$SESSION_DIR/pieces/registry" 2>/dev/null
    cp -p "$SCRIPT_DIR/default_op.txt" "$SESSION_DIR/default_op.txt"
    mkdir -p "$SESSION_DIR/projects/board-viewer/pieces"
    cp -r "$SCRIPT_DIR/projects/board-viewer/pieces/board_viewer" \
          "$SESSION_DIR/projects/board-viewer/pieces/board_viewer" 2>/dev/null

    cd "$SESSION_DIR"
    : > pieces/apps/player_app/interact_relay.txt
    : > pieces/keyboard/history.txt
    : > pieces/display/bv_screen_changed.txt
    : > pieces/display/frame_changed.txt
    : > pieces/display/renderer_pulse.txt
    # REAL, NEW 2026-08-30, direct question ("is there also a way we can
    # default to interact mode on as well?") - INVESTIGATED, NOT A REAL
    # FIX HERE. active_gui_is_typing.txt (bv_compose_frame.c's own
    # "Interact mode: ON/OFF" line reads it as nav_mode, ~line 542) is
    # NOT board-viewer's own state - it's chtpm_parser_pal.c's (the
    # shared engine core) own export_active_index() output
    # (chtpm_parser_pal.c:2719), re-written on every real mode
    # transition based on the engine's own internal active_index. A
    # pre-seed here gets silently overwritten the instant the engine's
    # own first real export runs, since nothing has clicked the real
    # <button onClick="INTERACT"> yet (confirmed live: seeding "1" here
    # read back as "0" moments later). A real fix needs either a change
    # inside the shared engine itself (chtpm_parser_pal.c, used by all
    # 16 legacy-GL projects, not board-viewer-specific - see legacy-
    # shared-fix.md) to seed active_index at startup, or a real
    # synthetic first INTERACT click injected right after launch - both
    # real, scoped follow-up work, not done here.
    echo "$HOUSE_DIR" > pieces/system/house_root.txt
    # REAL, NEW 2026-08-29 (gl_mirror.c -> x11_mirror.c conversion,
    # legacy-shared-fix.md's own real gap - board-viewer was never
    # queued in the 16-project sweep since it's a shared WIDGET, not
    # one of the 16 top-level projects) - same real convention
    # piececraft-xyz's own button.sh already uses: the shared
    # x11_mirror.+x binary derives its window title from THIS file's
    # content (falling back to basename(project_root) - an ugly
    # timestamp for board-viewer's own ephemeral SESSION_DIR - if
    # missing). Point it at the FOCUSED host project when there is one
    # (gives a real, correct, distinct-per-host title like "piececraft-
    # xyz RGB mirror" instead of the old gl_mirror.c's hardcoded,
    # always-wrong "wsr-pal RGB mirror" copied verbatim from
    # 014.wsr-pal - a real bug this conversion also fixes, not just a
    # rename), else this widget's own project dir for a standalone
    # launch.
    if [ -n "$FOCUS_PROJECT_ROOT" ]; then
        echo "$FOCUS_PROJECT_ROOT" > pieces/system/real_project_root.txt
    else
        echo "$SCRIPT_DIR" > pieces/system/real_project_root.txt
    fi

    # emoji_mode default-on at launch, via a config-file flag rather than
    # a runtime toggle (per direct instruction: "we dont show ascii we
    # go immediately to emoji mode for these game widgets... using a
    # config file flag or something") - overridable per-launch with
    # BV_EMOJI_MODE=0 for headless ASCII-only testing (BOARD_WIDGET_
    # ARCHITECTURE.md §1b's own headless-testability requirement).
    local EMOJI_MODE_DEFAULT="${BV_EMOJI_MODE:-1}"
    if [ -n "$FOCUS_PROJECT_ROOT" ]; then
        cat > pieces/system/bv_state.txt << EOF
focused_project_id=$(basename "$FOCUS_PROJECT_ROOT")
focused_project_root=$FOCUS_PROJECT_ROOT
emoji_mode=$EMOJI_MODE_DEFAULT
EOF
    else
        cat > pieces/system/bv_state.txt << EOF
emoji_mode=$EMOJI_MODE_DEFAULT
EOF
    fi

    cat > pieces/apps/player_app/state.txt << 'EOSTATE'
module_path=system/prisc+x pal/main_module.pal
project_id=board-viewer
active_target_id=board_viewer
EOSTATE

    export PRISC_PROJECT_ROOT="$SESSION_DIR"
    export PRISC_PROJECT_ID="board-viewer"

    # Real ledger registration - lets any host (civ-txt/tactics-txt/
    # piececraft-xyz's own OPEN_BOARD_WIDGET handler) discover an
    # ALREADY-RUNNING board-viewer session and refocus it (by directly
    # overwriting ITS bv_state.txt, which every ops/bv_*.c re-reads
    # fresh every single frame - no cmd-bus needed for a plain refocus)
    # instead of always spawning a brand new duplicate GL window. See
    # ops/ledger_append.c's own header comment for the full writeup.
    #
    # PER-PROJECT SCOPING (added 2026-08-03, direct user correction: two
    # different games' own OPEN_BOARD_WIDGET presses were clobbering each
    # other's board-viewer session - tactics-txt's own press stole
    # piececraft-xyz's already-open window, then piececraft-xyz's next
    # press silently refocused a session the user couldn't find, since
    # the ledger only ever tracked ONE global "board-viewer" identity
    # regardless of which host it was showing). Real fix: the ledger's
    # own project_id field is now scoped to the FOCUSED host
    # ("board-viewer:<host_basename>"), so each host project gets its
    # own independently-discoverable, independently-refocusable widget
    # session - civ-txt's own OPEN_BOARD_WIDGET can never find/steal
    # piececraft-xyz's, and vice versa. A standalone launch (no
    # FOCUS_PROJECT_ROOT) keeps the old bare "board-viewer" identity -
    # nothing to scope it to.
    LEDGER_PROJECT_ID="board-viewer"
    if [ -n "$FOCUS_PROJECT_ROOT" ]; then
        LEDGER_PROJECT_ID="board-viewer:$(basename "$FOCUS_PROJECT_ROOT")"
    fi
    if [ -x ./ops/+x/ledger_append.+x ]; then
        ./ops/+x/ledger_append.+x ONLINE widget "$LEDGER_PROJECT_ID" "$SESSION_DIR" "$$" "Board Viewer" "pieces/system/bv_state.txt" >/dev/null 2>&1 || true
    fi

    if [ -x ./ops/+x/bv_compose_frame.+x ]; then
        ./ops/+x/bv_compose_frame.+x >/dev/null 2>&1 || true
    fi

    ./system/renderer &
    RENDERER_PID=$!
    ./system/chtpm_parser_pal pieces/chtpm/layouts/board_viewer.chtpm >/dev/null 2>&1 &
    CHTPM_PID=$!

    GL_PID=""
    RGB_PID=""

    if [ "$PROFILE" = "widget" ] || ([ "$PROFILE" = "app" ] && [ -n "$DISPLAY" ]); then
        # Same three-layer race fix documented in file-menu's own
        # button.sh (PITFALL 54): chtpm_rgb_render's own initial render
        # must happen AFTER chtpm_parser_pal's first compose, or
        # rgb_frame.raw gets stuck permanently all-black.
        waited=0
        while [ ! -s pieces/display/current_frame.txt ] && [ "$waited" -lt 20 ]; do
            sleep 0.1
            waited=$((waited + 1))
        done
        # REAL, NEW 2026-08-29 (gl_mirror.c -> x11_mirror.c conversion -
        # see real_project_root.txt write above for the full why).
        # Same real preference order piececraft-xyz's own button.sh
        # already uses: prefer the shared x11_mirror.+x binary (one
        # compiled copy, plain Xlib, real proven fixes gl_mirror.c never
        # got - borderless, keyboard-fwd, receipt-writing) over this
        # widget's own local legacy gl_mirror, with FORCE_GL_MIRROR=1 as
        # the same real escape hatch to force the old path if ever
        # needed for comparison/regression testing. argv1 is
        # SESSION_DIR (same project_root gl_mirror already implicitly
        # used via cwd - x11_mirror.+x doesn't need to be a child of
        # that dir, so it's passed explicitly instead).
        SHARED_MIRROR="$SCRIPT_DIR/../_shared-lib/ops/+x/x11_mirror.+x"
        MIRROR_TITLE="wsr-pal RGB mirror"  # legacy gl_mirror's own hardcoded title, unchanged if that path is taken
        if [ -z "$NO_GL" ] && [ -z "$FORCE_GL_MIRROR" ] && [ -x "$SHARED_MIRROR" ]; then
            "$SHARED_MIRROR" "$SESSION_DIR" >/dev/null 2>&1 &
            GL_PID=$!
            TITLE_BASE="$SCRIPT_DIR"
            [ -n "$FOCUS_PROJECT_ROOT" ] && TITLE_BASE="$FOCUS_PROJECT_ROOT"
            MIRROR_TITLE="$(basename "$TITLE_BASE") RGB mirror"
            GL_PROC_PATTERN="x11_mirror\.\+x $SESSION_DIR"
        elif [ -z "$NO_GL" ] && [ -x ./system/gl_mirror ]; then
            ./system/gl_mirror >/dev/null 2>&1 &
            GL_PID=$!
            GL_PROC_PATTERN="system/gl_mirror"
        fi
        if [ -n "$GL_PID" ]; then
            # REAL, NEW 2026-08-04, direct instruction (tile-picker's own
            # "^" drag-anywhere mode - see &.widgits/tile-picker/
            # TILE_PICKER_DESIGN.md §4): tag this window with the real
            # _NET_WM_PID property so tile-picker's ledger_peers+PID
            # lookup can find THIS specific board-viewer instance's
            # window unambiguously (every widget's LEGACY gl_mirror
            # window shared the same fixed title, "wsr-pal RGB mirror" -
            # title matching alone couldn't disambiguate; x11_mirror's
            # own real, per-host-derived title mostly fixes this itself
            # now, but the "not yet tagged" extra match condition in
            # bv_set_wm_pid.c still matters whenever two widgets happen
            # to focus the SAME host, so this stays wired for both
            # binaries). Re-resolves the true running PID via cwd-scoped
            # pgrep (bash's own $! didn't match the actual gl_mirror
            # process in testing - it forks internally), same technique
            # kill_own_module() below already uses.
            if [ -x ./ops/+x/bv_set_wm_pid.+x ]; then
                (
                    sleep 0.3
                    real_pid=""
                    for cand in $(pgrep -f "$GL_PROC_PATTERN" 2>/dev/null); do
                        cwd="$(readlink -f "/proc/$cand/cwd" 2>/dev/null)"
                        if [ "$cwd" = "$SESSION_DIR" ]; then real_pid="$cand"; fi
                    done
                    [ -n "$real_pid" ] && ./ops/+x/bv_set_wm_pid.+x "$MIRROR_TITLE" "$real_pid" >/dev/null 2>&1
                ) &
            fi
        fi
        # REAL FIX 2026-08-30, found live investigating "absolute parity
        # with the legacy model" (direct instruction) - chtpm_rgb_
        # render.+x is the real, shared COMPOSITOR daemon (blits the
        # real text chrome chtpm_parser_pal renders AND the real 3D
        # overlay bv_render_3d.c writes into ONE real rgb_frame.raw -
        # see that file's own blit_overlay()/MAP3D_MARKER header
        # comment), genuinely independent of whether any real WINDOW
        # (x11_mirror/gl_mirror) ever displays the result - a
        # real caller (e.g. khtpm_core_render.c's own
        # run_pchq_board_mode()) can read rgb_frame.raw directly with
        # zero window of its own needed here. NO_GL was wrongly gating
        # BOTH the window AND the compositor as one unit - its own name
        # only ever promised "no GL window", not "no compositing
        # either". Real, separate flag for the compositor specifically
        # (NO_RGB_COMPOSITOR) - NO_GL alone (the common real case) now
        # still lets a real caller read a real, fully-composited
        # rgb_frame.raw with no window ever mapping.
        if [ -z "$NO_RGB_COMPOSITOR" ] && [ -x ./system/chtpm_rgb_render ]; then
            ./system/chtpm_rgb_render >/dev/null 2>&1 &
            RGB_PID=$!
        fi
    fi

    kill_own_module() {
        local pid cwd
        for pid in $(pgrep -f "system/prisc\+x" 2>/dev/null); do
            cwd="$(readlink -f "/proc/$pid/cwd" 2>/dev/null)"
            if [ "$cwd" = "$SESSION_DIR" ]; then
                kill -9 "$pid" 2>/dev/null
            fi
        done
    }

    mark_offline() {
        if [ -x "$SCRIPT_DIR/ops/+x/ledger_append.+x" ]; then
            (cd "$SESSION_DIR" 2>/dev/null && PRISC_PROJECT_ROOT="$SESSION_DIR" "$SCRIPT_DIR/ops/+x/ledger_append.+x" OFFLINE widget "$LEDGER_PROJECT_ID" "$SESSION_DIR" "$$" "Board Viewer" "pieces/system/bv_state.txt" >/dev/null 2>&1) || true
        fi
    }
    trap 'mark_offline; kill "$RENDERER_PID" "$CHTPM_PID" "$GL_PID" "$RGB_PID" 2>/dev/null; kill_own_module; rm -rf "$SESSION_DIR"' EXIT INT TERM

    if [ "$PROFILE" = "widget" ]; then
        # Widget profile has NO foreground keyboard_input (GL owns
        # input via gl_mirror's own interact_relay forwarding) - just
        # wait for the parser to exit (matches file-menu's own
        # "wait for parser to exit" widget-profile shape).
        wait "$CHTPM_PID" 2>/dev/null
    else
        : > pieces/apps/player_app/history.txt
        ./system/keyboard_input
    fi

    kill "$RENDERER_PID" "$CHTPM_PID" "$GL_PID" "$RGB_PID" 2>/dev/null
    kill_own_module
}

case "$ACTION" in
    compile|c|build)
        bash "$SCRIPT_DIR/scripts/build.sh"
        ;;
    run-app|app|a)
        RUN_PROFILE=app run_widget_session "${2:-}"
        ;;
    run-widget|widget|w)
        RUN_PROFILE=widget run_widget_session "${2:-}"
        ;;
    run|r|start)
        run_widget_session "${2:-}"
        ;;
    kill|k|stop)
        pkill -f "system/keyboard_input" 2>/dev/null
        pkill -f "system/renderer" 2>/dev/null
        pkill -f "system/prisc\+x" 2>/dev/null
        pkill -f "system/chtpm_parser_pal" 2>/dev/null
        pkill -f "system/chtpm_rgb_render" 2>/dev/null
        pkill -f "system/gl_mirror" 2>/dev/null
        # REAL, NEW 2026-08-29 (gl_mirror.c -> x11_mirror.c conversion) -
        # the shared binary is now the preferred display mirror, so a
        # real `kill` needs to reap it too, scoped to THIS widget's own
        # sessions dir (same scoping every other shared-binary kill verb
        # in this house uses, e.g. taskbar's own pkill patterns) so it
        # never touches another project's own x11_mirror.+x instance.
        pkill -f "x11_mirror\.\+x $SCRIPT_DIR/pieces/sessions" 2>/dev/null
        echo "done"
        ;;
    check|verify)
        for b in system/prisc+x system/keyboard_input system/renderer \
                 system/chtpm_parser_pal ops/+x/bv_compose_frame.+x \
                 ops/+x/bv_menu_input.+x; do
            if [ -x "$SCRIPT_DIR/$b" ]; then echo "OK   $b"; else echo "MISSING $b"; fi
        done
        for b in system/chtpm_rgb_render system/gl_mirror; do
            if [ -x "$SCRIPT_DIR/$b" ]; then echo "OK   $b (optional GL)"; else echo "OPTIONAL-MISS $b"; fi
        done
        if [ -x "$SCRIPT_DIR/../_shared-lib/ops/+x/x11_mirror.+x" ]; then
            echo "OK   shared x11_mirror (preferred display mirror)"
        else
            echo "SKIP shared x11_mirror (see &.widgits/_shared-lib/ops/build_x11_mirror.sh)"
        fi
        ;;
    help|h|-h|--help)
        echo "board-viewer widget — shared board/map viewer for civ-txt/tactics-txt"
        echo ""
        echo "Usage: ./button.sh <action> [focused_project_root]"
        echo "  run | r | start          - Auto-detect mode: ASCII if headless, GL if DISPLAY"
        echo "  run-widget | widget | w  - Force GL window mode"
        echo "  run-app | app | a        - Force ASCII terminal mode (headless testing)"
        echo "  compile                  - Build ops + copy system binaries"
        echo "  kill                     - Kill processes"
        echo "  check                    - Verify binaries"
        echo ""
        echo "Environment:"
        echo "  RUN_PROFILE=app|widget   - Force mode regardless of DISPLAY"
        echo "  NO_GL=1                  - Skip gl_mirror/chtpm_rgb_render entirely"
        ;;
    *)
        echo "Unknown: $ACTION"; exit 1 ;;
esac
