#!/bin/bash
# button.sh - text-editor-xyz app launcher
#
# Architecture (per xyzos-standards §36):
#   Editor and file-menu widget are SEPARATE PROGRAMS, each with their
#   own session directory, system binaries, ops, and PAL loops.
#   They communicate via file-mediated widget cmd bus (inbox/status).
#   App launcher starts both simultaneously and kills both on exit.
#
#   Editor: foreground session (owns TTY, has keyboard_input)
#   Widget: background session (widget profile, TTY→/dev/null)
ACTION="${1:-help}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
HOUSE_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"

EDITOR_DIR="$(ls -d "$HOUSE_DIR/102."*"editor"* 2>/dev/null | head -1)"
FM_DIR="$(ls -d "$HOUSE_DIR/&.widgits/file-menu"* 2>/dev/null | head -1)"

err() { echo "$*" >&2; exit 1; }
[ -n "$EDITOR_DIR" ] && [ -d "$EDITOR_DIR" ] || err "Editor not found in $HOUSE_DIR"
[ -n "$FM_DIR" ] && [ -d "$FM_DIR" ] || err "File-menu widget not found in $HOUSE_DIR"

SESSION_ID="$(date +%s)-$$"
EDITOR_SESSION="/tmp/.text-editor-xyz-editor-$SESSION_ID"

