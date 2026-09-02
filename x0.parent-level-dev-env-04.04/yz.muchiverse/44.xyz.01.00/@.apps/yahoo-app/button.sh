#!/bin/bash
# button.sh - yahoo-app launcher (bank GUI + broker widget)
#
# Architecture (per xyzos-standards §36):
#   Bank GUI and broker widget are SEPARATE PROGRAMS, each with their
#   own session directory, system binaries, ops, and PAL loops.
#   They communicate via shared state files (usr_acc.<hash>.txt,
#   broker_state.txt) — no cmd bus needed for simple refocus.
#   App launcher starts bank GUI in foreground and kills it on exit.
#   Broker widget is spawned on demand from the bank screen.
#
#   Bank GUI: foreground session (owns TTY, has keyboard_input)
#   Broker widget: background session (widget profile, TTY->/dev/null)
ACTION="${1:-help}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
HOUSE_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"

err() { echo "$*" >&2; exit 1; }

PKILL_PATTERNS='system/keyboard_input system/renderer system/prisc\+x system/chtpm_parser_pal system/gl_mirror system/chtpm_rgb_render system/orchestrator manager/\+x/agy_browser_manager\.\+x'

run_app() {
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

    SESSION_ID="$(date +%s)-$$"
    BANK_SESSION="/tmp/.yahoo-app-bank-$SESSION_ID"

    mkdir -p "$BANK_SESSION/pieces/system" "$BANK_SESSION/pieces/system/widget_cmds" \
             "$BANK_SESSION/pieces/display" "$BANK_SESSION/pieces/apps/player_app" \
             "$BANK_SESSION/pieces/keyboard" "$BANK_SESSION/pieces/os" \
             "$BANK_SESSION/projects/yahoo-app/manager"

    cp -r "$SCRIPT_DIR/system" "$BANK_SESSION/system"
    cp -r "$SCRIPT_DIR/ops" "$BANK_SESSION/ops"
    cp -r "$SCRIPT_DIR/pal" "$BANK_SESSION/pal"
    cp -r "$SCRIPT_DIR/default_op.txt" "$BANK_SESSION/default_op.txt"
    cp -r "$SCRIPT_DIR/pieces/chtpm" "$BANK_SESSION/pieces/chtpm"
    cp -r "$SCRIPT_DIR/pieces/registry" "$BANK_SESSION/pieces/registry"
    cp -r "$SCRIPT_DIR/projects/yahoo-app/pieces" "$BANK_SESSION/projects/yahoo-app/pieces"
    cp -r "$SCRIPT_DIR/projects/yahoo-app/data" "$BANK_SESSION/data"

    cd "$BANK_SESSION"

    : > pieces/apps/player_app/interact_relay.txt
    : > pieces/keyboard/history.txt
    : > pieces/display/frame_changed.txt
    : > pieces/display/renderer_pulse.txt
    : > pieces/system/widget_cmds/inbox.txt
    : > pieces/system/widget_cmds/status.txt
    : > projects/yahoo-app/manager/gui_state.txt

    if [ ! -f pieces/system/config.txt ]; then
        cat > pieces/system/config.txt << 'EOCONFIG'
user_hash=
bank_balance=5000.00
current_broker=
last_lookup_symbol=
last_lookup_price=0.00
last_lookup_time=
EOCONFIG
    fi

    if [ ! -f pieces/system/brokers.txt ]; then
        cat > pieces/system/brokers.txt << 'EOBROKERS'
yahoo_finance|Yahoo Finance|full
EOBROKERS
    fi

    mkdir -p "projects/yahoo-app/pieces/broker_select"
    if [ -f pieces/system/brokers.txt ]; then
        {
            printf 'SECTION      | KEY                | VALUE\n'
            printf '%s\n' '----------------------------------------'
            printf 'META         | piece_id           | broker_select\n'
            while IFS='|' read -r id name type rest; do
                id=$(echo "$id" | xargs)
                name=$(echo "$name" | xargs)
                type=$(echo "$type" | xargs)
                if [ -n "$id" ] && [ -n "$name" ]; then
                    printf 'METHOD       | %s (%s)                | SELECT_BROKER:%s\n' "$name" "$type" "$id"
                fi
            done < pieces/system/brokers.txt
        } > "projects/yahoo-app/pieces/broker_select/piece.pdl"
    fi

    echo "$HOUSE_DIR" > pieces/system/house_root.txt

    export PRISC_PROJECT_ROOT="$BANK_SESSION"
    export PRISC_PROJECT_ID="yahoo-app"
    export PAL_LAYOUT="pieces/chtpm/layouts/bank.chtpm"
    "$SCRIPT_DIR/system/orchestrator" 2>>pieces/system/orchestrator.log &
    ORCH_PID=$!

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
            if [ "$cwd" = "$BANK_SESSION" ]; then
                kill -9 "$pid" 2>/dev/null
            fi
        done
    }

    cleanup() {
        kill "$ORCH_PID" "$GL_PID" "$RGB_PID" 2>/dev/null || true
        kill_own_module
        pkill -f "manager/\+x/agy_browser_manager\.\+x" 2>/dev/null || true
        persist_session_state; rm -rf "$BANK_SESSION" 2>/dev/null || true
    }
    # Step 2 symlink-migration fix: copy mutable session state back
    # to the real project root before the session dir is deleted
    # (the old symlinks made these writes land at the real root for
    # free; cp -r sessions need this explicit copy-back). Merge
    # semantics - adds/overwrites, never deletes. Volatile files
    # (quit_flag, pids, history, relays, gui_state) are NOT copied.
    persist_session_state() {
        mkdir -p "$SCRIPT_DIR/data/" 2>/dev/null || true
        cp -r "$BANK_SESSION/data/." "$SCRIPT_DIR/data/" 2>/dev/null || true
    }
    trap cleanup EXIT INT TERM

    ./system/keyboard_input

    cleanup
}

case "$ACTION" in
    compile|c|build)
        bash "$SCRIPT_DIR/scripts/build.sh"
        ;;
    run|r|start)
        run_app
        ;;
    kill|k|stop)
        for pat in $PKILL_PATTERNS; do
            pkill -f "$pat" 2>/dev/null
        done
        pkill -f "yahoo-broker/button.sh" 2>/dev/null || true
        rm -rf /tmp/.yahoo-app-* 2>/dev/null || true
        echo "done"
        ;;
    help|h|-h|--help)
        echo "yahoo-app — Bank GUI + broker widget"
        echo ""
        echo "Usage: ./button.sh <action>"
        echo "  compile      - Build all ops + system"
        echo "  run          - Launch bank GUI (broker widget spawns on demand)"
        echo "  kill         - Kill lingering processes"
        echo "  help         - This help"
        ;;
    *)
        echo "Unknown: $ACTION (try ./button.sh help)"
        exit 1
        ;;
esac
