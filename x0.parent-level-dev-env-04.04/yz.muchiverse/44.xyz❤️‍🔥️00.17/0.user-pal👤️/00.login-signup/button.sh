#!/bin/bash
# button.sh - launcher for user-pal, modeled directly on pal-forum's
# own button.sh (real interact+module chtpm pattern). Session-isolated
# from day one (USER-PAL-STANDARD.txt sec. 3) - built AFTER session
# isolation was already proven in pal-forum, so this is the correct
# starting shape, not a retrofit.
ACTION="${1:-help}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

case "$ACTION" in
    compile|c|build)
        bash "$SCRIPT_DIR/scripts/build.sh"
        ;;
    run|r|start)
        cd "$SCRIPT_DIR"
        # SESSION ISOLATION (xyzos-standards.txt sec. 23, USER-PAL-STANDARD.txt
        # sec. 3) - every "run" gets a private, throwaway directory for
        # its own ephemeral UI state (keyboard history, interact_relay,
        # gui_state), deleted on exit.
        #
        # SHARED, PERSISTENT, NEVER SESSION-SCOPED (copied in at launch,
        # copied back out at exit): users/ (the real identity registry)
        # and current_login.txt (USER-PAL-STANDARD.txt sec. 2 - deliberately
        # NOT like pal-forum's own net/session.txt; this file must outlive
        # any one session, or "save logged in user data in one place" would
        # mean nothing the moment a terminal closes).
        SESSION_ID="$(date +%s)-$$"
        SESSION_DIR="$SCRIPT_DIR/pieces/sessions/$SESSION_ID"
        mkdir -p "$SESSION_DIR/pieces/system" "$SESSION_DIR/pieces/display" \
                 "$SESSION_DIR/pieces/apps/player_app" "$SESSION_DIR/pieces/keyboard" \
                 "$SESSION_DIR/projects/user-pal/manager"
        mkdir -p "$SCRIPT_DIR/users"  # shared, real - not session-scoped
        # xyzfs/ is the multi-user filesystem: xyzfs/users/<uuid>/...
        # Shared + durable (never session-scoped). Created empty at first run;
        # per-user trees appear under users/<uuid>/ at signup.
        mkdir -p "$SCRIPT_DIR/xyzfs/bin" "$SCRIPT_DIR/xyzfs/users"
        touch "$SCRIPT_DIR/current_login.txt"  # ensure the real file exists
        # Ensure guest session.pdl exists until first real login
        if [ ! -f "$SCRIPT_DIR/xyzfs/session.pdl" ]; then
            cat > "$SCRIPT_DIR/xyzfs/session.pdl" << 'EOF'
SECTION      | KEY                | VALUE
----------------------------------------
META         | piece_id           | xyzfs_session
META         | version            | 1.0
STATE        | mode                 | guest
STATE        | user_id              | 
STATE        | user_uuid            | 
STATE        | display_name         | Guest
STATE        | xyzfs_path           | 
STATE        | logged_in_at         | 0
STATE        | active_avatar_uuid   | 
STATE        | active_avatar_path   | 
EOF
        fi
        cp -r "$SCRIPT_DIR/xyzfs" "$SESSION_DIR/xyzfs" 2>/dev/null || true
        cp -r "$SCRIPT_DIR/system" "$SESSION_DIR/system"
        cp -r "$SCRIPT_DIR/ops" "$SESSION_DIR/ops"
        cp -r "$SCRIPT_DIR/pal" "$SESSION_DIR/pal"
        cp -r "$SCRIPT_DIR/default_op.txt" "$SESSION_DIR/default_op.txt"
        cp -r "$SCRIPT_DIR/pieces/chtpm" "$SESSION_DIR/pieces/chtpm"
        cp -r "$SCRIPT_DIR/projects/user-pal/pieces" "$SESSION_DIR/projects/user-pal/pieces"
        cp -r "$SCRIPT_DIR/users" "$SESSION_DIR/users"
        cp -r "$SCRIPT_DIR/xyzfs" "$SESSION_DIR/xyzfs"
        cp -r "$SCRIPT_DIR/current_login.txt" "$SESSION_DIR/current_login.txt"

        cd "$SESSION_DIR"
        : > pieces/apps/player_app/interact_relay.txt
        : > pieces/keyboard/history.txt
        : > pieces/display/userpal_screen_changed.txt
        : > projects/user-pal/manager/gui_state.txt

        cat > pieces/system/userpal_menu_state.txt << 'EOF'
last_message=Welcome to user-pal.
EOF
        cat > pieces/apps/player_app/state.txt << 'EOSTATE'
