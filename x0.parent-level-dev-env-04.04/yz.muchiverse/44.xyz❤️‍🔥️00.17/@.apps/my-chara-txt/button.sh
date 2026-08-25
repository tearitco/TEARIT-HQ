#!/bin/bash
# button.sh - launcher for my-chara-txt, modeled directly on
# 041.pal-chain's own button.sh (real interact+module chtpm pattern,
# session-isolation per xyzos-standards §23 - see that file's own
# header comment for the full rationale this reuses). No palnet_peer/
# P2P here (single-player, NO_NET=1 exported below) - orchestrator.c's
# own launch is already gated on both NO_NET and the binary existing,
# so this is a clean no-op skip, not a special case.
ACTION="${1:-help}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

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
        # private, throwaway directory for its own ephemeral UI state,
        # deleted on exit. SHARED (symlinked, never copied): system/
        # ops/pal/pieces-chtpm/default_op.txt/projects pieces (static,
        # read-only) and data/ (the REAL persistent ledger every
        # session of this project must see and mutate together).
        SESSION_ID="$(date +%s)-$$"
        SESSION_DIR="$SCRIPT_DIR/pieces/sessions/$SESSION_ID"
        mkdir -p "$SESSION_DIR/pieces/system" "$SESSION_DIR/pieces/display" \
                 "$SESSION_DIR/pieces/apps/player_app" "$SESSION_DIR/pieces/keyboard" \
                 "$SESSION_DIR/pieces/os" "$SESSION_DIR/projects/my-chara-txt/manager"
        mkdir -p "$SCRIPT_DIR/data"

        # SIMLINK ELIMINATION 2026-08-20 (see SIMLINK_PITFALL.md /
        # sim-smell-fix.md), DIFFERENT STRATEGY than mutaclysm/board-
        # viewer/piececraft-xyz: this project's own shared engine
        # (chtpm_parser_pal.c) crashes its per-screen module relaunch
        # when PRISC_PROJECT_ROOT genuinely diverges from the session
        # dir (real, reproduced bug - my-chara-txt is the only project
        # of the four with a SEPARATE PAL module per screen, switched
        # via <button href>, so it's the only one that exercises this
        # crash path). Rather than risk destabilizing the shared engine
        # for every other project that uses it, PRISC_PROJECT_ROOT stays
        # "$SESSION_DIR" (unchanged, below) - real, static, read-only
        # content is COPIED into the session dir instead of symlinked,
        # eliminating every symlink without touching any downstream
        # project_root/session_root assumption at all.
        cp -p "$SCRIPT_DIR/pieces/os/kill_all.sh" "$SESSION_DIR/pieces/os/kill_all.sh" 2>/dev/null
        cp -r "$SCRIPT_DIR/system" "$SESSION_DIR/system"
        cp -r "$SCRIPT_DIR/ops" "$SESSION_DIR/ops"
        cp -r "$SCRIPT_DIR/pal" "$SESSION_DIR/pal"
        cp -p "$SCRIPT_DIR/default_op.txt" "$SESSION_DIR/default_op.txt"
        cp -r "$SCRIPT_DIR/pieces/chtpm" "$SESSION_DIR/pieces/chtpm"
        # Required for chtpm_rgb_render/gl_mirror to find glyph bitmap
        # data (PITFALL 52, see text-editor-xyz's own button.sh) -
        # without this every character renders invisible in the GL
        # window (checksummed but visually blank).
        cp -r "$SCRIPT_DIR/pieces/registry" "$SESSION_DIR/pieces/registry" 2>/dev/null
        mkdir -p "$SESSION_DIR/projects/my-chara-txt"
        cp -r "$SCRIPT_DIR/projects/my-chara-txt/pieces" "$SESSION_DIR/projects/my-chara-txt/pieces"
        # data/ is the REAL persistent ledger (mutated by real gameplay,
        # meant to survive across relaunches) - copying it in is only
        # half the story. Copied back out to the real location in the
        # EXIT trap below, before the session dir gets deleted, so
        # writes made this session aren't silently lost. Real,
        # acknowledged trade-off vs. the old symlink's live write-
        # through: two truly CONCURRENT sessions each mutating their own
        # copy would clobber each other on exit instead of merging -
        # acceptable here since this is a single-player game
        # (NO_NET=1, per this file's own header comment), not a real
        # concern for the actual, intended usage pattern.
        cp -r "$SCRIPT_DIR/data" "$SESSION_DIR/data"

        # REAL, SAME CONVENTION as piececraft-xyz's own button.sh (2026-08-17,
        # khtpm-merge-how2.md §5c.6) - the real, non-session project dir,
        # so the shared x11_mirror binary's own derive_title() can find
        # the REAL project name instead of this session dir's own
        # timestamp-based one.
        mkdir -p "$SESSION_DIR/pieces/system"
        echo "$SCRIPT_DIR" > "$SESSION_DIR/pieces/system/real_project_root.txt"

        cd "$SESSION_DIR"
        : > pieces/apps/player_app/interact_relay.txt
        : > pieces/keyboard/history.txt
        : > pieces/system/quit_flag.txt
        : > pieces/display/mychara_screen_changed.txt
        : > pieces/display/frame_changed.txt
        : > projects/my-chara-txt/manager/gui_state.txt

        # Fresh per-session config.txt seeded with starting state if the
        # persistent one under the REAL project dir doesn't exist yet.
        if [ ! -f "$SCRIPT_DIR/pieces/system/config.txt" ]; then
            mkdir -p "$SCRIPT_DIR/pieces/system"
            cat > "$SCRIPT_DIR/pieces/system/config.txt" << 'EOCONFIG'
