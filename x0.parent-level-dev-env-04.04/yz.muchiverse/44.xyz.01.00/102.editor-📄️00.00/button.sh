#!/bin/bash
# button.sh - launcher for agy-editor (INTERACT canvas text edit)
# House pattern: session-isolated UI state + shared system/ops/pal.
ACTION="${1:-help}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

case "$ACTION" in
    compile|c|build)
        bash "$SCRIPT_DIR/scripts/build.sh"
        ;;
    run|r|start)
        cd "$SCRIPT_DIR"
        # Same profile auto-detect as &.widgits/file-menu/button.sh,
        # copied exactly (not reinvented): no DISPLAY -> app (ASCII
        # fallback, honest degrade); DISPLAY set -> widget (GL primary).
        PROFILE="${RUN_PROFILE:-}"
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
                 "$SESSION_DIR/projects/agy-editor/manager"
        # docs/ deliberately NOT in the mkdir -p list above (PITFALL 62,
        # 2026-07-30, found via 102.agy-txt's own real save/load testing
        # this session, then confirmed to affect this project's own
        # button.sh too, unnoticed until now): `mkdir -p` pre-creating
        # SESSION_DIR/docs as a REAL directory means the `ln -sfn`
        # below does NOT replace it with a symlink - `ln` treats an
        # EXISTING directory target as "put the symlink INSIDE it",
        # landing a nested SESSION_DIR/docs/docs -> real docs/ symlink
        # one level deeper than intended. Every SAVE_AS using a
        # relative "docs/..." path (do_save_to()'s own plain fopen(path,
        # "w"), cwd = SESSION_DIR) silently wrote into the wrong,
        # ephemeral SESSION_DIR/docs/ instead of the durable house-level
        # docs/ - never caught because this project's own real harness
        # (test-harn-ed-app) always used absolute save paths, which
        # never touch this code path at all. Removing docs/ from mkdir
        # -p lets `ln -sfn` create the symlink directly at
        # SESSION_DIR/docs as originally intended.

        # No symlinks — C processes resolve shared/persistent files via PRISC_PROJECT_ROOT env var

        cd "$SESSION_DIR"
        : > pieces/apps/player_app/interact_relay.txt
        : > pieces/keyboard/history.txt
        : > pieces/display/editor_screen_changed.txt
        : > projects/agy-editor/manager/gui_state.txt

        # Seed buffer + cursor (flow: sample line + empty cursor line)
        if [ ! -f pieces/system/editor_buffer.txt ]; then
            printf 'hi   😅 😆 😇\n' > pieces/system/editor_buffer.txt
        fi
        cat > pieces/system/editor_state.txt << 'EOF'
file_path=docs/untitled.txt
cursor_pos=-1
last_message=Welcome to XYZ Editor. Focus EDIT TEXT, Enter to INTERACT.
EOF
        # cursor_pos=-1 means end of buffer

        # House root for ledger discovery
        echo "$(cd "$SCRIPT_DIR/.." && pwd)" > pieces/system/house_root.txt

        cat > pieces/apps/player_app/state.txt << 'EOSTATE'