module_path=system/prisc+x pal/main_loop_chtpm.pal
project_id=user-pal
active_target_id=login
EOSTATE

        export PRISC_PROJECT_ROOT="$SESSION_DIR"
        export PRISC_PROJECT_ID="user-pal"
        # Per-user xyzfs homes are HOUSE-level state (<house>/xyzfs/users/<uuid>,
        # clean schema §9 - see ops' resolve_house_root()). Under symlinks the
        # ops found the house root by realpath()-ing through the session links;
        # plain copies break that walk (it would land on pieces/), so pin it
        # explicitly - resolve_house_root() checks this env var FIRST.
        export HOUSE_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

        # PRE-SEED view.txt BEFORE chtpm's first load_vars/compose_frame.
        # Real bug (live-caught 2026-07-27 via login harness): chtpm_parser
        # runs compose_frame() once at startup; if pieces/apps/player_app/
        # view.txt does not exist yet it caches game_map="[Map Loading...]".
        # The persistent pal loop then writes a real view.txt and pings
        # frame_changed.txt, but that ping often lands before the main
        # loop samples last_display_pulse_size - so the marker never
        # "grows" again and the first paint stays Map Loading until the
        # next keypress. Humans and harnesses both saw this. Seeding
        # compose once here makes the first load_vars see real content.
        if [ -x "./ops/+x/userpal_compose_frame.+x" ]; then
            ./ops/+x/userpal_compose_frame.+x >/dev/null 2>&1 || true
        elif [ -x "$SCRIPT_DIR/ops/+x/userpal_compose_frame.+x" ]; then
            "$SCRIPT_DIR/ops/+x/userpal_compose_frame.+x" >/dev/null 2>&1 || true
        fi

        ./system/renderer &
        RENDERER_PID=$!
        ./system/chtpm_parser_pal pieces/chtpm/layouts/login.chtpm >/dev/null 2>&1 &
        CHTPM_PID=$!

        # See pal-forum/button.sh's own identical comment: chtpm_parser_pal
        # has no SIGTERM handler of its own, so its own spawned module
        # (system/prisc+x) must be reaped by cwd match, not a bare
        # `pkill -f` (argv text is identical across every session).
        kill_own_module() {
            local pid cwd
            for pid in $(pgrep -f "system/prisc\+x" 2>/dev/null); do
                cwd="$(readlink -f "/proc/$pid/cwd" 2>/dev/null)"
                if [ "$cwd" = "$SESSION_DIR" ]; then
                    kill -9 "$pid" 2>/dev/null
                fi
            done
        }

        # Copy-back half of the copy-in/copy-out write-through emulation
        # for users/, current_login.txt and xyzfs/session.pdl (see their
        # own copy-in comments above) - must run BEFORE rm -rf deletes
        # the session dir, so accounts created / logins recorded this
        # session aren't silently lost. Scope note: per-user xyzfs TREES
        # are NOT persisted here - with HOUSE_ROOT exported above, the ops
        # write those directly to the real house xyzfs mid-session (same
        # as the symlink era); only session.pdl (login/logout state) is
        # written into the session's own xyzfs copy.
        persist_session_state() {
            cp -r "$SESSION_DIR/users/." "$SCRIPT_DIR/users/" 2>/dev/null
            cp -p "$SESSION_DIR/current_login.txt" "$SCRIPT_DIR/current_login.txt" 2>/dev/null
            cp -p "$SESSION_DIR/xyzfs/session.pdl" "$SCRIPT_DIR/xyzfs/session.pdl" 2>/dev/null
        }

        # Deletes ONLY this session's own private directory - never the
        # real, shared system/ops/pal/users/current_login.txt (copies;
        # `rm -rf` on the session dir only ever touched symlinked-in
        # files back when they were symlinks; now everything here is a
        # private copy, and persist_session_state has already fanned the
        # real state back out).
        trap 'kill "$RENDERER_PID" "$CHTPM_PID" 2>/dev/null; kill_own_module; persist_session_state; rm -rf "$SESSION_DIR"' EXIT INT TERM

        : > pieces/apps/player_app/history.txt

        ./system/keyboard_input

        kill "$RENDERER_PID" "$CHTPM_PID" 2>/dev/null
        kill_own_module
        ;;
    kill|k|stop)
        # NOTE: same blunt shape as every other project's own "kill"
        # debug action (pal-forum/pal-chain button.sh) - matches by
        # binary name only, so it is NOT session-scoped and NOT
        # project-scoped (a plain `pkill -f "system/prisc\+x"` matches
        # every project's own module, not just user-pal's). This is
        # fine for a manual debug helper a human runs deliberately; the
        # real "run" path's own exit trap uses kill_own_module() (cwd-
        # scoped) instead, never this. Do not use "kill" while another
        # project's own session might be running.
        pkill -f "system/keyboard_input" 2>/dev/null
        pkill -f "system/renderer" 2>/dev/null
        pkill -f "system/prisc\+x" 2>/dev/null
        pkill -f "system/chtpm_parser_pal" 2>/dev/null
        echo "done"
        ;;
    check|verify)
        for b in system/prisc+x system/keyboard_input system/renderer \
                 system/chtpm_parser_pal ops/+x/userpal_create_account.+x \
                 ops/+x/userpal_login.+x ops/+x/userpal_logout.+x \
                 ops/+x/userpal_whoami.+x ops/+x/userpal_menu_input.+x \
                 ops/+x/userpal_compose_frame.+x; do
            if [ -x "$SCRIPT_DIR/$b" ]; then echo "OK   $b"; else echo "MISSING $b"; fi
        done
        ;;
    help|h|-h|--help)
        echo "user-pal button.sh"
        echo ""
        echo "Usage: ./button.sh <action>"
        echo "  compile, c, build   - Build prisc+x + ops"
        echo "  run, r              - THE REAL PLAYABLE UI (interactive, needs a real terminal)"
        echo "  kill, k, stop       - Kill any lingering user-pal processes"
        echo "  check, verify       - Verify all binaries exist"
        echo "  help, h             - Show this help"
        ;;
    *)
        echo "Unknown action: $ACTION"
        exit 1
        ;;
esac