game_id=my-chara-001
player_name=Adam
day=1
max_days=10
health=100
money=500
grain_in_inventory=10
silver_in_inventory=0
gold_in_inventory=0
game_state=playing
supervision_mode=manual
decision_mode=0
risk_level=5
compute_tier=0
paused_for_confirmation=0
last_auto_tick=0
EOCONFIG
        fi
        # Also ensure plots.txt exists as a persistent state file
        if [ ! -f "$SCRIPT_DIR/pieces/system/plots.txt" ]; then
            cat > "$SCRIPT_DIR/pieces/system/plots.txt" << 'EOPLOTS'
plot_0_state=empty
plot_0_crop=
plot_0_harvest_day=0
plot_1_state=empty
plot_1_crop=
plot_1_harvest_day=0
plot_2_state=empty
plot_2_crop=
plot_2_harvest_day=0
EOPLOTS
        fi
        # Same copy-in/copy-out-on-exit strategy as data/ above - real
        # persistent state, write-through emulated via the EXIT trap.
        cp -p "$SCRIPT_DIR/pieces/system/config.txt" "$SESSION_DIR/pieces/system/config.txt"
        cp -p "$SCRIPT_DIR/pieces/system/plots.txt" "$SESSION_DIR/pieces/system/plots.txt"

        cat > pieces/apps/player_app/state.txt << 'EOSTATE'
