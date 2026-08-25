#!/bin/bash
# button.sh - launcher for civ-txt, modeled directly on
# @.apps/my-chara-txt's own button.sh (real interact+module chtpm
# pattern, session-isolation per xyzos-standards §23). Single-player
# P1 skeleton for now (NO_NET=1) - AI civs will come in a later phase.
ACTION="${1:-help}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# House root - two levels up from this project (civ-txt is directly
# under @.apps/), same computation text-editor-xyz's own button.sh
# uses. Written into each session as house_root.txt so ops (like
# civ_menu_input.c's OPEN_BOARD_WIDGET handler) can find sibling
# projects/widgets without hardcoding a path - see
# &.widgits/WIDGIT_BIBLE.md's own "house_root.txt - where is the
# house?" resolution-chain convention.
HOUSE_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"

# Own-scoped board-viewer widget cleanup (2026-08-03, direct user
# correction caught while building piececraft-xyz - see that project's
# own button.sh header comment on this same change for the full
# writeup: after board-viewer's own ledger registration was scoped
# per-host ("board-viewer:civ-txt"), a real gap remained - if this
# project's own game session dies without its board-viewer widget also
# dying, the orphaned widget stays alive and gets silently REFOCUSED
# (not respawned) by the next OPEN_BOARD_WIDGET press, invisible if
# that window isn't already on top. Kill any surviving same-scope
# widget on START (safety net) AND on QUIT (so a clean Ctrl+C never
# leaves one behind).
kill_own_board_widget() {
    pkill -f "board-viewer/button.sh run-widget $SCRIPT_DIR" 2>/dev/null || true
}

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
        # Safety-net cleanup - see kill_own_board_widget()'s own header
        # comment above for why this must run on every fresh start.
        kill_own_board_widget
        SESSION_ID="$(date +%s)-$$"
        SESSION_DIR="$SCRIPT_DIR/pieces/sessions/$SESSION_ID"
        mkdir -p "$SESSION_DIR/pieces/system" "$SESSION_DIR/pieces/display" \
                 "$SESSION_DIR/pieces/apps/player_app" "$SESSION_DIR/pieces/keyboard" \
                 "$SESSION_DIR/pieces/os" "$SESSION_DIR/projects/civ-txt/manager"
        mkdir -p "$SCRIPT_DIR/data"

        cp -r "$SCRIPT_DIR/pieces/os/kill_all.sh" "$SESSION_DIR/pieces/os/kill_all.sh" 2>/dev/null
        cp -r "$SCRIPT_DIR/system" "$SESSION_DIR/system"
        cp -r "$SCRIPT_DIR/ops" "$SESSION_DIR/ops"
        cp -r "$SCRIPT_DIR/pal" "$SESSION_DIR/pal"
        cp -r "$SCRIPT_DIR/default_op.txt" "$SESSION_DIR/default_op.txt"
        cp -r "$SCRIPT_DIR/pieces/chtpm" "$SESSION_DIR/pieces/chtpm"
        cp -r "$SCRIPT_DIR/pieces/registry" "$SESSION_DIR/pieces/registry"
        cp -r "$SCRIPT_DIR/projects/civ-txt/pieces" "$SESSION_DIR/projects/civ-txt/pieces"
        cp -r "$SCRIPT_DIR/data" "$SESSION_DIR/data"

        cd "$SESSION_DIR"
        : > pieces/apps/player_app/interact_relay.txt
        : > pieces/keyboard/history.txt
        : > pieces/system/quit_flag.txt
        : > pieces/display/civ_screen_changed.txt
        : > pieces/display/frame_changed.txt
        : > projects/civ-txt/manager/gui_state.txt
        # For OPEN_BOARD_WIDGET (and any other future widget-spawn
        # logic) to find &.widgits/ and civ-txt's own REAL (non-
        # session) project root - see BOARD_WIDGET_ARCHITECTURE.md §4.
        echo "$HOUSE_DIR" > pieces/system/house_root.txt
        echo "$SCRIPT_DIR" > pieces/system/real_project_root.txt

        # Fresh per-session config.txt seeded with starting state if the
        # persistent one under the REAL project dir doesn't exist yet.
        if [ ! -f "$SCRIPT_DIR/pieces/system/config.txt" ]; then
            mkdir -p "$SCRIPT_DIR/pieces/system"
            cat > "$SCRIPT_DIR/pieces/system/config.txt" << 'EOCONFIG'
