#!/bin/bash
# button.sh - launcher for piececraft-hq, modeled directly on
# @.apps/civ-txt's own button.sh (real interact+module chtpm
# pattern, session-isolation per xyzos-standards §23). P1 clone phase
# is verified and done (mutant-clone.txt, HANDOFF_NEXT_SESSION.md) -
# real Phase 2 divergence now in progress per civ-vs-piece.md and
# phase2-plan.md. keybinds.txt (real, space=jump/g=mine/h=build) and
# board_widget_bridge.txt/widget_cmds/ (real widget->host command
# delivery, JUMP/MINE/BUILD/MOVE all have real handlers in pc_menu_
# input.c) are seeded below, same real shape civ-txt's own button.sh
# uses.
ACTION="${1:-help}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# House root - two levels up from this project (piececraft-hq is
# directly under @.apps/), same computation civ-txt's/tactics-txt's own
# button.sh use. Written into each session as house_root.txt so ops
# (like pc_menu_input.c's OPEN_BOARD_WIDGET handler) can find sibling
# projects/widgets without hardcoding a path.
HOUSE_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"

# Own-scoped board-viewer widget cleanup (2026-08-03, direct user
# correction: "kill orphan on quit(ctrl+c) and re kill on start just in
# case" - after board-viewer's own ledger registration was scoped
# per-host ("board-viewer:piececraft-hq", see &.widgits/board-viewer/
# button.sh's own header comment), a real gap remained: if piececraft-
# xyz's own game session dies WITHOUT its board-viewer widget also
# dying (crash, force-kill, or simply Ctrl+C not reaching the widget's
# own separate process group), the orphaned widget stays alive and gets
# silently REFOCUSED (not respawned) by the next OPEN_BOARD_WIDGET press
# - correct per the per-project scoping fix, but invisible to the user
# if that orphaned window isn't already on top, which looks exactly
# like "the board didn't open." Two-layer real fix: kill any surviving
# same-scope widget on START (safety net for a prior crash) AND on
# QUIT (so a clean Ctrl+C never leaves one behind in the first place).
# pkill -f matches the widget's own real invocation command line
# directly (board-viewer/button.sh run-widget <this project's own real
# path>) - simpler and more direct than parsing the ledger from bash.
kill_own_board_widget() {
    pkill -f "board-viewer/button.sh run-widget $SCRIPT_DIR" 2>/dev/null || true
}

# REAL, NEW 2026-08-04, direct instruction ("make sure we add demons to
# quit kill... im not running a session" - a REAL orphaned pc_clock_
# daemon.+x was found live, from a session that had already ended, still
# writing to world_01/state.txt and racing with other real writers,
# causing real file corruption). Same exact real two-layer pattern
# kill_own_board_widget() already established above (kill on START as a
# safety net for a prior crash, AND on QUIT so a clean exit never leaves
# one behind) - pc_clock_daemon.+x is a real persistent background
# process (ops/pc_clock_daemon.c's own header comment), same real class
# of "must not outlive its session" risk the board-viewer widget already
# had.
kill_own_clock_daemon() {
    # REAL FIX 2026-08-04, direct user report ("random" ticker behavior,
    # traced to FOUR real daemons from four past sessions all running
    # concurrently at once): this pattern never actually matched
    # anything. The daemon is launched via pc_menu_input.c's own
    # launch_clock_daemon_if_needed() using the EPHEMERAL SESSION root
    # (ops/ is a real symlink INTO that session), so its own real
    # command line is ".../pieces/sessions/<id>/ops/+x/pc_clock_daemon.
    # +x" - never literally "$SCRIPT_DIR/ops/...". Real fix: match the
    # real binary's own suffix only (no session-specific prefix), which
    # catches every past session's own real orphan, not just this one.
    pkill -f "ops/\+x/pc_clock_daemon\.\+x" 2>/dev/null || true
    rm -f "$SCRIPT_DIR/pieces/system/pc_clock_daemon.pid" 2>/dev/null || true
}

