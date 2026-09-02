#!/bin/bash
# avatar-creation launcher - muchi-pals shaped clone manager.
# Reads identity from sibling 00.login-signup (current_login + xyzfs).
#
# Quit safety: session trap writes quit_flag, runs kill_all.sh (TERM->KILL
# escalate, reaps avatar_window desktop pets so they cannot keep spinning
# at ~60fps after the terminal exits).
ACTION="${1:-help}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
LOGIN_ROOT=""
for d in "$SCRIPT_DIR/../00.login-signup" "$SCRIPT_DIR"/../00.login-signup*; do
    if [ -d "$d" ]; then LOGIN_ROOT="$(cd "$d" && pwd)"; break; fi
done
KILL_ALL="$SCRIPT_DIR/pieces/os/kill_all.sh"

case "$ACTION" in
    compile|c|build)
        bash "$SCRIPT_DIR/scripts/build.sh"
        ;;
    run|r|start)
        cd "$SCRIPT_DIR"
        # Pre-run sweep: no orphan windows/UI from a previous crash
        bash "$KILL_ALL" 2>/dev/null || true

        SESSION_ID="$(date +%s)-$$"
        SESSION_DIR="$SCRIPT_DIR/pieces/sessions/$SESSION_ID"
        mkdir -p "$SESSION_DIR/pieces/system" "$SESSION_DIR/pieces/display" \
                 "$SESSION_DIR/pieces/apps/player_app" "$SESSION_DIR/pieces/keyboard" \
                 "$SESSION_DIR/projects/avatar-creation/manager" \
                 "$SESSION_DIR/pieces/os" \
                 "$SESSION_DIR/pieces/world_01/map_lobby/user_01"
        mkdir -p "$SCRIPT_DIR/pieces/world_01/map_lobby/user_01" \
                 "$SCRIPT_DIR/pieces/system" "$SCRIPT_DIR/pieces/os"
        touch "$SCRIPT_DIR/pieces/world_01/map_lobby/user_01/inventory.txt"
        : > "$SCRIPT_DIR/pieces/system/avatar_window_pids.txt"
        if [ ! -f "$SCRIPT_DIR/pieces/world_01/map_lobby/user_01/state.txt" ]; then
            printf 'name=user_01\ntype=user\ntokens=0\n' > "$SCRIPT_DIR/pieces/world_01/map_lobby/user_01/state.txt"
        fi

        cp -r "$SCRIPT_DIR/system" "$SESSION_DIR/system"
        cp -r "$SCRIPT_DIR/ops" "$SESSION_DIR/ops"
        cp -r "$SCRIPT_DIR/pal" "$SESSION_DIR/pal"
        cp -r "$SCRIPT_DIR/default_op.txt" "$SESSION_DIR/default_op.txt"
        cp -r "$SCRIPT_DIR/pieces/chtpm" "$SESSION_DIR/pieces/chtpm"
        cp -r "$SCRIPT_DIR/pieces/registry" "$SESSION_DIR/pieces/registry"
        cp -r "$SCRIPT_DIR/projects/avatar-creation/pieces" "$SESSION_DIR/projects/avatar-creation/pieces"
        cp -r "$SCRIPT_DIR/pieces/world_01" "$SESSION_DIR/pieces/world_01"
        cp -r "$KILL_ALL" "$SESSION_DIR/pieces/os/kill_all.sh"
        cp -r "$SCRIPT_DIR/pieces/system/avatar_window_pids.txt" \
            "$SESSION_DIR/pieces/system/avatar_window_pids.txt" 2>/dev/null || true

        cd "$SESSION_DIR"
        : > pieces/apps/player_app/interact_relay.txt
        : > pieces/keyboard/history.txt
        : > pieces/display/avatar_screen_changed.txt
        : > pieces/system/quit_flag.txt
        : > projects/avatar-creation/manager/gui_state.txt
        cat > pieces/system/avatar_menu_state.txt << 'EOF'
last_message=Welcome - faucet, store, then customize your clones.
selected_avatar=
last_screen=main
EOF
        cat > pieces/apps/player_app/state.txt << 'EOSTATE'