module_path=system/prisc+x pal/main_loop_chtpm.pal
project_id=my-chara-txt
active_target_id=main
EOSTATE

        export PRISC_PROJECT_ROOT="$SESSION_DIR"
        export PRISC_PROJECT_ID="my-chara-txt"
        export NO_NET=1
        export PAL_LAYOUT="pieces/chtpm/layouts/main.chtpm"
        "$SCRIPT_DIR/system/orchestrator" 2>>pieces/system/orchestrator.log &
        ORCH_PID=$!

        # OPTIONAL GL/RGB MIRROR (§35 GL-primary, ported from
        # text-editor-xyz/101.mutaclsym's own button.sh pattern):
        # gated on NO_GL and a real DISPLAY, skips gracefully otherwise.
        # MUST wait for chtpm_parser_pal's own first real compose before
        # launching chtpm_rgb_render, or rgb_frame.raw gets stuck
        # permanently all-black/stale (PITFALL 54 - the exact race this
        # wait-loop exists to avoid).
        GL_PID=""
        RGB_PID=""
        if [ -z "$NO_GL" ] && [ -n "$DISPLAY" ]; then
            waited=0
            while [ ! -s pieces/display/current_frame.txt ] && [ "$waited" -lt 20 ]; do
                sleep 0.1
                waited=$((waited + 1))
            done
            # REAL SHARED BINARY (2026-08-17, khtpm-merge-how2.md §5c.6,
            # legacy-shared-fix.md §3) - prefer the shared x11_mirror
            # binary over this project's own local gl_mirror.
            # FORCE_GL_MIRROR=1 to force the legacy GL path.
            SHARED_MIRROR="$SCRIPT_DIR/../../&.widgits/_shared-lib/ops/+x/x11_mirror.+x"
            if [ -z "$FORCE_GL_MIRROR" ] && [ -x "$SHARED_MIRROR" ]; then
                "$SHARED_MIRROR" "$SESSION_DIR" >/dev/null 2>&1 &
                GL_PID=$!
            elif [ -x ./system/gl_mirror ]; then
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

        # Copy-back half of the copy-in/copy-out write-through emulation
        # for data/config.txt/plots.txt (see their own copy-in comments
        # above) - must run BEFORE rm -rf deletes the session dir, so any
        # real gameplay writes this session made aren't silently lost.
        persist_session_state() {
            cp -r "$SESSION_DIR/data/." "$SCRIPT_DIR/data/" 2>/dev/null
            cp -p "$SESSION_DIR/pieces/system/config.txt" "$SCRIPT_DIR/pieces/system/config.txt" 2>/dev/null
            cp -p "$SESSION_DIR/pieces/system/plots.txt" "$SCRIPT_DIR/pieces/system/plots.txt" 2>/dev/null
        }

        trap 'kill "$ORCH_PID" "$GL_PID" "$RGB_PID" 2>/dev/null; wait "$ORCH_PID" 2>/dev/null; kill_own_module; persist_session_state; rm -rf "$SESSION_DIR"' EXIT INT TERM

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
        # REAL SHARED BINARY (2026-08-17) - match on THIS project's own
        # session dir, not the bare binary name (shared across projects).
        pkill -f "x11_mirror.+x.*pieces/sessions" 2>/dev/null
        # REAL FIX (2026-08-17, legacy-shared-fix.md §2.6) - a bare
        # "system/orchestrator" match kills EVERY project's orchestrator
        # process (confirmed live collateral kill during this session's
        # own testing, killed mutaclysm's session). Every one of the 16
        # legacy projects launches an absolute "$SCRIPT_DIR/system/
        # orchestrator" path, indistinguishable in `ps` without it.
        SCRIPT_DIR_RE=$(printf '%s' "$SCRIPT_DIR" | sed 's/[.[\*^$()+?{|]/\\&/g')
        pkill -f "$SCRIPT_DIR_RE/system/orchestrator" 2>/dev/null
        echo "done"
        ;;
    check|verify)
        for b in system/prisc+x system/keyboard_input system/renderer \
                 system/chtpm_parser_pal system/orchestrator \
                 ops/+x/mychara_menu_input.+x ops/+x/mychara_compose_frame.+x \
                 ops/+x/mychara_ai_decide.+x; do
            if [ -x "$SCRIPT_DIR/$b" ]; then echo "OK   $b"; else echo "MISSING $b"; fi
        done
        if [ -x "$SCRIPT_DIR/../../&.widgits/_shared-lib/ops/+x/x11_mirror.+x" ]; then
            echo "OK   shared x11_mirror (preferred display mirror)"
        else
            echo "SKIP shared x11_mirror (see &.widgits/_shared-lib/ops/build_x11_mirror.sh)"
        fi
        for b in system/chtpm_rgb_render system/gl_mirror; do
            if [ -x "$SCRIPT_DIR/$b" ]; then echo "OK   $b (optional GL mirror)"; else echo "OPTIONAL-MISS $b"; fi
        done
        ;;
    help|h|-h|--help)
        echo "my-chara-txt button.sh"
        echo ""
        echo "Usage: ./button.sh <action>"
        echo "  compile, c, build   - Build prisc+x + ops"
        echo "  run, r              - THE REAL PLAYABLE UI (interactive, needs a real terminal)"
        echo "  kill, k, stop       - Kill any lingering my-chara-txt processes"
        echo "  check, verify       - Verify all binaries exist"
        echo "  help, h             - Show this help"
        ;;
    *)
        echo "Unknown action: $ACTION"
        exit 1
        ;;
esac