run_app() {
    # REAL BUG, LIVE-CAUGHT 2026-07-31: this function never killed
    # anything before building a fresh session - a stale agy_browser_
    # manager.+x from a PRIOR session (this same "kill" action below was
    # ALSO missing this daemon from its own pkill list until this same
    # fix) kept running indefinitely, sharing the SAME real docs/
    # symlink every session uses, and produced real, confusing symptoms
    # in a fresh session: typed text not persisting, LOAD/SAVE AS
    # screens not responding to nav past a certain point. A defensive
    # kill here means every launch starts clean regardless of whether a
    # prior session was ever properly torn down.
    # REAL BUG, LIVE-CAUGHT 2026-07-31 (found immediately after the fix
    # above): a bare `pkill; sleep 0.2` does NOT prove anything actually
    # died - pkill only sends the signal and returns immediately, it
    # never waits for the target to exit. A threaded daemon like
    # agy_browser_manager.+x can outlive a flat 0.2s sleep, giving a
    # real window where the OLD process and the brand-new session's own
    # freshly-launched process are BOTH alive and BOTH consuming the
    # same real keystrokes - confirmed live: typed characters landed
    # doubled in the editor buffer ("hi" -> "hhii") right after this
    # exact fix was added. Poll until genuinely dead instead of guessing
    # a fixed delay is long enough.
    PKILL_PATTERNS='system/keyboard_input system/renderer system/prisc\+x system/chtpm_parser_pal system/gl_mirror system/chtpm_rgb_render manager/\+x/agy_browser_manager\.\+x'
    for pat in $PKILL_PATTERNS; do
        pkill -f "$pat" 2>/dev/null
    done
    waited=0
    while [ "$waited" -lt 30 ]; do
        still_alive=0
        for pat in $PKILL_PATTERNS; do
            pgrep -f "$pat" >/dev/null 2>&1 && still_alive=1
        done
        [ "$still_alive" = "0" ] && break
        sleep 0.1
        waited=$((waited + 1))
    done
    if [ "$still_alive" = "1" ]; then
        for pat in $PKILL_PATTERNS; do
            pkill -9 -f "$pat" 2>/dev/null
        done
        sleep 0.2
    fi
    # ── 1. Create EDITOR session directory ──
    mkdir -p "$EDITOR_SESSION/pieces/system" "$EDITOR_SESSION/pieces/display" \
             "$EDITOR_SESSION/pieces/apps/player_app" "$EDITOR_SESSION/pieces/keyboard" \
             "$EDITOR_SESSION/projects/agy-editor/manager" \
             "$EDITOR_SESSION/docs"

    cp -r "$EDITOR_DIR/system" "$EDITOR_SESSION/system"
    cp -r "$EDITOR_DIR/ops" "$EDITOR_SESSION/ops"
    cp -r "$EDITOR_DIR/pal" "$EDITOR_SESSION/pal"
    cp -r "$EDITOR_DIR/default_op.txt" "$EDITOR_SESSION/default_op.txt"
    cp -r "$EDITOR_DIR/pieces/chtpm" "$EDITOR_SESSION/pieces/chtpm"
    # Required for chtpm_rgb_render to find glyph data (PITFALL 52) —
    # without this every character renders invisible (checksummed but
    # visually blank GL window). This launcher builds its own separate
    # editor session rather than delegating to 102.editor's own
    # button.sh, so it needs this symlink added independently.
    cp -r "$EDITOR_DIR/pieces/registry" "$EDITOR_SESSION/pieces/registry" 2>/dev/null || true
    cp -r "$EDITOR_DIR/projects/agy-editor/pieces" "$EDITOR_SESSION/projects/agy-editor/pieces"
    cp -r "$EDITOR_DIR/docs" "$EDITOR_SESSION/docs"

    cd "$EDITOR_SESSION"

    # ── 2. Init editor state ──
    : > pieces/apps/player_app/interact_relay.txt
    : > pieces/keyboard/history.txt
    : > pieces/display/editor_screen_changed.txt
    : > pieces/display/frame_changed.txt
    : > pieces/display/renderer_pulse.txt
    : > projects/agy-editor/manager/gui_state.txt

    if [ ! -f pieces/system/editor_buffer.txt ]; then
        printf 'hi   \360\237\230\205 \360\237\230\206 \360\237\220\247\n' > pieces/system/editor_buffer.txt
    fi
    cat > pieces/system/editor_state.txt << 'EOF'
file_path=docs/untitled.txt
cursor_pos=-1
last_message=text-editor-xyz ready. F4 for FILE MENU.
EOF
    cat > pieces/apps/player_app/state.txt << 'EOSTATE'
module_path=system/prisc+x pal/main_loop_chtpm.pal
project_id=agy-editor
active_target_id=editor
EOSTATE

    export PRISC_PROJECT_ROOT="$EDITOR_SESSION"
    export PRISC_PROJECT_ID="agy-editor"

    # ── 3. Set up widget cmd bus (editor's inbox for widget commands) ──
    mkdir -p pieces/system/widget_cmds
    : > pieces/system/widget_cmds/inbox.txt
    : > pieces/system/widget_cmds/status.txt

    # ── 4. Compose initial editor frame ──
    if [ -x ./ops/+x/editor_compose_frame.+x ]; then
        PRISC_PROJECT_ROOT="$EDITOR_SESSION" \
            ./ops/+x/editor_compose_frame.+x >/dev/null 2>&1 || true
    fi

    # ── 5. Write house root for widget/ledger discovery ──
    echo "$HOUSE_DIR" > pieces/system/house_root.txt

    # ── 6. Register in xyzfs runtime ledger ──
    LEDGER_APPEND="$FM_DIR/ops/+x/ledger_append.+x"
    if [ -x "$LEDGER_APPEND" ]; then
        PRISC_PROJECT_ROOT="$EDITOR_SESSION" \
            "$LEDGER_APPEND" ONLINE editor agy-editor "$EDITOR_SESSION" $$ \
            "text-editor-xyz" pieces/system/widget_cmds/inbox.txt >/dev/null 2>&1 || true
    fi

    # ── 7. Start editor daemons (background) ──
    ./system/renderer &
    RENDERER_PID=$!
    ./system/chtpm_parser_pal pieces/chtpm/layouts/editor.chtpm >/dev/null 2>&1 &
    CHTPM_PID=$!

    # ── 7b. Editor's OWN GL/RGB window (§35 GL-primary) — this
    #       launcher previously only ever opened file-menu's widget
    #       window (step 8 below), never editor's own. Same generic
    #       pipeline, same wait-for-frame race fix already applied to
    #       102.editor-📄️00.00/button.sh and &.widgits/file-menu/
    #       button.sh (PITFALL 54) — chtpm_rgb_render's own initial
    #       render + baseline pulse-size read must happen AFTER
    #       chtpm_parser_pal's first compose, or rgb_frame.raw gets
    #       stuck permanently all-black.
    EDITOR_GL_PID=""
    EDITOR_RGB_PID=""
    if [ -z "$NO_GL" ] && [ -n "$DISPLAY" ]; then
        waited=0
        while [ ! -s pieces/display/current_frame.txt ] && [ "$waited" -lt 20 ]; do
            sleep 0.1
            waited=$((waited + 1))
        done
        if [ -x ./system/gl_mirror ]; then
            ./system/gl_mirror >/dev/null 2>&1 &
            EDITOR_GL_PID=$!
        fi
        if [ -x ./system/chtpm_rgb_render ]; then
            ./system/chtpm_rgb_render >/dev/null 2>&1 &
            EDITOR_RGB_PID=$!
        fi
    fi

    # ── 7. Start widget cmd drainer (background) ──
    WIDGET_CMDS_PID=""
    if [ -x "$EDITOR_DIR/ops/+x/editor_widget_cmds.+x" ]; then
        (
            cd "$EDITOR_SESSION"
            while [ -f pieces/system/widget_cmds/inbox.txt ]; do
                PRISC_PROJECT_ROOT="$EDITOR_SESSION" \
                    "$EDITOR_DIR/ops/+x/editor_widget_cmds.+x" 8 >/dev/null 2>&1 || true
                sleep 0.2
            done
        ) &
        WIDGET_CMDS_PID=$!
    fi

    # ── 8. Start file-menu widget (background, widget profile, TTY→/dev/null)
    #      gl_mirror opens the GL window; keyboard forwarded to interact_relay.txt ──
    cd "$FM_DIR"
    RUN_PROFILE=widget \
        bash "$FM_DIR/button.sh" r "$EDITOR_SESSION" >/dev/null 2>&1 &
    WIDGET_PID=$!

    cd "$EDITOR_SESSION"

    # ── 9. Cleanup trap ──
    kill_own_module() {
        local pid cwd
        for pid in $(pgrep -f "system/prisc\+x" 2>/dev/null); do
            cwd="$(readlink -f "/proc/$pid/cwd" 2>/dev/null)"
            if [ "$cwd" = "$EDITOR_SESSION" ]; then
                kill -9 "$pid" 2>/dev/null
            fi
        done
    }

    cleanup() {
        if [ -x "$LEDGER_APPEND" ]; then
            PRISC_PROJECT_ROOT="$EDITOR_SESSION" \
                "$LEDGER_APPEND" OFFLINE editor agy-editor "$EDITOR_SESSION" $$ \
                "text-editor-xyz" pieces/system/widget_cmds/inbox.txt >/dev/null 2>&1 || true
        fi
        kill "$WIDGET_CMDS_PID" "$RENDERER_PID" "$CHTPM_PID" 2>/dev/null || true
        kill "$EDITOR_GL_PID" "$EDITOR_RGB_PID" 2>/dev/null || true
        kill "$WIDGET_PID" 2>/dev/null || true
        kill_own_module
        # REAL BUG, LIVE-CAUGHT 2026-07-31: this Ctrl+C/exit trap only
        # ever killed the EDITOR's own prisc+x (by cwd match) - it never
        # touched agy_browser_manager.+x at all (spawned by either the
        # editor's own or file-menu's own browser screens), and killing
        # $WIDGET_PID (the wrapper `bash file-menu/button.sh` process)
        # does NOT cascade to file-menu's own real child stack (its own
        # renderer/chtpm_parser_pal/keyboard_input/prisc+x, running
        # under a DIFFERENT session dir this trap's own cwd check never
        # matches). Confirmed live: real orphaned processes from a
        # normal Ctrl+C exit kept running and corrupting later sessions
        # via the shared docs/ symlink. Broad pkill here (not cwd-
        # scoped) matches this house's own EMERGENCY_KILL.sh/button.sh
        # kill convention elsewhere - reusing file-menu's own real kill
        # action for its half rather than reimplementing it.
        pkill -f "manager/\+x/agy_browser_manager\.\+x" 2>/dev/null || true
        [ -n "${FM_DIR:-}" ] && [ -x "$FM_DIR/button.sh" ] && bash "$FM_DIR/button.sh" kill >/dev/null 2>&1 || true
        mkdir -p "$SCRIPT_DIR/sessions"
        if [ -f pieces/system/editor_buffer.txt ]; then
            cp pieces/system/editor_buffer.txt "$SCRIPT_DIR/sessions/autosave.txt" 2>/dev/null || true
        fi
        persist_session_state; rm -rf "$EDITOR_SESSION" 2>/dev/null || true
    }
    # Step 2 symlink-migration fix: copy mutable session state back
    # to the real project root before the session dir is deleted
    # (the old symlinks made these writes land at the real root for
    # free; cp -r sessions need this explicit copy-back). Merge
    # semantics - adds/overwrites, never deletes. Volatile files
    # (quit_flag, pids, history, relays, gui_state) are NOT copied.
    persist_session_state() {
        mkdir -p "$EDITOR_DIR/docs/" 2>/dev/null || true
        cp -r "$EDITOR_SESSION/docs/." "$EDITOR_DIR/docs/" 2>/dev/null || true
    }
    trap cleanup EXIT INT TERM

    # ── 10. Keyboard input (foreground, blocking) ──
    : > pieces/apps/player_app/history.txt
    ./system/keyboard_input

    cleanup
}

