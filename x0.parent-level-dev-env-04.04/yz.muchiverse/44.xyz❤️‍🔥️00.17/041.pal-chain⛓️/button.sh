#!/bin/bash
# button.sh - launcher for pal-chain, modeled directly on wsr-pal's own
# button.sh (real interact+module chtpm pattern, proven live earlier
# this session - see that file's own header comment for the full
# rationale this reuses verbatim).
#
# "run" launches, alongside the normal chtpm/renderer/keyboard trio, one
# persistent palnet_peer.+x instance (own_kind=chain_node, seek_kind=
# chain_node - PAL-NET-STANDARD.txt's own full-mesh precedent, sec. 0 of
# PAL-CHAIN-STANDARD.txt) so this node is discoverable by, and discovers,
# every other chain_node on the network the instant it starts - required
# for the "mine against other headless nodes" scenario. chain_miner.+x
# and chain_inbox_watcher.+x are NOT launched by "run" itself - they're
# started on demand from the Wallet menu's own "Start Mining" action
# (see ops/chain_menu_input.c), since a node might just want to browse a
# wallet without ever mining.
ACTION="${1:-help}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

case "$ACTION" in
    compile|c|build)
        bash "$SCRIPT_DIR/scripts/build.sh"
        gcc -o "$SCRIPT_DIR/system/orchestrator" "$SCRIPT_DIR/system/orchestrator.c" 2>/dev/null \
            && echo "OK   system/orchestrator" || echo "SKIP system/orchestrator"
        ;;
    run|r|start)
        cd "$SCRIPT_DIR"
        if [ ! -x "$SCRIPT_DIR/system/orchestrator" ]; then
            echo "Compiling orchestrator..."
            gcc -o "$SCRIPT_DIR/system/orchestrator" "$SCRIPT_DIR/system/orchestrator.c" 2>/dev/null
        fi
        # SESSION ISOLATION (xyzos-standards.txt sec. 23) - backported
        # from pal-forum's own proven "run" action (built first there,
        # live-verified, then pal-chat-irc built session-isolated from
        # day one - this is the same pattern applied a third time, no
        # design work re-done). Every "run" gets a private, throwaway
        # directory for its own ephemeral UI state, deleted on exit.
        # SHARED (symlinked, never copied): system/ops/pal/pieces-chtpm/
        # default_op.txt/projects pieces (static, read-only) and
        # wallets/, data/ (the REAL persistent ledger + wallet database
        # every session of this project must see and mutate together -
        # same role pal-forum's own users/ plays).
        SESSION_ID="$(date +%s)-$$"
        SESSION_DIR="$SCRIPT_DIR/pieces/sessions/$SESSION_ID"
        mkdir -p "$SESSION_DIR/pieces/system" "$SESSION_DIR/pieces/display" \
                 "$SESSION_DIR/pieces/apps/player_app" "$SESSION_DIR/pieces/keyboard" \
                 "$SESSION_DIR/pieces/os" "$SESSION_DIR/net" "$SESSION_DIR/projects/pal-chain/manager"
        mkdir -p "$SCRIPT_DIR/wallets" "$SCRIPT_DIR/data"
        # pieces/os/proc_list.txt (written by orchestrator at runtime) stays
        # REAL and session-local - see kill_all.sh's own header for why.
        # No symlinks — C processes resolve shared/persistent files via PRISC_PROJECT_ROOT env var
        cd "$SESSION_DIR"
        : > pieces/apps/player_app/interact_relay.txt
        : > pieces/keyboard/history.txt
        : > pieces/system/quit_flag.txt
        : > pieces/display/chain_screen_changed.txt
        : > net/outbox.txt
        : > net/inbox.txt
        # Real chtpm-native <cli_io target_id="..."> fields (login/signup/
        # send_screen.chtpm) persist their typed values here across
        # keystrokes AND across screens (chtpm_parser_pal.c's own
        # save_cli_io_gui_state()) - a fresh, empty file every session by
        # construction now (this session's own private directory), so a
        # previous session's leftover wallet_id/password can never
        # pre-fill a fresh one.
        : > projects/pal-chain/manager/gui_state.txt
        : > pieces/apps/player_app/cli_buffers.txt

        cat > pieces/apps/player_app/state.txt << 'EOSTATE'
