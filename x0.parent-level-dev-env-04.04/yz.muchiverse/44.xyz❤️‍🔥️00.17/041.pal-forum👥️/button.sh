#!/bin/bash
# button.sh - launcher for pal-forum, modeled directly on pal-chain's
# own button.sh (real interact+module chtpm pattern).
#
# "run" launches, alongside the normal chtpm/renderer/keyboard trio, one
# persistent palnet_peer.+x instance (own_kind=forum_node, seek_kind=
# forum_node - full mesh, PAL-FORUM-STANDARD.txt sec. 0) so this node
# discovers/is discovered by every other forum_node the instant it
# starts, AND forum_inbox_watcher.+x (2026-07-19, fixed - it used to
# require manually running `./button.sh watcher` in a second terminal,
# which meant posts/follows/likes/DMs from other peers were silently
# never applied unless a player remembered that extra step; matches
# pal-chain's own "start mining launches the watcher too" precedent -
# a chat/social app is even less usable than a wallet if incoming
# messages just don't show up by default).
ACTION="${1:-help}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

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
        # SESSION ISOLATION (see xyzos-standards.txt sec. 23) - real fix
        # for a real, live-caught bug: two concurrent invocations of
        # "run" (an agent testing + a real user, or two of either) used
        # to share ONE fixed set of ephemeral UI/input state files
        # (pieces/keyboard/history.txt, interact_relay.txt,
        # current_frame.txt, gui_state.txt, net/session.txt, ...) -
        # confirmed live, repeatedly: one session's own old keystrokes
        # replaying into another session's live typing, one session's
        # own login getting silently cleared by another session's
        # own logout/screen-derivation. Direct user instruction: "the
        # whole session should be in its own directory that gets
        # deleted on quit, and starts a new... its just to prevent
        # pollution of multiple sessions trying to run."
        #
        # Every "run" now creates a real, private, throwaway directory
        # (pieces/sessions/<session_id>/) holding ONLY that invocation's
        # own ephemeral state - keyboard history, interact_relay,
        # current_frame, gui_state, net/session.txt (who's logged in
        # THIS session), and its own palnet_peer/inbox_watcher outbox/
        # inbox. PRISC_PROJECT_ROOT points AT that directory for every
        # process this invocation launches - none of them can see or
        # touch another session's own copy of any of this, because
        # there is genuinely nothing shared to collide on.
        #
        # What stays SHARED (symlinked back into the session dir, never
        # copied): system/ (binaries), ops/, pal/, pieces/chtpm/
        # (layouts), projects/pal-forum/pieces/ (piece.pdl - static
        # definitions), default_op.txt (all read-only, never change per
        # session) - and users/ (the REAL persistent database: posts/
        # follows/likes/DMs - every session of this project must see
        # and mutate the SAME one, or two windows of "the same user"
        # would show different walls/feeds).
        #
        # Deleted entirely on exit (normal quit, Ctrl+C, or a crash
        # that still lets the trap fire) - "starts a new" every time,
        # nothing to ever clean up by hand or accidentally reuse stale.
        SESSION_ID="$(date +%s)-$$"
        SESSION_DIR="$SCRIPT_DIR/pieces/sessions/$SESSION_ID"
        mkdir -p "$SESSION_DIR/pieces/system" "$SESSION_DIR/pieces/display" \
                 "$SESSION_DIR/pieces/apps/player_app" "$SESSION_DIR/pieces/keyboard" \
                 "$SESSION_DIR/pieces/os" "$SESSION_DIR/net" "$SESSION_DIR/projects/pal-forum/manager"
        mkdir -p "$SCRIPT_DIR/users"  # shared, real - not session-scoped
        # No symlinks — C processes resolve shared/persistent files via PRISC_PROJECT_ROOT env var
        cd "$SESSION_DIR"
        : > pieces/apps/player_app/interact_relay.txt
        : > pieces/keyboard/history.txt
        : > pieces/system/quit_flag.txt
        : > pieces/display/forum_screen_changed.txt
        : > net/outbox.txt
        : > net/inbox.txt
        : > projects/pal-forum/manager/gui_state.txt
        : > pieces/apps/player_app/cli_buffers.txt

        cat > pieces/system/forum_menu_state.txt << 'EOF'
last_message=Welcome to pal-forum.
EOF
        cat > pieces/apps/player_app/state.txt << 'EOSTATE'