case "$ACTION" in
    compile|c|build)
        echo "text-editor-xyz: checking dependencies..."
        echo "  editor: $EDITOR_DIR"
        echo "  file-menu: $FM_DIR"
        echo "--- Editor ---"
        for b in system/prisc+x system/keyboard_input system/renderer \
                 system/chtpm_parser_pal; do
            [ -x "$EDITOR_DIR/$b" ] && echo "OK   $b" || echo "MISS $b"
        done
        for b in system/gl_mirror system/chtpm_rgb_render; do
            [ -x "$EDITOR_DIR/$b" ] && echo "OK   $b (editor's own GL window)" || echo "OPTIONAL-MISS $b"
        done
        for b in ops/+x/editor_menu_input.+x ops/+x/editor_compose_frame.+x \
                 ops/+x/editor_widget_cmds.+x; do
            [ -x "$EDITOR_DIR/$b" ] && echo "OK   $b" || echo "MISS $b"
        done
        echo "--- File-menu widget ---"
        for b in system/prisc+x system/chtpm_parser_pal; do
            [ -x "$FM_DIR/$b" ] && echo "OK   $b" || echo "MISS $b"
        done
        for b in ops/+x/fm_set_focus.+x ops/+x/fm_enqueue_cmd.+x \
                 ops/+x/fm_compose_frame.+x ops/+x/fm_menu_input.+x; do
            [ -x "$FM_DIR/$b" ] && echo "OK   $b" || echo "MISS $b"
        done
        echo "--- Layouts ---"
        [ -f "$FM_DIR/pieces/chtpm/layouts/file-menu.chtpm" ] && echo "OK   file-menu.chtpm" || echo "MISS file-menu.chtpm"
        [ -f "$FM_DIR/pal/main_loop_chtpm.pal" ] && echo "OK   main_loop_chtpm.pal" || echo "MISS main_loop_chtpm.pal"
        [ -f "$EDITOR_DIR/pieces/chtpm/layouts/editor.chtpm" ] && echo "OK   editor.chtpm" || echo "MISS editor.chtpm"
        [ -f "$EDITOR_DIR/pal/main_loop_chtpm.pal" ] && echo "OK   editor main_loop_chtpm.pal" || echo "MISS editor main_loop_chtpm.pal"
        ;;
    run|r|start)
        run_app
        ;;
    kill|k|stop)
        pkill -f "system/keyboard_input" 2>/dev/null
        pkill -f "system/renderer" 2>/dev/null
        pkill -f "system/prisc\+x" 2>/dev/null
        pkill -f "system/chtpm_parser_pal" 2>/dev/null
        pkill -f "system/gl_mirror" 2>/dev/null
        pkill -f "system/chtpm_rgb_render" 2>/dev/null
        pkill -f "manager/\+x/agy_browser_manager\.\+x" 2>/dev/null
        rm -rf /tmp/.text-editor-xyz-* 2>/dev/null
        echo "done"
        ;;
    help|h|-h|--help)
        echo "text-editor-xyz — Text editor + file-menu widget, each with its own GL window"
        echo ""
        echo "Architecture (xyzos-standards §36):"
        echo "  Editor and file-menu widget are SEPARATE PROGRAMS."
        echo "  Editor: foreground (owns TTY, INTERACT canvas, own GL window if DISPLAY set)."
        echo "  Widget: background (own GL window, no TTY)."
        echo "  Communication: file-mediated widget cmd bus (inbox.txt)."
        echo "  NO_GL=1 forces ASCII-only for both (headless/harness use)."
        echo ""
        echo "Usage: ./button.sh <action>"
        echo "  compile      - Check dependencies"
        echo "  run          - Launch editor + file-menu widget"
        echo "  kill         - Kill processes"
        echo "  help         - This help"
        ;;
    *)
        echo "Unknown: $ACTION (try ./button.sh help)"
        exit 1
        ;;
esac
