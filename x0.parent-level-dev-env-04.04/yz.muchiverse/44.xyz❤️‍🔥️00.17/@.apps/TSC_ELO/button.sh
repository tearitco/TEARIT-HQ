#!/bin/bash
# button.sh - launcher for TSC_ELO (True Swords Clash).
# Host session modeled directly on my-chara-txt/button.sh (session
# isolation per xyzos-standards §23, symlinked static assets, real
# persistent data/) + text-editor-xyz/button.sh (combined editor+
# widget launch, cmd-bus drainer, widget as a SEPARATE program in a
# background session with TTY->/dev/null). The Match Setup WIDGIT
# (widgets/setup/button.sh) is a distinct program with its own session/
# system/ops/pal/GL window; host <-> widget talk ONLY through
# file-mediated cmd buses (pieces/system/widget_cmds/*).
ACTION="${1:-help}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
HOUSE_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"

case "$ACTION" in
    compile|c|build)
        bash "$SCRIPT_DIR/scripts/build.sh"
        ;;
    run|r|start)
        cd "$SCRIPT_DIR"
        if [ ! -x "$SCRIPT_DIR/system/orchestrator" ]; then
            echo "Compiling..."
            bash "$SCRIPT_DIR/scripts/build.sh"
        fi
        # SESSION ISOLATION (xyzos-standards §23): every "run" gets a
        # private, throwaway directory for ephemeral UI state, deleted
        # on exit. SHARED (symlinked): system/ ops/ pal/ default_op.txt
        # pieces-chtpm/ pieces/registry/ and data/ (the real persistent
        # master ledger every session must see and mutate together).
        SESSION_ID="$(date +%s)-$$"
        SESSION_DIR="$SCRIPT_DIR/pieces/sessions/$SESSION_ID"
        mkdir -p "$SESSION_DIR/pieces/system" "$SESSION_DIR/pieces/display" \
                 "$SESSION_DIR/pieces/apps/player_app" "$SESSION_DIR/pieces/keyboard" \
                 "$SESSION_DIR/pieces/os" "$SESSION_DIR/projects/tsc-elo/manager" \
                 "$SESSION_DIR/net"
        mkdir -p "$SESSION_DIR/pieces/system/widget_cmds"
        mkdir -p "$SCRIPT_DIR/data"
        mkdir -p "$SCRIPT_DIR/net/presence"

        cp -r "$SCRIPT_DIR/pieces/os/kill_all.sh" "$SESSION_DIR/pieces/os/kill_all.sh" 2>/dev/null
        cp -r "$SCRIPT_DIR/system" "$SESSION_DIR/system"
        cp -r "$SCRIPT_DIR/ops" "$SESSION_DIR/ops"
        cp -r "$SCRIPT_DIR/pal" "$SESSION_DIR/pal"
        cp -r "$SCRIPT_DIR/default_op.txt" "$SESSION_DIR/default_op.txt"
        cp -r "$SCRIPT_DIR/pieces/chtpm" "$SESSION_DIR/pieces/chtpm"
        # Glyph bitmaps for chtpm_rgb_render/gl_mirror (PITFALL 52)
        cp -r "$SCRIPT_DIR/pieces/registry" "$SESSION_DIR/pieces/registry" 2>/dev/null
        cp -r "$SCRIPT_DIR/data" "$SESSION_DIR/data"

        cd "$SESSION_DIR"
        : > pieces/apps/player_app/interact_relay.txt
        : > pieces/keyboard/history.txt
        : > pieces/system/quit_flag.txt
        : > pieces/system/player_action.txt
        : > pieces/display/frame_changed.txt
        : > pieces/display/current_frame.txt 2>/dev/null || true
        : > pieces/system/widget_cmds/inbox.txt
        : > pieces/system/widget_cmds/status.txt
        : > net/outbox.txt
        : > net/inbox.txt
        echo "$HOUSE_DIR" > pieces/system/house_root.txt

        # Persistent config.txt (the authoritative duel state) is seeded
        # once in the REAL project dir, then symlinked into this session.
        if [ ! -f "$SCRIPT_DIR/pieces/system/config.txt" ]; then
            mkdir -p "$SCRIPT_DIR/pieces/system"
            cp "$SCRIPT_DIR/pieces/system/config.seed.txt" "$SCRIPT_DIR/pieces/system/config.txt" 2>/dev/null || cat > "$SCRIPT_DIR/pieces/system/config.txt" << 'EOCONFIG'