kill_own_hq_status_manager() {
    # Real, NEW 2026-08-30 - clean up the piececraft-hq status manager
    # daemon (same two-layer pattern as clock_daemon above).
    pkill -f "ops/\+x/pc_hq_status_manager\.\+x" 2>/dev/null || true
}

# REAL, NEW 2026-08-30, direct live report ("they should quit when
# program is quit programmatically. make sure that is fixed") - found
# live during this same session's own repeated test launches: the EXIT
# trap below (kill "$ORCH_PID" ...) only ever fires for a clean exit of
# THIS SAME button.sh process. Relaunching via a brand-new `setsid bash
# button.sh run &` (or any crash/force-kill that never lets the old
# script's own trap run at all) leaves its orchestrator orphaned - same
# real "must not outlive its session" class kill_own_board_widget()/
# _clock_daemon()/_hq_status_manager() already exist for, just missing
# an equivalent for the orchestrator itself. Uses the SAME real,
# regex-escaped, THIS-project-scoped pkill pattern the dedicated kill
# action already established (2026-08-17, legacy-shared-fix.md §2.6) -
# a bare "system/orchestrator" match would kill every OTHER project's
# orchestrator too (confirmed real collateral-kill incident that day).
kill_own_orchestrator() {
    local script_dir_re
    script_dir_re=$(printf '%s' "$SCRIPT_DIR" | sed 's/[.[\*^$()+?{|]/\\&/g')
    pkill -f "$script_dir_re/system/orchestrator" 2>/dev/null || true
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
        # comment above for why this must run on every fresh start, not
        # just on quit.
        #
        # REAL FIX 2026-08-30, direct live report ("sometimes it still
        # doesn't open... no other hq window has this problem") - found
        # by direct comparison against civ-txt/tactics-txt (both share
        # this exact same clone lineage, neither has ever had this
        # report): their own `run)` case calls NONE of these kills -
        # only their `kill|k|stop)` verb does. piececraft-hq is the
        # ONLY entity that runs a broad, PATH-scoped (not PID/session-
        # scoped) self-kill unconditionally on every single `run`.
        # kill_own_orchestrator() in particular matches ANY orchestrator
        # under this SCRIPT_DIR - including one a concurrent/just-
        # started sibling `run` invocation spawned moments earlier (a
        # double-fired taskbar click, or the user clicking again before
        # a window appears). That sibling's own orchestrator gets killed
        # out from under it before it can ever show a window - a real,
        # self-inflicted, intermittent race unique to this project's own
        # extra orphan-cleanup safety net. Real fix: skip this whole
        # safety net if a session dir was created in the last few
        # seconds - that's real, live evidence another `run` is already
        # in flight right now, not a stale orphan from a dead session.
        RECENT_SESSION=0
        if [ -d "$SCRIPT_DIR/pieces/sessions" ]; then
            NOW_TS="$(date +%s)"
            for d in "$SCRIPT_DIR"/pieces/sessions/*/; do
                [ -d "$d" ] || continue
                base="$(basename "$d")"
                sess_ts="${base%%-*}"
                case "$sess_ts" in ''|*[!0-9]*) continue ;; esac
                age=$((NOW_TS - sess_ts))
                if [ "$age" -ge 0 ] && [ "$age" -lt 5 ]; then RECENT_SESSION=1; break; fi
            done
        fi
        if [ "$RECENT_SESSION" -eq 0 ]; then
            kill_own_board_widget
            kill_own_clock_daemon
            kill_own_hq_status_manager
            kill_own_orchestrator
        fi
        SESSION_ID="$(date +%s)-$$"
        SESSION_DIR="$SCRIPT_DIR/pieces/sessions/$SESSION_ID"
        mkdir -p "$SESSION_DIR/pieces/system" "$SESSION_DIR/pieces/display" \
                 "$SESSION_DIR/pieces/apps/player_app" "$SESSION_DIR/pieces/keyboard" \
                 "$SESSION_DIR/pieces/os" "$SESSION_DIR/projects/piececraft-hq/manager"
        mkdir -p "$SCRIPT_DIR/data"

        # SIMLINK ELIMINATION 2026-08-20 (see SIMLINK_PITFALL.md /
        # sim-smell-fix.md's own "THE SOLUTION" section) - copy-based
        # strategy, same as my-chara-txt's own confirmed-working fix.
        # PRISC_PROJECT_ROOT stays "$SESSION_DIR" (unchanged, below) -
        # this project ALSO has two distinct <module> paths
        # (new_game_module.pal / main_module.pal), so it's equally at
        # risk of the shared engine's own module-relaunch crash if
        # PRISC_PROJECT_ROOT were switched to $SCRIPT_DIR instead - see
        # sim-smell-fix.md's own "REAL, UNRESOLVED shared-engine bug"
        # section, don't attempt that strategy here.
        cp -p "$SCRIPT_DIR/pieces/os/kill_all.sh" "$SESSION_DIR/pieces/os/kill_all.sh" 2>/dev/null
        cp -r "$SCRIPT_DIR/system" "$SESSION_DIR/system"
        cp -r "$SCRIPT_DIR/ops" "$SESSION_DIR/ops"
        cp -r "$SCRIPT_DIR/pal" "$SESSION_DIR/pal"
        cp -p "$SCRIPT_DIR/default_op.txt" "$SESSION_DIR/default_op.txt"
        cp -r "$SCRIPT_DIR/pieces/chtpm" "$SESSION_DIR/pieces/chtpm"
        cp -r "$SCRIPT_DIR/pieces/registry" "$SESSION_DIR/pieces/registry" 2>/dev/null
        mkdir -p "$SESSION_DIR/projects/piececraft-hq"
        cp -r "$SCRIPT_DIR/projects/piececraft-hq/pieces" "$SESSION_DIR/projects/piececraft-hq/pieces"
        # data/ is real persistent state - copied back out in the EXIT
        # trap below (persist_session_state()), same pattern as
        # my-chara-txt's own working fix.
        cp -r "$SCRIPT_DIR/data" "$SESSION_DIR/data"

        cd "$SESSION_DIR"
        : > pieces/apps/player_app/interact_relay.txt
        : > pieces/keyboard/history.txt
        : > pieces/system/quit_flag.txt
        : > pieces/display/pc_screen_changed.txt
        : > pieces/display/frame_changed.txt
        : > projects/piececraft-hq/manager/gui_state.txt
        # For OPEN_BOARD_WIDGET (and any other future widget-spawn
        # logic) to find &.widgits/ and piececraft-hq's own REAL
        # (non-session) project root.
        echo "$HOUSE_DIR" > pieces/system/house_root.txt
        echo "$SCRIPT_DIR" > pieces/system/real_project_root.txt

        # Fresh per-session config.txt seeded with starting state if the
        # persistent one under the REAL project dir doesn't exist yet.
        # Same shape civ-txt's own P1 config uses - generic game config,
        # NOT hero/player position (that's Phase 2+ entity state).
        if [ ! -f "$SCRIPT_DIR/pieces/system/config.txt" ]; then
            mkdir -p "$SCRIPT_DIR/pieces/system"
            cat > "$SCRIPT_DIR/pieces/system/config.txt" << 'EOCONFIG'
game_id=piececraft-hq-001
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
        # config.txt is real persistent state - copied in here, copied
        # back out by persist_session_state() in the EXIT trap below,
        # same pattern as data/ and my-chara-txt's own working fix.
        cp -p "$SCRIPT_DIR/pieces/system/config.txt" "$SESSION_DIR/pieces/system/config.txt"
        # board.txt is REAL PERSISTENT DATA (like config.txt), generated
        # by CONFIRM_START - same copy-in/copy-out treatment. NOTE
        # 2026-08-20: nothing in ops/*.c actually writes board.txt yet
        # (grepped clean - CONFIRM_START's board generation isn't wired
        # up), so this is a placeholder file for now, but the pattern is
        # future-proof for when it is.
        touch "$SCRIPT_DIR/pieces/system/board.txt"
        cp -p "$SCRIPT_DIR/pieces/system/board.txt" "$SESSION_DIR/pieces/system/board.txt"
        # entities.txt - real generic entity manifest board-viewer
        # reads. Not yet populated - piececraft-hq has no real
        # cities/units data yet in P1 clone phase.
        touch "$SCRIPT_DIR/pieces/system/entities.txt"
        cp -p "$SCRIPT_DIR/pieces/system/entities.txt" "$SESSION_DIR/pieces/system/entities.txt"

        # widget_cmds/inbox.txt + board_widget_bridge.txt - real Phase 2
        # widget->host command delivery. REAL BUG FIX 2026-08-20: under
        # the copy-based strategy these no longer need session-symlinking
        # at all - board-viewer already resolves both via real_root
        # (open_board_widget()/resolve_real_root() in pc_menu_input.c),
        # and pc_menu_input.c's own inbox drain was fixed today to read
        # via resolve_real_root() too (see sim-smell-fix.md). Both files
        # now live ONLY at the REAL project root, never copied into the
        # session dir.
        mkdir -p "$SCRIPT_DIR/pieces/system/widget_cmds"
        touch "$SCRIPT_DIR/pieces/system/widget_cmds/inbox.txt"
        cat > "$SCRIPT_DIR/pieces/system/board_widget_bridge.txt" << EOF
inbox_path=pieces/system/widget_cmds/inbox.txt
kind=board_game
project_id=piececraft-hq
display_name=Piececraft-HQ
EOF

        # REAL FIX 2026-08-30 (Piece 1 - completing the in-scene desks
        # screen, direct instruction: "it should just load the 3d
        # window. we shouldn't have to do setup and enter"). This used
        # to unconditionally force PAL_LAYOUT=new_game.chtpm/
        # active_target_id=new_game on EVERY launch, regardless of real
        # session state - the C-side get_current_piece_id() fallback
        # fix (pc_menu_input.c, same date) never mattered because this
        # env var/state.txt write happens first and wins. Real fix:
        # check config.txt's own real game_state (already the house's
        # real "has a world been created" signal - CONFIRM_START/
        # CREATE_WORLD_SEEDED/CREATE_WORLD_DEBUG all set it to
        # "playing") and skip straight to main.chtpm/module main_module
        # .pal when a world already exists. On a genuinely fresh
        # install (game_state still "setup", checked BEFORE the config
        # copy-in below since $SCRIPT_DIR's own real, persistent file is
        # what already reflects any prior real session), auto-generate
        # a default seeded world right here - same real
        # pc_generate_chunk.+x call CREATE_WORLD_SEEDED's own handler
        # makes - so first-ever launch also lands straight in the game,
        # never showing the old blocking ATLAS-EDITOR setup screen at
        # all.
        REAL_GAME_STATE="setup"
        if [ -f "$SCRIPT_DIR/pieces/system/config.txt" ]; then
            REAL_GAME_STATE="$(grep -m1 '^game_state=' "$SCRIPT_DIR/pieces/system/config.txt" | cut -d= -f2)"
        fi
        if [ "$REAL_GAME_STATE" != "playing" ] && [ -x "$SESSION_DIR/ops/+x/pc_generate_chunk.+x" ]; then
            AUTO_SEED="$(date +%s)"
            ( cd "$SESSION_DIR" && PRISC_PROJECT_ROOT="$SESSION_DIR" "$SESSION_DIR/ops/+x/pc_generate_chunk.+x" "$AUTO_SEED" 0 0 >/dev/null 2>&1 )
            sed -i 's/^game_state=.*/game_state=playing/' "$SCRIPT_DIR/pieces/system/config.txt" 2>/dev/null || true
            sed -i 's/^game_state=.*/game_state=playing/' "$SESSION_DIR/pieces/system/config.txt" 2>/dev/null || true
            REAL_GAME_STATE="playing"
        fi

        if [ "$REAL_GAME_STATE" = "playing" ]; then
            cat > pieces/apps/player_app/state.txt << 'EOSTATE'
module_path=system/prisc+x pal/main_module.pal
project_id=piececraft-hq
active_target_id=main
EOSTATE
            echo "pieces/chtpm/layouts/main.chtpm" > pieces/display/current_layout.txt
            PC_PAL_LAYOUT="pieces/chtpm/layouts/main.chtpm"
        else
            cat > pieces/apps/player_app/state.txt << 'EOSTATE'
module_path=system/prisc+x pal/new_game_module.pal
project_id=piececraft-hq
active_target_id=new_game
EOSTATE
            PC_PAL_LAYOUT="pieces/chtpm/layouts/new_game.chtpm"
        fi

        export PRISC_PROJECT_ROOT="$SESSION_DIR"
        export PRISC_PROJECT_ID="piececraft-hq"
        export NO_NET=1
        export PAL_LAYOUT="$PC_PAL_LAYOUT"
        "$SCRIPT_DIR/system/orchestrator" 2>>pieces/system/orchestrator.log &
        ORCH_PID=$!

        # OPTIONAL GL/RGB MIRROR - gated on NO_GL and a real DISPLAY,
        # skips gracefully otherwise. MUST wait for chtpm_parser_pal's
        # own first real compose before launching chtpm_rgb_render, or
        # rgb_frame.raw gets stuck permanently all-black/stale.
        #
        # REAL FIX 2026-08-30, direct live report ("dont u see it opens
        # 2 windows? the original window from -xyz is still opening" -
        # meaning THIS primary session's own real x11_mirror text-mode
        # UI, the same style piececraft-xyz has always shown; earlier
        # fixes this session only suppressed board-viewer's own legacy
        # display, not this one). piececraft-hq exists specifically to
        # be the real khtpm-only experience (see PIECECRAFT-HQ-CLONE-
        # NOTE.md) - its own primary text-mode mirror is hardcoded off
        # here, unconditionally, rather than left NO_GL-toggle-gated
        # for whoever launches it (the taskbar's real toy.pdl launcher
        # never sets NO_GL, so it always showed by default before this
        # fix). piececraft-xyz's own identical block is UNTOUCHED - this
        # is piececraft-hq-only behavior.
        GL_PID=""
        RGB_PID=""
        # REAL FIX 2026-08-30, found live testing the fix just above:
        # a plain `NO_GL=1` here leaked into the LATER, unrelated
        # board-widget-launch check further down this same script
        # (`[ -z "$NO_GL" ]`, same shell scope, same variable name) -
        # confirmed live: it silently disabled the real board-viewer
        # widget spawn entirely, so NEITHER window opened. Real fix:
        # a distinct local flag, scoped to only this one check.
        PCHQ_SKIP_PRIMARY_MIRROR=1
        if [ -z "$PCHQ_SKIP_PRIMARY_MIRROR" ] && [ -n "$DISPLAY" ]; then
            waited=0
            while [ ! -s pieces/display/current_frame.txt ] && [ "$waited" -lt 20 ]; do
                sleep 0.1
                waited=$((waited + 1))
            done
            # REAL SHARED BINARY (2026-08-17, khtpm-merge-how2.md §5c.6,
            # legacy-shared-fix.md §3) - prefer the shared x11_mirror
            # binary (plain Xlib, one copy shared by every legacy-GL
            # project) over this project's own local gl_mirror. cwd is
            # already SESSION_DIR here (see `cd "$SESSION_DIR"` above),
            # so passing "." as project_root is correct - the shared
            # binary's own derive_title() finds
            # pieces/system/real_project_root.txt from there and uses
            # the REAL project name, not the session dir's own
            # timestamp-based name. FORCE_GL_MIRROR=1 to force the
            # legacy GL path.
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

        # REAL FIX 2026-08-30, same direct instruction as the
        # game_state check above ("it should just load the 3d window.
        # we shouldn't have to do setup and enter") - auto-open the
        # real board-viewer widget the instant the game reaches "main"
        # (REAL_GAME_STATE=="playing"), instead of requiring the "2.
        # View Board" menu click every launch. Real, deliberate scope
        # choice: this duplicates the CORE spawn call open_board_widget()
        # (ops/pc_menu_input.c) makes rather than calling that function
        # directly (it's a static C function, not shell-callable) -
        # skips that function's own peer-refocus dedup check on
        # purpose, since this only ever runs once per fresh launch (no
        # earlier widget for THIS session could exist yet to dedup
        # against); the menu's own "View Board" row still goes through
        # the real, deduped path for any later manual re-open.
        if [ "$REAL_GAME_STATE" = "playing" ] && [ -z "$NO_GL" ] && [ -n "$DISPLAY" ]; then
            BOARD_BTN="$HOUSE_DIR/&.widgits/board-viewer/button.sh"
            if [ -x "$BOARD_BTN" ]; then
                # REAL FIX 2026-08-30, direct live report ("first it
                # opens legacy before switching 2 x11 khtpm version...
                # see it open?") - real, visible flash: the legacy
                # widget's own x11_mirror.+x display used to map for
                # real, then get killed ~1.5s later once the khtpm
                # window attached. NO_GL=1 (board-viewer's own real,
                # already-documented flag, "Skip gl_mirror/chtpm_rgb_
                # render entirely") stops that legacy display from ever
                # mapping at all - bv_render_3d.c (the real 3D data
                # generator this khtpm window actually reads) is
                # entirely independent of it (confirmed via default_op
                # .txt's own comment: "bypasses system/chtpm_rgb_
                # render"), so the real data pipeline is unaffected.
                NO_GL=1 setsid bash "$BOARD_BTN" run-widget "$SCRIPT_DIR" >/dev/null 2>&1 < /dev/null &
            fi
            # REAL, NEW 2026-08-30, direct live report ("the khtpm
            # piececraft is the one that is supposed to open when
            # piececraft-hq is clicked. why isn't it there yet?") - the
            # real khtpm-family board window (khtpm_entity_menu_
            # render.c's run_pchq_board_mode(), pchq-board.chtpm at this
            # project's own root) was wired into pc_menu_input.c's real
            # "View Board" menu handler, but THIS auto-boot launch is a
            # real, separate code path (see this block's own header
            # comment above) that never went through it. Same real
            # wiring, mirrored here: a short sleep so the widget's own
            # real ledger row exists first (run_pchq_board_mode()'s own
            # session-discovery needs it), then launch the khtpm window,
            # which kills the legacy display once it attaches.
            KHTPM_BIN="$HOUSE_DIR/*.monads/*.livedesk-taskbar/ops/+x/khtpm_entity_menu_render.+x"
            if [ -x "$KHTPM_BIN" ] && [ -f "$SCRIPT_DIR/pchq-board.chtpm" ]; then
                ( sleep 1.5; setsid "$KHTPM_BIN" "$HOUSE_DIR" "$SCRIPT_DIR/pchq-board.chtpm" "piececraft-hq" >/dev/null 2>&1 < /dev/null & ) &
            fi
        fi

        # REAL, NEW 2026-08-30 - launch piececraft-hq status manager to
        # poll game state and publish live info for the khtpm info window.
        # Real, single-instance daemon (no dedup like board-viewer, since
        # this lives only in the session and dies with it). Manager reads
        # SESSION_DIR's real state files and publishes to a simple state
        # file - no window positioning yet (v1 phase).
        HQ_MGR="$SESSION_DIR/ops/+x/pc_hq_status_manager.+x"
        if [ -x "$HQ_MGR" ] && [ "$REAL_GAME_STATE" = "playing" ]; then
            "$HQ_MGR" "$SESSION_DIR" "$SESSION_DIR" >/dev/null 2>&1 &
            HQ_MGR_PID=$!
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

        persist_session_state() {
            cp -r "$SESSION_DIR/data/." "$SCRIPT_DIR/data/" 2>/dev/null
            cp -p "$SESSION_DIR/pieces/system/config.txt" "$SCRIPT_DIR/pieces/system/config.txt" 2>/dev/null
            cp -p "$SESSION_DIR/pieces/system/board.txt" "$SCRIPT_DIR/pieces/system/board.txt" 2>/dev/null
            cp -p "$SESSION_DIR/pieces/system/entities.txt" "$SCRIPT_DIR/pieces/system/entities.txt" 2>/dev/null
        }

        trap 'kill "$ORCH_PID" "$GL_PID" "$RGB_PID" "$HQ_MGR_PID" 2>/dev/null; wait "$ORCH_PID" 2>/dev/null; kill_own_module; kill_own_board_widget; kill_own_clock_daemon; kill_own_hq_status_manager; kill_own_orchestrator; persist_session_state; rm -rf "$SESSION_DIR"' EXIT INT TERM

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
        # REAL SHARED BINARY (2026-08-17) - x11_mirror is shared across
        # projects, match on THIS project's own session dir (passed as
        # argv[1] via "." with cwd=SESSION_DIR, so the real cmdline is
        # the resolved SESSION_DIR path), not the bare binary name.
        pkill -f "x11_mirror.+x.*pieces/sessions" 2>/dev/null
        # REAL FIX (2026-08-17, legacy-shared-fix.md §2.6) - a bare
        # "system/orchestrator" match kills EVERY project's orchestrator
        # process (confirmed live collateral kill during this session's
        # own testing, killed mutaclysm's session). Every one of the 16
        # legacy projects launches an absolute "$SCRIPT_DIR/system/
        # orchestrator" path, indistinguishable in `ps` without it.
        SCRIPT_DIR_RE=$(printf '%s' "$SCRIPT_DIR" | sed 's/[.[\*^$()+?{|]/\\&/g')
        pkill -f "$SCRIPT_DIR_RE/system/orchestrator" 2>/dev/null
        kill_own_board_widget
        kill_own_clock_daemon
        kill_own_hq_status_manager
        echo "done"
        ;;
    check|verify)
        for b in system/prisc+x system/keyboard_input system/renderer \
                 system/chtpm_parser_pal system/orchestrator \
                 ops/+x/pc_menu_input.+x ops/+x/pc_compose_frame.+x; do
            if [ -x "$SCRIPT_DIR/$b" ]; then echo "OK   $b"; else echo "MISSING $b"; fi
        done
        for b in system/chtpm_rgb_render system/gl_mirror; do
            if [ -x "$SCRIPT_DIR/$b" ]; then echo "OK   $b (optional GL mirror)"; else echo "OPTIONAL-MISS $b"; fi
        done
        if [ -x "$SCRIPT_DIR/../../&.widgits/_shared-lib/ops/+x/x11_mirror.+x" ]; then
            echo "OK   shared x11_mirror (preferred display mirror)"
        else
            echo "SKIP shared x11_mirror (see &.widgits/_shared-lib/ops/build_x11_mirror.sh)"
        fi
        ;;
    help|h|-h|--help)
        echo "piececraft-hq button.sh"
        echo ""
        echo "Usage: ./button.sh <action>"
        echo "  compile, c, build   - Build prisc+x + ops"
        echo "  run, r              - THE REAL PLAYABLE UI (interactive, needs a real terminal)"
        echo "  kill, k, stop       - Kill any lingering piececraft-hq processes"
        echo "  check, verify       - Verify all binaries exist"
        echo "  help, h             - Show this help"
        ;;
    *)
        echo "Unknown action: $ACTION"
        exit 1
        ;;
esac