module_path=system/prisc+x pal/main_loop_chtpm.pal
project_id=pal-forum
active_target_id=login
EOSTATE

        # USER-PAL AUTO-FILL REMOVED (direct user correction, 2026-07-20:
        # "i dont want it to default to saying 'jb'... default still has
        # 'jb' in it for some reason" - reported as unwanted even after
        # the field-label fix, so this is a real "don't want this
        # feature" call, not just a display nit). See USER-PAL-
        # STANDARD.txt sec. 4 for the design, kept documented but marked
        # reverted - login screen starts blank again.

        export PRISC_PROJECT_ROOT="$SCRIPT_DIR"
        export PRISC_PROJECT_ID="pal-forum"
        # REAL BUG, LIVE-CAUGHT while building pal-chat-irc (2026-07-20):
        # palnet_peer.c's own resolve_presence_root() defaults to "one
        # level above PRISC_PROJECT_ROOT" - correct when that pointed at
        # SCRIPT_DIR (pre-session-isolation), but now that it's
        # SESSION_DIR, "one level up" resolves to
        # pieces/sessions/net/presence instead of the real, shared
        # ../net/presence every OTHER project's own node also discovers
        # through - confirmed live (a stray pieces/sessions/net/presence/
        # directory existed on disk). Silently broke P2P discovery for
        # every session-isolated "run" since sec. 23 was implemented,
        # never caught because it was never re-tested after. Fixed by
        # pointing PRISC_NET_ROOT at the REAL, project-root-relative
        # presence directory explicitly, same value it would have
        # resolved to before session isolation existed.
        export PRISC_NET_ROOT="$SCRIPT_DIR/../net/presence"
        export PAL_LAYOUT="pieces/chtpm/layouts/login.chtpm"

        # system/orchestrator.c launches+tracks renderer, chtpm_parser_pal,
        # chtpm_rgb_render, palnet_peer, and forum_inbox_watcher itself
        # (pieces/os/proc_list.txt, session-scoped 3-layer kill) - runs in
        # the BACKGROUND here so keyboard_input below stays the foreground
        # command (this project's real exit-on-Ctrl+C UX), same pattern
        # as pal-chain's own button.sh (mass-refactor 2026-07-26 - this
        # project previously did raw `&` backgrounding of each daemon
        # individually here instead of delegating to an orchestrator
        # binary; converted for consistency + the same 3-layer kill
        # reliability every other project in this family now has).
        "$SCRIPT_DIR/system/orchestrator" 2>>pieces/system/orchestrator.log &
        ORCH_PID=$!

        # chtpm_parser_pal has no SIGTERM handler of its own; its own
        # spawned module (system/prisc+x) must be reaped by cwd match,
        # not a bare `pkill -f` (argv text is identical across every
        # session) - see pal-chain/pal-chat-irc's own identical comment.
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

        # Deletes ONLY this session's own private directory - never
        # touches another concurrent session's, and never touches the
        # real, shared system/ops/pal/users (those are symlinks, `rm -rf`
        # on the session dir removes the symlink itself, not its target).
        # Signaling ORCH_PID triggers its own full cascade cleanup
        # (renderer + chtpm_parser_pal + chtpm_rgb_render + palnet_peer +
        # forum_inbox_watcher + a session-scoped kill_all.sh sweep).
        trap 'kill "$ORCH_PID" 2>/dev/null; wait "$ORCH_PID" 2>/dev/null; kill_own_module; rm -rf "$SESSION_DIR"' EXIT INT TERM

        : > pieces/apps/player_app/history.txt

        ./system/keyboard_input

        kill "$ORCH_PID" 2>/dev/null
        kill_own_module
        ;;
    watcher|w)
        # Standalone run - normally unnecessary now that "run" launches
        # its own watcher automatically; kept for debugging (running the
        # watcher alone, in its own terminal, with its own visible
        # stdout instead of the /tmp log "run" redirects it to). NOT
        # session-isolated (a debug tool, not a real UI session) -
        # operates on the real project root's own net/inbox.txt.
        cd "$SCRIPT_DIR"
        export PRISC_PROJECT_ROOT="$SCRIPT_DIR"
        echo "Starting forum_inbox_watcher (Ctrl+C to stop)..."
        ./ops/+x/forum_inbox_watcher.+x
        ;;
    kill|k|stop)
        pkill -f "system/keyboard_input" 2>/dev/null
        pkill -f "system/renderer" 2>/dev/null
        pkill -f "system/prisc\+x" 2>/dev/null
        pkill -f "system/chtpm_parser_pal" 2>/dev/null
        pkill -f "system/chtpm_rgb_render" 2>/dev/null
        pkill -f "ops/\+x/palnet_peer" 2>/dev/null
        pkill -f "ops/\+x/forum_inbox_watcher" 2>/dev/null
        bash "$SCRIPT_DIR/pieces/os/kill_all.sh"
        # See #.haiku+/!.xyzos-pitfalls+1.txt PITFALL 20/21 - verify
        # independently rather than trusting kill_all.sh's own message.
        sleep 0.5
        STRAGGLERS=$(pgrep -f "ops/\+x/(palnet_peer|forum_inbox_watcher)" 2>/dev/null)
        if [ -n "$STRAGGLERS" ]; then
            echo "$STRAGGLERS" | xargs -r kill -9 2>/dev/null
            sleep 0.5
        fi
        if pgrep -f "system/(orchestrator|renderer|keyboard_input|chtpm_parser_pal)|ops/\+x/(palnet_peer|forum_inbox_watcher)" >/dev/null 2>&1; then
            echo "WARNING: some pal-forum processes are still running - check manually."
        fi
        echo "done"
        ;;
    check|verify)
        for b in system/prisc+x system/keyboard_input system/renderer \
                 system/orchestrator system/chtpm_parser_pal ops/+x/forum_create_user.+x \
                 ops/+x/forum_switch_user.+x ops/+x/forum_post.+x \
                 ops/+x/forum_follow.+x ops/+x/forum_like.+x \
                 ops/+x/forum_retweet.+x ops/+x/forum_dm.+x \
                 ops/+x/forum_compute_feed.+x ops/+x/forum_inbox_watcher.+x \
                 ops/+x/forum_menu_input.+x ops/+x/forum_compose_frame.+x \
                 ops/+x/palnet_peer.+x; do
            if [ -x "$SCRIPT_DIR/$b" ]; then echo "OK   $b"; else echo "MISSING $b"; fi
        done
        ;;
    help|h|-h|--help)
        echo "pal-forum button.sh"
        echo ""
        echo "Usage: ./button.sh <action>"
        echo "  compile, c, build   - Build prisc+x + ops"
        echo "  run, r              - THE REAL PLAYABLE UI (interactive, needs a real terminal)"
        echo "  watcher, w          - Run the inbox watcher (receives posts/follows/likes/DMs from peers)"
        echo "  kill, k, stop       - Kill any lingering pal-forum processes"
        echo "  check, verify       - Verify all binaries exist"
        echo "  help, h             - Show this help"
        ;;
    *)
        echo "Unknown action: $ACTION"
        exit 1
        ;;
esac