module_path=system/prisc+x pal/main_loop_chtpm.pal
project_id=pal-chain
active_target_id=login
EOSTATE

        export PRISC_PROJECT_ROOT="$SCRIPT_DIR"
        export PRISC_PROJECT_ID="pal-chain"
        # REAL BUG, LIVE-CAUGHT while building pal-chat-irc (2026-07-20,
        # PAL-NET-STANDARD.txt sec. 1/xyzos-standards.txt sec. 23's own
        # writeup): palnet_peer.c's own resolve_presence_root() defaults
        # to "one level above PRISC_PROJECT_ROOT" - wrong once that's a
        # SESSION_DIR, silently resolving to a directory private to this
        # one session's own parent instead of the real, shared presence
        # dir every other project's own node discovers through. Pinned
        # explicitly here from day one (pal-forum/pal-chat-irc needed a
        # retrofit for this same bug; pal-chain gets it correct from the
        # start since this whole button.sh is being rewritten anyway).
        export PRISC_NET_ROOT="$SCRIPT_DIR/../net/presence"

        # system/orchestrator.c launches+tracks renderer, chtpm_parser_pal,
        # chtpm_rgb_render, and palnet_peer itself (pieces/os/proc_list.txt,
        # session-scoped 3-layer kill) - runs in the BACKGROUND here so
        # keyboard_input below stays the foreground command (this
        # project's real exit-on-Ctrl+C UX) - see orchestrator.c's own
        # header comment for the full rationale.
        export PAL_LAYOUT="pieces/chtpm/layouts/login.chtpm"
        "$SCRIPT_DIR/system/orchestrator" 2>>pieces/system/orchestrator.log &
        ORCH_PID=$!

        # chtpm_parser_pal has no SIGTERM handler of its own; its own
        # spawned module (system/prisc+x) must be reaped by cwd match,
        # not a bare `pkill -f` (argv text is identical across every
        # session) - see pal-forum/button.sh's own identical comment.
        kill_own_module() {
            local pid cwd
            for pid in $(pgrep -f "system/prisc\+x" 2>/dev/null); do
                cwd="$(readlink -f "/proc/$pid/cwd" 2>/dev/null)"
                cwd="${cwd% (deleted)}"
                if [ "$cwd" = "$SESSION_DIR" ]; then
                    kill -9 "$pid" 2>/dev/null
                fi
            done
        }

        # Deletes ONLY this session's own private directory - never the
        # real, shared system/ops/pal/wallets/data (symlinks; `rm -rf`
        # on the session dir removes the symlink itself, not its target).
        # Signaling ORCH_PID triggers its own full cascade cleanup
        # (renderer + chtpm_parser_pal + chtpm_rgb_render + palnet_peer +
        # a session-scoped kill_all.sh sweep).
        trap 'kill "$ORCH_PID" 2>/dev/null; wait "$ORCH_PID" 2>/dev/null; kill_own_module; rm -rf "$SESSION_DIR"' EXIT INT TERM

        : > pieces/apps/player_app/history.txt

        ./system/keyboard_input

        kill "$ORCH_PID" 2>/dev/null
        kill_own_module
        ;;
    kill|k|stop)
        pkill -f "system/keyboard_input" 2>/dev/null
        pkill -f "system/renderer" 2>/dev/null
        pkill -f "system/prisc\+x" 2>/dev/null
        pkill -f "system/chtpm_parser_pal" 2>/dev/null
        pkill -f "system/chtpm_rgb_render" 2>/dev/null
        pkill -f "system/orchestrator" 2>/dev/null
        pkill -f "ops/\+x/palnet_peer" 2>/dev/null
        pkill -f "ops/\+x/chain_miner" 2>/dev/null
        pkill -f "ops/\+x/chain_inbox_watcher" 2>/dev/null
        bash "$SCRIPT_DIR/pieces/os/kill_all.sh" 2>/dev/null
        echo "done"
        ;;
    check|verify)
        for b in system/prisc+x system/keyboard_input system/renderer \
                 system/chtpm_parser_pal system/orchestrator \
                 ops/+x/chain_create_wallet.+x \
                 ops/+x/chain_login.+x ops/+x/chain_balance.+x \
                 ops/+x/chain_send.+x ops/+x/chain_miner.+x \
                 ops/+x/chain_inbox_watcher.+x ops/+x/chain_menu_input.+x \
                 ops/+x/chain_compose_frame.+x \
                 ops/+x/palnet_peer.+x; do
            if [ -x "$SCRIPT_DIR/$b" ]; then echo "OK   $b"; else echo "MISSING $b"; fi
        done
        ;;
    help|h|-h|--help)
        echo "pal-chain button.sh"
        echo ""
        echo "Usage: ./button.sh <action>"
        echo "  compile, c, build   - Build prisc+x + ops"
        echo "  run, r              - THE REAL PLAYABLE UI (interactive, needs a real terminal)"
        echo "  kill, k, stop       - Kill any lingering pal-chain processes"
        echo "  check, verify       - Verify all binaries exist"
        echo "  help, h             - Show this help"
        ;;
    *)
        echo "Unknown action: $ACTION"
        exit 1
        ;;
esac