module_path=system/prisc+x pal/main_loop_chtpm.pal
project_id=agy-editor
active_target_id=editor
EOSTATE

        export PRISC_PROJECT_ROOT="$SCRIPT_DIR"
        export PRISC_PROJECT_ID="agy-editor"

        if [ -x "./ops/+x/editor_compose_frame.+x" ]; then
            ./ops/+x/editor_compose_frame.+x >/dev/null 2>&1 || true
        elif [ -x "$SCRIPT_DIR/ops/+x/editor_compose_frame.+x" ]; then
            "$SCRIPT_DIR/ops/+x/editor_compose_frame.+x" >/dev/null 2>&1 || true
        fi

        ./system/renderer &
        RENDERER_PID=$!
        ./system/chtpm_parser_pal pieces/chtpm/layouts/editor.chtpm >/dev/null 2>&1 &
        CHTPM_PID=$!

        # §35 GL IS PRIMARY UI: same generic pipeline every project uses
        # (compose -> view.txt -> chtpm -> current_frame.txt -> rgb_frame.raw
        # -> gl_mirror), zero editor-specific code needed for this part —
        # editor_compose_frame.c already writes ONLY view.txt (ONE VISIBLE
        # FRAME WRITER, PITFALL 15), same as every other project's RGB path
        # consumes. Config decision, matching &.widgits/file-menu/button.sh's
        # own real (not aspirational) conditional exactly: GL when explicitly
        # requested (PROFILE=widget) OR auto-detected (PROFILE=app + DISPLAY
        # set); NO_GL=1 forces ASCII-only regardless (harness/headless use).
        # A real headless box still gets the honest ASCII fallback instead of
        # a GL window that can't open — this is what "config decides which
        # to open" means in practice, not a silent omission of the law.
        GL_PID=""
        RGB_PID=""
        if [ "$PROFILE" = "widget" ] || ([ "$PROFILE" = "app" ] && [ -n "$DISPLAY" ]); then
            # THREE-LAYER RACE FIX (PITFALL 54, @.apps/text-editor-xyz/
            # widget-bug-fix-j29.txt): chtpm_rgb_render's own initial
            # synchronous render + its baseline pulse-size read BOTH
            # happen at process start. If either happens before
            # chtpm_parser_pal's first compose_frame() has ever run,
            # the result is a permanently-stuck all-black rgb_frame.raw
            # — not transient, forever, because the renderer's poll
            # loop only re-renders on a pulse-size CHANGE, and if the
            # baseline read already caught the post-compose size, it
            # never sees one. Under single-project testing this is
            # unlikely (few competing processes, parser wins the race
            # most of the time by luck) — this is exactly why it looked
            # fixed in isolated testing this session and then failed
            # for real. Wait for current_frame.txt to actually exist
            # (parser's first real compose) before starting either GL
            # process at all.
            waited=0
            while [ ! -s pieces/display/current_frame.txt ] && [ "$waited" -lt 20 ]; do
                sleep 0.1
                waited=$((waited + 1))
            done
            if [ -z "$NO_GL" ] && [ -x ./system/gl_mirror ]; then
                ./system/gl_mirror >/dev/null 2>&1 &
                GL_PID=$!
            fi
            if [ -z "$NO_GL" ] && [ -x ./system/chtpm_rgb_render ]; then
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

        trap 'if [ -x ./ops/+x/ledger_append.+x ]; then PRISC_PROJECT_ROOT="$SCRIPT_DIR" ./ops/+x/ledger_append.+x OFFLINE editor agy-editor "$SESSION_DIR" $$ "XYZ Editor" pieces/system/widget_cmds/inbox.txt >/dev/null 2>&1 || true; fi; kill "$RENDERER_PID" "$CHTPM_PID" "$GL_PID" "$RGB_PID" 2>/dev/null; kill_own_module; rm -rf "$SESSION_DIR"' EXIT INT TERM

        : > pieces/apps/player_app/history.txt

        # Register in xyzfs runtime ledger
        if [ -x ./ops/+x/ledger_append.+x ]; then
            PRISC_PROJECT_ROOT="$SCRIPT_DIR" \
                ./ops/+x/ledger_append.+x ONLINE editor agy-editor "$SESSION_DIR" $$ \
                "XYZ Editor" pieces/system/widget_cmds/inbox.txt >/dev/null 2>&1 || true
        fi

        ./system/keyboard_input

        kill "$RENDERER_PID" "$CHTPM_PID" "$GL_PID" "$RGB_PID" 2>/dev/null
        kill_own_module
        ;;
    kill|k|stop)
        pkill -f "system/keyboard_input" 2>/dev/null
        pkill -f "system/renderer" 2>/dev/null
        pkill -f "system/prisc\+x" 2>/dev/null
        pkill -f "system/chtpm_parser_pal" 2>/dev/null
        pkill -f "system/gl_mirror" 2>/dev/null
        pkill -f "system/chtpm_rgb_render" 2>/dev/null
        echo "done"
        ;;
    check|verify)
        for b in system/prisc+x system/keyboard_input system/renderer \
                 system/chtpm_parser_pal ops/+x/editor_menu_input.+x \
                 ops/+x/editor_compose_frame.+x; do
            if [ -x "$SCRIPT_DIR/$b" ]; then echo "OK   $b"; else echo "MISSING $b"; fi
        done
        for b in system/gl_mirror system/chtpm_rgb_render; do
            if [ -x "$SCRIPT_DIR/$b" ]; then echo "OK   $b (GL/RGB, §35)"; else echo "OPTIONAL-MISSING $b (ASCII-only fallback; run ./button.sh compile with wsr-pal present)"; fi
        done
        ;;
    help|h|-h|--help)
        echo "xyz-editor button.sh"
        echo ""
        echo "Usage: ./button.sh <action>"
        echo "  compile, c, build   - Build prisc+x + ops (+ copy GL/RGB from wsr-pal)"
        echo "  run, r              - Interactive editor (INTERACT canvas)"
        echo "  kill, k, stop       - Kill lingering processes"
        echo "  check, verify       - Verify binaries exist"
        echo "  help, h             - Show this help"
        echo ""
        echo "GL / RGB window (§35 GL-primary):"
        echo "  Auto-opens if DISPLAY is set (same auto-detect as &.widgits/file-menu)."
        echo "  RUN_PROFILE=widget  - Force GL on regardless of DISPLAY-detection path"
        echo "  NO_GL=1             - Force ASCII-only even with DISPLAY set (headless/harness use)"
        ;;
    *)
        echo "Unknown action: $ACTION"
        exit 1
        ;;
esac
