#!/bin/bash
# button.sh - launcher for pal-chat-irc, modeled directly on pal-forum's
# own button.sh (real interact+module chtpm pattern). BUILT SESSION-
# ISOLATED AND USER-PAL-INTEGRATED FROM DAY ONE (PAL-CHAT-IRC-
# STANDARD.txt sec. 2/1) - the first project in this family with both
# from its very first version, not retrofitted.
#
# "run" launches, alongside the normal chtpm/renderer/keyboard trio, one
# persistent palnet_peer.+x instance (own_kind=irc_node, seek_kind=
# irc_node - full mesh) and chat_inbox_watcher.+x (started automatically,
# matching pal-forum's own "start mining/watching launches the watcher
# too" precedent - a chat app is even less usable than a wallet if
# incoming messages just don't show up by default).
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
        # SESSION ISOLATION (xyzos-standards.txt sec. 23)
        SESSION_ID="$(date +%s)-$$"
        SESSION_DIR="$SCRIPT_DIR/pieces/sessions/$SESSION_ID"
        mkdir -p "$SESSION_DIR/pieces/system" "$SESSION_DIR/pieces/display" \
                 "$SESSION_DIR/pieces/apps/player_app" "$SESSION_DIR/pieces/keyboard" \
                 "$SESSION_DIR/net" "$SESSION_DIR/projects/pal-chat-irc/manager" \
                 "$SESSION_DIR/debug"
        mkdir -p "$SCRIPT_DIR/users" "$SCRIPT_DIR/rooms" "$SCRIPT_DIR/data"
        # No symlinks — C processes resolve shared/persistent files via PRISC_PROJECT_ROOT env var
        cd "$SESSION_DIR"
        : > pieces/apps/player_app/interact_relay.txt
        : > pieces/keyboard/history.txt
        : > pieces/system/quit_flag.txt
        : > pieces/display/chat_screen_changed.txt
        : > net/outbox.txt
        : > net/inbox.txt
        : > projects/pal-chat-irc/manager/gui_state.txt
        : > pieces/apps/player_app/cli_buffers.txt
        : > debug/frame_history.txt

        cat > pieces/system/chat_menu_state.txt << 'EOF'
last_message=Welcome to pal-chat-irc.
EOF
        cat > pieces/apps/player_app/state.txt << 'EOSTATE'
module_path=system/prisc+x pal/main_loop_chtpm.pal
project_id=pal-chat-irc
active_target_id=login
EOSTATE

        export PRISC_PROJECT_ROOT="$SCRIPT_DIR"
        export PRISC_PROJECT_ID="pal-chat-irc"
        export PRISC_NET_ROOT="$SCRIPT_DIR/../net/presence"

        if [ "$PAL_MODE" -eq 1 ]; then
            export PAL_LAYOUT="pieces/chtpm/layouts/login.chtpm"
        else
            export PAL_LAYOUT=""
        fi

        exec "$SCRIPT_DIR/system/orchestrator"
        ;;
    watcher|w)
        cd "$SCRIPT_DIR"
        export PRISC_PROJECT_ROOT="$SCRIPT_DIR"
        echo "Starting chat_inbox_watcher (Ctrl+C to stop)..."
        ./ops/+x/chat_inbox_watcher.+x
        ;;
    replay-ledger|replay)
        cd "$SCRIPT_DIR"
        export PRISC_PROJECT_ROOT="$SCRIPT_DIR"
        ./ops/+x/chat_replay_ledger.+x
        ;;
    kill|k|stop)
        pkill -f "system/keyboard_input" 2>/dev/null
        pkill -f "system/renderer" 2>/dev/null
        pkill -f "system/prisc\+x" 2>/dev/null
        pkill -f "system/chtpm_parser_pal" 2>/dev/null
        pkill -f "ops/\+x/palnet_peer" 2>/dev/null
        pkill -f "ops/\+x/chat_inbox_watcher" 2>/dev/null
        bash "$SCRIPT_DIR/pieces/os/kill_all.sh"
        # XYZOS-PITFALLS #20/21 (2026-07-26): kill_all.sh's own kill of
        # ops/+x/palnet_peer and ops/+x/chat_inbox_watcher has been
        # observed to be unreliable (pgrep finds them, kill -9 by PID is
        # sent, they're still alive moments later - not root-caused).
        # Verify independently and retry directly by PID before telling
        # the user it's done, instead of trusting the above.
        sleep 0.5
        STRAGGLERS=$(pgrep -f "ops/\+x/(palnet_peer|chat_inbox_watcher)" 2>/dev/null)
        if [ -n "$STRAGGLERS" ]; then
            echo "$STRAGGLERS" | xargs -r kill -9 2>/dev/null
            sleep 0.5
        fi
        if pgrep -f "system/(orchestrator|renderer|keyboard_input|chtpm_parser_pal)|ops/\+x/(palnet_peer|chat_inbox_watcher)" >/dev/null 2>&1; then
            echo "WARNING: some pal-chat-irc processes are still running - run again or check manually with: ps aux | grep -E 'system/(orchestrator|renderer|keyboard_input|chtpm_parser_pal)|ops/\\+x/(palnet_peer|chat_inbox_watcher)'"
        fi
        echo "done"
        ;;
    check|verify)
        for b in system/prisc+x system/keyboard_input system/renderer \
                 system/chtpm_parser_pal ops/+x/chat_create_user.+x \
                 ops/+x/chat_switch_user.+x ops/+x/chat_post_message.+x \
                 ops/+x/chat_inbox_watcher.+x ops/+x/chat_menu_input.+x \
                 ops/+x/chat_compose_frame.+x ops/+x/palnet_peer.+x \
                 ops/+x/chat_replay_ledger.+x; do
            if [ -x "$SCRIPT_DIR/$b" ]; then echo "OK   $b"; else echo "MISSING $b"; fi
        done
        ;;
    help|h|-h|--help)
        echo "pal-chat-irc button.sh"
        echo ""
        echo "Usage: ./button.sh <action> [--pal]"
        echo "  compile, c, build   - Build prisc+x + orchestrator + ops"
        echo "  run, r              - THE REAL PLAYABLE UI (orchestrator manages all processes)"
        echo "  --pal               - Use PAL script mode (passed to run)"
        echo "  watcher, w          - Run the inbox watcher (receives messages from peers)"
        echo "  replay-ledger,replay - Rebuild all rooms/*/messages.txt from data/master_ledger.txt"
        echo "  kill, k, stop       - Kill any lingering pal-chat-irc processes"
        echo "  check, verify       - Verify all binaries exist"
        echo "  help, h             - Show this help"
        ;;
    *)
        echo "Unknown action: $ACTION"
        exit 1
        ;;
esac