module_path=system/prisc+x pal/main_loop_chtpm.pal
project_id=avatar-creation
active_target_id=main
EOSTATE

        export PRISC_PROJECT_ROOT="$SESSION_DIR"
        export PRISC_PROJECT_ID="avatar-creation"
        export USERPAL_LOGIN_ROOT="${LOGIN_ROOT:-$SCRIPT_DIR}"

        # Durable identity (guest-<uuid> or logged-in) + pull clones into lobby
        if [ -x "./ops/+x/ensure_user_identity.+x" ]; then
            ./ops/+x/ensure_user_identity.+x >/dev/null 2>&1 || true
        fi
        if [ -x "./ops/+x/hydrate_avatars.+x" ]; then
            ./ops/+x/hydrate_avatars.+x >/dev/null 2>&1 || true
        fi

        if [ -x "./ops/+x/avatar_compose_frame.+x" ]; then
            ./ops/+x/avatar_compose_frame.+x >/dev/null 2>&1 || true
        fi

        ./system/renderer &
        RENDERER_PID=$!
        ./system/chtpm_parser_pal pieces/chtpm/layouts/main.chtpm >/dev/null 2>&1 &
        CHTPM_PID=$!

        # Copy-back half of the copy-in/copy-out write-through emulation
        # for pieces/world_01/ (clones minted, tokens claimed, DNA cycled,
        # inventory changes - every gameplay op writes these via
        # project_root = the session copy). Must run BEFORE rm -rf deletes
        # the session dir. Identity (current_login/session.pdl/xyzfs homes)
        # needs NO persisting here: those ops write via USERPAL_LOGIN_ROOT,
        # which already points at the REAL 00.login-signup root.
        # avatar_window_pids.txt is deliberately NOT persisted - the real
        # root's copy is truncated at every launch by design (stale PIDs
        # from a dead session are worse than none).
        persist_session_state() {
            cp -r "$SESSION_DIR/pieces/world_01/." "$SCRIPT_DIR/pieces/world_01/" 2>/dev/null
        }

        session_cleanup() {
            # Order matters: soft stop -> kill windows (60fps) -> UI -> module -> session dir
            printf '1\n' > "$SESSION_DIR/pieces/system/quit_flag.txt" 2>/dev/null || true
            printf '1\n' > "$SCRIPT_DIR/pieces/system/quit_flag.txt" 2>/dev/null || true
            # Reap desktop avatar windows first (they detach via setsid)
            bash "$KILL_ALL" "$SESSION_DIR" 2>/dev/null || true
            kill -TERM "$RENDERER_PID" "$CHTPM_PID" 2>/dev/null || true
            sleep 0.15
            kill -9 "$RENDERER_PID" "$CHTPM_PID" 2>/dev/null || true
            # cwd-scoped prisc module
            local pid cwd
            for pid in $(ls /proc 2>/dev/null | grep -E '^[0-9]+$'); do
                cmd=$(tr '\0' ' ' < "/proc/$pid/cmdline" 2>/dev/null) || continue
                case "$cmd" in
                    *"/system/prisc+x"*|*"system/prisc+x "*)
                        cwd="$(readlink -f "/proc/$pid/cwd" 2>/dev/null || true)"
                        cwd="${cwd% (deleted)}"
                        if [ "$cwd" = "$SESSION_DIR" ]; then
                            kill -9 "$pid" 2>/dev/null || true
                        fi
                        ;;
                esac
            done
            persist_session_state
            rm -rf "$SESSION_DIR"
        }
        trap 'session_cleanup' EXIT INT TERM HUP

        : > pieces/apps/player_app/history.txt
        ./system/keyboard_input
        # normal exit falls through trap
        ;;
    kill|k|stop)
        bash "$KILL_ALL"
        # also clear stale sessions left after crash
        rm -rf "$SCRIPT_DIR/pieces/sessions"/* 2>/dev/null || true
        echo "done"
        ;;
    kill-windows|kw)
        bash "$SCRIPT_DIR/scripts/kill_avatar_windows.sh"
        ;;
    demo|d)
        export PRISC_PROJECT_ROOT="$SCRIPT_DIR"
        export USERPAL_LOGIN_ROOT="${LOGIN_ROOT:-$SCRIPT_DIR}"
        bash "$SCRIPT_DIR/scripts/build.sh" >/dev/null
        ID=$(./ops/+x/generate_clone.+x "DemoClone")
        echo "Minted: $ID"
        cat "pieces/world_01/map_lobby/$ID/state.txt"
        ;;
    check|verify)
        for b in system/prisc+x system/keyboard_input system/renderer system/chtpm_parser_pal \
                 system/avatar_window \
                 ops/+x/generate_clone.+x ops/+x/claim_tokens.+x ops/+x/buy_clone.+x \
                 ops/+x/cycle_dna.+x ops/+x/avatar_menu_input.+x ops/+x/avatar_compose_frame.+x \
                 ops/+x/open_avatar_window.+x pieces/os/kill_all.sh; do
            if [ -x "$SCRIPT_DIR/$b" ] || [ -f "$SCRIPT_DIR/$b" ]; then echo "OK   $b"
            else echo "MISSING $b"; fi
        done
        ;;
    help|*)
        echo "avatar-creation button.sh"
        echo "  compile | run | kill | kill-windows | demo | check"
        echo "  kill          - full cleanup (UI + avatar_window + quit_flag)"
        echo "  kill-windows  - only desktop avatar windows"
        ;;
esac