game_id=tsc_elo-001
mode=HvH
game_state=waiting_setup
current_epoch=1
current_turn=0
num_players=2
player_1_name=Player1
player_1_type=human
player_1_rating=1000
player_1_hp=100
player_1_mana=0
player_1_status=none
player_2_name=SKYNET
player_2_type=computer
player_2_rating=1000
player_2_hp=100
player_2_mana=0
player_2_status=none
EOCONFIG
        fi
        cp -r "$SCRIPT_DIR/pieces/system/config.txt" "$SESSION_DIR/pieces/system/config.txt"

        # Persistent master ledger: seeded once if missing.
        if [ ! -f "$SCRIPT_DIR/data/master_ledger.txt" ]; then
            printf '2026-08-02T00:00:00|1|referee|0|tsc_elo genesis ledger initialized|init\n' \
                > "$SCRIPT_DIR/data/master_ledger.txt"
        fi

        cat > pieces/apps/player_app/state.txt << 'EOSTATE'
module_path=system/prisc+x pal/main_loop_chtpm.pal
project_id=tsc-elo
active_target_id=main
EOSTATE

        export PRISC_PROJECT_ROOT="$SESSION_DIR"
        export PRISC_PROJECT_ID="tsc-elo"
        # PvP discovery: shared flat presence dir so two sessions on one
        # machine find each other by kind (PAL-NET-STANDARD sec. 1).
        export PRISC_NET_ROOT="$SCRIPT_DIR/net/presence"
        export PAL_LAYOUT="pieces/chtpm/layouts/main.chtpm"

        # The host's widget cmd-bus drainer (background): consumes the
        # Setup WIDGIT's MATCH:/RATING:/PLAYER:/START commands.
        if [ -x ./ops/+x/tsc_setup.+x ]; then
            ( while [ -f pieces/system/widget_cmds/inbox.txt ]; do
                  PRISC_PROJECT_ROOT="$SESSION_DIR" ./ops/+x/tsc_setup.+x 8 >/dev/null 2>&1 || true
                  sleep 0.2
              done ) &
            SETUP_DRAIN_PID=$!
        fi

        # Match Setup WIDGIT (W1): separate program, background GL session.
        # setsid => own process group, so host-level timeout/kill cascades
        # never reach the widget (same fix civ-txt's OPEN_BOARD_WIDGET
        # handler applies). Focus = this host session dir (its cmd-bus
        # inbox/status live under pieces/system/widget_cmds/).
        SETUP_WIDGET_PID=""
        if [ -x "$SCRIPT_DIR/widgets/setup/button.sh" ]; then
            cd "$SCRIPT_DIR/widgets/setup"
            setsid env RUN_PROFILE=widget \
                bash "$SCRIPT_DIR/widgets/setup/button.sh" run-widget "$SESSION_DIR" \
                >/dev/null 2>&1 < /dev/null &
            SETUP_WIDGET_PID=$!
            cd "$SESSION_DIR"
        fi

        "$SCRIPT_DIR/system/orchestrator" 2>>pieces/system/orchestrator.log &
        ORCH_PID=$!

        # OPTIONAL GL/RGB mirror (§35 GL-primary): gated on NO_GL + a real
        # DISPLAY, skips gracefully. MUST wait for chtpm_parser_pal's first
        # real compose before launching chtpm_rgb_render (PITFALL 54).
        GL_PID=""
        RGB_PID=""
        if [ -z "$NO_GL" ] && [ -n "$DISPLAY" ]; then
            waited=0
            while [ ! -s pieces/display/current_frame.txt ] && [ "$waited" -lt 20 ]; do
                sleep 0.1
                waited=$((waited + 1))
            done
            if [ -x ./system/gl_mirror ]; then
                ./system/gl_mirror >/dev/null 2>&1 &
                GL_PID=$!
            fi
            if [ -x ./system/chtpm_rgb_render ]; then
                ./system/chtpm_rgb_render >/dev/null 2>&1 &
                RGB_PID=$!
            fi
        fi

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

        cleanup() {
            kill "$ORCH_PID" "$GL_PID" "$RGB_PID" "$SETUP_WIDGET_PID" "$SETUP_DRAIN_PID" 2>/dev/null
            wait "$ORCH_PID" 2>/dev/null
            kill_own_module
            [ -x "$SCRIPT_DIR/widgets/setup/button.sh" ] && bash "$SCRIPT_DIR/widgets/setup/button.sh" kill >/dev/null 2>&1 || true
            persist_session_state; rm -rf "$SESSION_DIR"
        }
        # Step 2 symlink-migration fix: copy mutable session state back
        # to the real project root before the session dir is deleted
        # (the old symlinks made these writes land at the real root for
        # free; cp -r sessions need this explicit copy-back). Merge
        # semantics - adds/overwrites, never deletes. Volatile files
        # (quit_flag, pids, history, relays, gui_state) are NOT copied.
        persist_session_state() {
            mkdir -p "$(dirname "$SCRIPT_DIR/pieces/system/config.txt")" 2>/dev/null || true
            cp -r "$SESSION_DIR/pieces/system/config.txt" "$SCRIPT_DIR/pieces/system/config.txt" 2>/dev/null || true
        }
        trap cleanup EXIT INT TERM

        : > pieces/apps/player_app/history.txt
        ./system/keyboard_input

        cleanup
        ;;
    kill|k|stop)
        pkill -f "system/keyboard_input" 2>/dev/null
        pkill -f "system/renderer" 2>/dev/null
        pkill -f "system/prisc\+x" 2>/dev/null
        pkill -f "system/chtpm_parser_pal" 2>/dev/null
        pkill -f "system/chtpm_rgb_render" 2>/dev/null
        pkill -f "system/gl_mirror" 2>/dev/null
        pkill -f "system/orchestrator" 2>/dev/null
        pkill -f "ops/+x/palnet_peer" 2>/dev/null
        pkill -f "ops/+x/tsc_net" 2>/dev/null
        if [ -x "$SCRIPT_DIR/widgets/setup/button.sh" ]; then
            bash "$SCRIPT_DIR/widgets/setup/button.sh" kill >/dev/null 2>&1 || true
        fi
        echo "done"
        ;;
    check|verify)
        for b in system/prisc+x system/keyboard_input system/renderer \
                 system/chtpm_parser_pal system/orchestrator \
                 ops/+x/tsc_compose.+x ops/+x/tsc_tick.+x ops/+x/tsc_setup.+x \
                 ops/+x/tsc_elo.+x ops/+x/tsc_ai.+x ops/+x/tsc_deal.+x \
                 ops/+x/tsc_input.+x ops/+x/tsc_net.+x \
                 ops/+x/palnet_peer.+x \
                 ops/+x/tsc_miracle.+x \
                 ops/+x/ledger_append.+x ops/+x/ledger_peers.+x; do
            if [ -x "$SCRIPT_DIR/$b" ]; then echo "OK   $b"; else echo "MISSING $b"; fi
        done
        for b in system/chtpm_rgb_render system/gl_mirror; do
            if [ -x "$SCRIPT_DIR/$b" ]; then echo "OK   $b (optional GL mirror)"; else echo "OPTIONAL-MISS $b"; fi
        done
        ;;
    help|h|-h|--help)
        echo "TSC_ELO (True Swords Clash) button.sh"
        echo ""
        echo "Usage: ./button.sh <action>"
        echo "  compile, c, build   - Build system processes + host/widget ops"
        echo "  run, r              - THE REAL PLAYABLE UI (needs a real terminal;"
        echo "                        also launches the Match Setup WIDGIT GL window)"
        echo "  kill, k, stop       - Kill any lingering TSC_ELO processes"
        echo "  check, verify       - Verify all binaries exist"
        echo "  help, h             - Show this help"
        echo ""
        echo "Controls (host, when playing): 1:strike  2:heavy  3:heal  4:block"
        echo "Use the MATCH SETUP window to pick HvH / HvC / CvC + rating, START."
        ;;
    *)
        echo "Unknown action: $ACTION"
        exit 1
        ;;
esac