game_id=civ-txt-001
turn=1
turn_order_index=0
victory_condition=
map_scale=
combat_resolution=
treasury=50
city_count=1
game_state=setup
EOCONFIG
        fi
        cp -r "$SCRIPT_DIR/pieces/system/config.txt" "$SESSION_DIR/pieces/system/config.txt"
        # board.txt is REAL PERSISTENT DATA (like config.txt), generated
        # by CONFIRM_START and read by the board-viewer widget from
        # civ-txt's own REAL (non-session) root - must be symlinked into
        # every session the same way config.txt is, or it silently
        # writes into the ephemeral session dir instead and vanishes on
        # exit (exact same bug class as my-chara-txt's own plots.txt
        # symlink omission, found and fixed earlier this session).
        touch "$SCRIPT_DIR/pieces/system/board.txt"
        cp -r "$SCRIPT_DIR/pieces/system/board.txt" "$SESSION_DIR/pieces/system/board.txt"
        # terrain_legend.txt - real per-host data-bank board-viewer reads
        # instead of hardcoding civ-txt's own terrain in its C source
        # (Phase 0 of @.apps/piececraft-xyz/PIECECRAFT_XYZ_DESIGN.md §0a -
        # see &.widgits/board-viewer/ops/bv_render_3d.c's own
        # load_terrain_legend() for the full writeup). Seeded once, same
        # if-missing pattern config.txt already uses, so a fresh checkout
        # still works without this file having been hand-created.
        if [ ! -f "$SCRIPT_DIR/pieces/system/terrain_legend.txt" ]; then
            mkdir -p "$SCRIPT_DIR/pieces/system"
            cat > "$SCRIPT_DIR/pieces/system/terrain_legend.txt" << 'EOLEGEND'
# glyph|height|r|g|b|asset_hex|name  ("-" asset_hex = no texture)
.|0.0|120|170|80|1F33E|plains
f|0.4|30|110|40|1F332|forest
^|0.8|140|120|100|26F0|hills
~|-0.3|40|90|180|1F30A|water
C|1.2|200|180|60|1F3F0|CAPITAL
EOLEGEND
        fi
        cp -r "$SCRIPT_DIR/pieces/system/terrain_legend.txt" "$SESSION_DIR/pieces/system/terrain_legend.txt"
        # widget_cmds/inbox.txt + board_widget_bridge.txt - real widget->
        # host command delivery (2026-08-03, Phase 0 follow-up per
        # @.apps/piececraft-xyz/PIECECRAFT_XYZ_DESIGN.md §4a). Bridge
        # file matches file-menu's own real widget_bridge.txt shape (a
        # host-published connection descriptor any widget can read) -
        # board-viewer isn't reading this yet (no real verbs to send
        # from inside the widget until piececraft-xyz's own keys are
        # decided), but civ_menu_input.c's own inbox-draining is real
        # and live now, so this is here ready for it, not speculative.
        mkdir -p "$SCRIPT_DIR/pieces/system/widget_cmds"
        touch "$SCRIPT_DIR/pieces/system/widget_cmds/inbox.txt"
        cp -r "$SCRIPT_DIR/pieces/system/widget_cmds" "$SESSION_DIR/pieces/system/widget_cmds"
        cat > "$SCRIPT_DIR/pieces/system/board_widget_bridge.txt" << EOF
inbox_path=pieces/system/widget_cmds/inbox.txt
kind=board_game
project_id=civ-txt
display_name=Civ-txt
EOF
        cp -r "$SCRIPT_DIR/pieces/system/board_widget_bridge.txt" "$SESSION_DIR/pieces/system/board_widget_bridge.txt"
        # entities.txt - same generic entity-render manifest tactics-txt's
        # own button.sh now writes (see that project's own ops/tactics_
        # menu_input.c CONFIRM_START comment for the format). Not yet
        # populated here - civ-txt has no real cities/units data yet
        # (see @.apps/PORTABLE_ENTITY_ARCHITECTURE.md) - wired up now so
        # the plumbing is ready once P2 gives it real content.
        touch "$SCRIPT_DIR/pieces/system/entities.txt"
        cp -r "$SCRIPT_DIR/pieces/system/entities.txt" "$SESSION_DIR/pieces/system/entities.txt"

        cat > pieces/apps/player_app/state.txt << 'EOSTATE'
module_path=system/prisc+x pal/new_game_module.pal
project_id=civ-txt
active_target_id=new_game
EOSTATE

        export PRISC_PROJECT_ROOT="$SESSION_DIR"
        export PRISC_PROJECT_ID="civ-txt"
        export NO_NET=1
        export PAL_LAYOUT="pieces/chtpm/layouts/new_game.chtpm"
        "$SCRIPT_DIR/system/orchestrator" 2>>pieces/system/orchestrator.log &
        ORCH_PID=$!

        # OPTIONAL GL/RGB MIRROR - gated on NO_GL and a real DISPLAY,
        # skips gracefully otherwise. MUST wait for chtpm_parser_pal's
        # own first real compose before launching chtpm_rgb_render, or
        # rgb_frame.raw gets stuck permanently all-black/stale.
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

        # Step 2 symlink-migration fix: copy mutable session state back
        # to the real project root before the session dir is deleted
        # (the old symlinks made these writes land at the real root for
        # free; cp -r sessions need this explicit copy-back). Merge
        # semantics - adds/overwrites, never deletes. Volatile files
        # (quit_flag, pids, history, relays, gui_state) are NOT copied.
        persist_session_state() {
            mkdir -p "$(dirname "$SCRIPT_DIR/pieces/system/config.txt")" 2>/dev/null || true
            cp -r "$SESSION_DIR/pieces/system/config.txt" "$SCRIPT_DIR/pieces/system/config.txt" 2>/dev/null || true
            mkdir -p "$(dirname "$SCRIPT_DIR/pieces/system/board.txt")" 2>/dev/null || true
            cp -r "$SESSION_DIR/pieces/system/board.txt" "$SCRIPT_DIR/pieces/system/board.txt" 2>/dev/null || true
            mkdir -p "$(dirname "$SCRIPT_DIR/pieces/system/terrain_legend.txt")" 2>/dev/null || true
            cp -r "$SESSION_DIR/pieces/system/terrain_legend.txt" "$SCRIPT_DIR/pieces/system/terrain_legend.txt" 2>/dev/null || true
            mkdir -p "$(dirname "$SCRIPT_DIR/pieces/system/entities.txt")" 2>/dev/null || true
            cp -r "$SESSION_DIR/pieces/system/entities.txt" "$SCRIPT_DIR/pieces/system/entities.txt" 2>/dev/null || true
            mkdir -p "$SCRIPT_DIR/pieces/system/widget_cmds/" 2>/dev/null || true
            cp -r "$SESSION_DIR/pieces/system/widget_cmds/." "$SCRIPT_DIR/pieces/system/widget_cmds/" 2>/dev/null || true
            mkdir -p "$(dirname "$SCRIPT_DIR/pieces/system/board_widget_bridge.txt")" 2>/dev/null || true
            cp -r "$SESSION_DIR/pieces/system/board_widget_bridge.txt" "$SCRIPT_DIR/pieces/system/board_widget_bridge.txt" 2>/dev/null || true
        }
        trap 'kill "$ORCH_PID" "$GL_PID" "$RGB_PID" 2>/dev/null; wait "$ORCH_PID" 2>/dev/null; kill_own_module; kill_own_board_widget; persist_session_state; rm -rf "$SESSION_DIR"' EXIT INT TERM

        ./system/keyboard_input

        kill "$ORCH_PID" "$GL_PID" "$RGB_PID" 2>/dev/null
        kill_own_module
        ;;
    kill|k|stop)
        pkill -f "system/keyboard_input" 2>/dev/null
        pkill -f "system/renderer" 2>/dev/null
        pkill -f "system/prisc\+x" 2>/dev/null
        pkill -f "system/chtpm_parser_pal" 2>/dev/null
        pkill -f "system/chtpm_rgb_render" 2>/dev/null
        pkill -f "system/gl_mirror" 2>/dev/null
        pkill -f "system/orchestrator" 2>/dev/null
        kill_own_board_widget
        echo "done"
        ;;
    check|verify)
        for b in system/prisc+x system/keyboard_input system/renderer \
                 system/chtpm_parser_pal system/orchestrator \
                 ops/+x/civ_menu_input.+x ops/+x/civ_compose_frame.+x; do
            if [ -x "$SCRIPT_DIR/$b" ]; then echo "OK   $b"; else echo "MISSING $b"; fi
        done
        for b in system/chtpm_rgb_render system/gl_mirror; do
            if [ -x "$SCRIPT_DIR/$b" ]; then echo "OK   $b (optional GL mirror)"; else echo "OPTIONAL-MISS $b"; fi
        done
        ;;
    help|h|-h|--help)
        echo "civ-txt button.sh"
        echo ""
        echo "Usage: ./button.sh <action>"
        echo "  compile, c, build   - Build prisc+x + ops"
        echo "  run, r              - THE REAL PLAYABLE UI (interactive, needs a real terminal)"
        echo "  kill, k, stop       - Kill any lingering civ-txt processes"
        echo "  check, verify       - Verify all binaries exist"
        echo "  help, h             - Show this help"
        ;;
    *)
        echo "Unknown action: $ACTION"
        exit 1
        ;;
esac
