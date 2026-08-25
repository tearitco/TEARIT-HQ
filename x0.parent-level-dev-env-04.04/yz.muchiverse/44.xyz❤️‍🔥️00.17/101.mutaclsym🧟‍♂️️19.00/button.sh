#!/bin/bash
# button.sh - launcher for mutaclysm, rebuilt on piececraft-xyz's
# architecture (session-isolation, interact+module chtpm pattern,
# board-viewer widget, clock daemon lifecycle).
ACTION="${1:-help}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
HOUSE_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

kill_own_board_widget() {
    pkill -f "board-viewer/button.sh run-widget $SCRIPT_DIR" 2>/dev/null || true
}

kill_own_clock_daemon() {
    pkill -f "ops/\+x/mua_clock_daemon\.\+x" 2>/dev/null || true
    rm -f "$SCRIPT_DIR/pieces/system/mua_clock_daemon.pid" 2>/dev/null || true
}

# REAL BUG FIX 2026-08-18, direct user report (TWO live chtpm_rgb_render
# processes found running against the SAME session dir, racing on the same
# output files - one composited a fresh frame, the other's own stale
# internal state overwrote it right back, making real key-driven movement
# invisible even though the underlying game state was updating correctly
# the whole time). `bash button.sh kill`'s own existing `pkill -f
# "system/chtpm_rgb_render"` call did NOT actually catch this - matches a
# real, already-documented house-wide gotcha (confirmed elsewhere this
# session's own memory: pkill -f pattern matching can silently fail to
# match against long, emoji-heavy project paths in some shells/
# environments, not specific to this one binary). A single pkill call is
# not trustworthy enough to guarantee zero stray processes before a fresh
# launch - explicit PID enumeration + individual kill + re-verify is the
# real, established house remediation for this exact bug class (see
# _.0.aigent-testing-k9.txt's own SCOPE ADDENDUM 2026-08-13, "PROCEDURE"
# section). Direct user instruction: "make sure code protects against
# that" - this function is now called automatically at the START of
# every `run`, not left as a manual habit to remember each time. */
kill_own_stray_processes() {
    # REAL SAFETY REQUIREMENT (must not weaken): this house runs MANY
    # separate projects that each launch their OWN same-named binaries
    # (system/gl_mirror, system/chtpm_rgb_render, etc, all sharing the
    # exact same relative path convention). A bare `pgrep -f
    # "system/gl_mirror"` matches ALL of them, project-agnostic - killing
    # by that alone would destroy a DIFFERENT project's own live,
    # legitimate session (e.g. a backup/reference window the user is
    # actively using side-by-side). Every kill below is scoped by
    # checking each matched PID's own real /proc/$pid/cwd against THIS
    # project's own $SCRIPT_DIR specifically - a process only dies if it
    # is truly running FROM somewhere under this project's own tree.
    local pattern pids pid cwd
    for pattern in "system/chtpm_rgb_render" "system/gl_mirror" "x11_mirror\.\+x" "system/prisc\+x" "system/chtpm_parser_pal" "system/orchestrator"; do
        pids=$(pgrep -f "$pattern" 2>/dev/null || true)
        for pid in $pids; do
            cwd=$(readlink -f "/proc/$pid/cwd" 2>/dev/null)
            case "$cwd" in
                "$SCRIPT_DIR"*) kill -9 "$pid" 2>/dev/null || true ;;
            esac
        done
    done
    sleep 0.3
    # Real re-verify, not just trust the loop above ran - same scoped
    # check, one more pass, before allowing a fresh launch to proceed.
    for pattern in "system/chtpm_rgb_render" "system/gl_mirror"; do
        pids=$(pgrep -f "$pattern" 2>/dev/null || true)
        for pid in $pids; do
            cwd=$(readlink -f "/proc/$pid/cwd" 2>/dev/null)
            case "$cwd" in
                "$SCRIPT_DIR"*) kill -9 "$pid" 2>/dev/null || true ;;
            esac
        done
    done
}

case "$ACTION" in
    compile|c|build)
        bash "$SCRIPT_DIR/scripts/build.sh"
        ;;
    run|r|start)
        cd "$SCRIPT_DIR"
        # REAL BUG FIX 2026-08-18, direct user instruction ("i think it
        # should pop open as a new tab in terminal, only if not launched
        # from terminal. this is the same behavior we want for tb
        # refactor."): the taskbar's own real launch command
        # (khtpm_taskbar_manager.c, "livedesk:open-toy:" handler) runs
        # `setsid nohup sh -c 'sh "button.sh" run' >/dev/null 2>&1 &` -
        # setsid strips any controlling terminal and stdout/stderr are
        # binned, so system/keyboard_input (raw termios) has no real tty
        # to read from and system/renderer's ASCII output (see the
        # REND_PID fix above, same pass) has nowhere to be seen even
        # though it's running. Detect that case (stdin/stdout NOT a real
        # tty - true for the taskbar's detached launch, false when a user
        # runs `./button.sh run` by hand in an already-open terminal) and
        # only THEN re-exec this same command inside a fresh
        # gnome-terminal tab, so there's a real tty for keyboard_input/
        # renderer to attach to. When already running inside a real
        # terminal, this is a no-op passthrough - never spawns a
        # redundant second tab. gnome-terminal is the real, confirmed-
        # installed terminal on this system (x-terminal-emulator's own
        # alternative already points at it) - fall through to running
        # headless (old behavior) if it's ever missing, rather than
        # failing the launch outright.
        if [ ! -t 0 ] || [ ! -t 1 ]; then
            if command -v gnome-terminal >/dev/null 2>&1; then
                gnome-terminal --tab -- bash -c "\"$SCRIPT_DIR/button.sh\" run; exec bash"
                exit 0
            fi
        fi
        if [ ! -x "$SCRIPT_DIR/system/orchestrator" ]; then
            echo "Compiling..."
            bash "$SCRIPT_DIR/scripts/build.sh"
        fi
        kill_own_stray_processes
        kill_own_board_widget
        kill_own_clock_daemon
        SESSION_ID="$(date +%s)-$$"
        SESSION_DIR="$SCRIPT_DIR/pieces/sessions/$SESSION_ID"
        mkdir -p "$SESSION_DIR/pieces/system" "$SESSION_DIR/pieces/display" \
                 "$SESSION_DIR/pieces/apps/player_app" "$SESSION_DIR/pieces/keyboard" \
                 "$SESSION_DIR/pieces/os" "$SESSION_DIR/projects/mutaclysm/manager"
        mkdir -p "$SCRIPT_DIR/data"
        # DEFENSIVE FIX 2026-08-20 (found while fixing the identical real
        # bug in my-chara-txt's own button.sh, see SIMLINK_PITFALL.md):
        # compose_frame.c and friends write into pieces/apps/player_app/
        # via PRISC_PROJECT_ROOT (now $SCRIPT_DIR) - this directory was
        # only ever created under $SESSION_DIR by this script, never at
        # $SCRIPT_DIR itself. This project has been working by luck only
        # (a pre-existing pieces/apps/player_app/ directory left over from
        # before the symlink-elimination pass) - make it real/guaranteed
        # rather than relying on that accident persisting.
        mkdir -p "$SCRIPT_DIR/pieces/apps/player_app"
        mkdir -p "$SCRIPT_DIR/pieces/display"

        # No symlinks — C processes resolve shared/persistent files via
        # PRISC_PROJECT_ROOT env var (set below), session-specific files
        # via CWD (getcwd).

        cd "$SESSION_DIR"
        : > pieces/apps/player_app/interact_relay.txt
        : > pieces/keyboard/history.txt
        : > "$SCRIPT_DIR/pieces/system/quit_flag.txt"
        : > pieces/display/pc_screen_changed.txt
        : > pieces/display/frame_changed.txt
        : > projects/mutaclysm/manager/gui_state.txt
        echo "$HOUSE_DIR" > pieces/system/house_root.txt
        echo "$SCRIPT_DIR" > pieces/system/real_project_root.txt

        # Auto-generate the voxel world on first launch (no manual "Confirm &
        # Start" step - direct user instruction: render map immediately).
        # World persists across relaunches (chunks/ lives in SCRIPT_DIR, not
        # the disposable session dir), so this only runs once ever.
        if [ ! -d "$SCRIPT_DIR/pieces/system/chunks/chunk_0_0" ]; then
            SEED=$(( $(date +%s) ^ $$ ))
            PRISC_PROJECT_ROOT="$SCRIPT_DIR" "$SCRIPT_DIR/ops/+x/mua_generate_chunk.+x" "$SEED" 0 0 >>"$SCRIPT_DIR/pieces/system/orchestrator.log" 2>&1
            # mua_generate_chunk.+x doesn't write render_mode/camera fields -
            # compose_frame.c's own MAP3D_MARKER emit (line ~1088) reads
            # render_mode from hero_01/state.txt with a bare 0 default, so
            # without this the 3D view never activates at all. Real bug
            # found+fixed 2026-08-17 (screen stayed on 2D "[Map Loading...]"
            # fallback forever). render_mode=1 (3D), camera_mode=2
            # (third-person, matches board-viewer's own default).
            if [ -f "$SCRIPT_DIR/pieces/hero_01/state.txt" ]; then
                cat >> "$SCRIPT_DIR/pieces/hero_01/state.txt" << 'EOHEROEXTRA'
render_mode=1
camera_mode=2
cam_yaw=180
cam_pitch=6
cam_pan_x=0
cam_pan_y=0
cam_pan_z=0
cam_z_level=0
interact_mode=0
EOHEROEXTRA
            fi
        fi

        # SIMLINK ELIMINATION 2026-08-20 (see SIMLINK_PITFALL.md): the OLD
        # 2D-map-era path pieces/world_01/map_start/hero/state.txt used to be
        # symlinked at pieces/hero_01/state.txt (the real, NEW voxel-world
        # location) because ~14 ops files + the shared chtpm_parser_pal.c
        # all hardcoded the OLD path. That symlink was recreated on EVERY
        # launch, which meant it always came right back even after deleting
        # it - a genuine blocker for Windows (which chokes on symlinks
        # without Developer Mode + git symlink support). Real fix: every one
        # of those ~14 ops files (and the shared chtpm_parser_pal.c's own
        # set_interact_mode()) was updated to reference pieces/hero_01/
        # state.txt / pieces/hero_01/inventory directly - hero_01 is the
        # canonical, more-heavily-used convention (board-viewer's own
        # possession-id system already keys off "hero_01" as an entity ID,
        # not just a path), so redirecting the FEW old-path readers was less
        # risk than redirecting the MANY hero_01 ones. No symlink needed at
        # all now - pieces/world_01/map_start/hero/ is dead, unused.
        mkdir -p "$SCRIPT_DIR/pieces/hero_01/inventory"
        if [ -f "$SCRIPT_DIR/pieces/hero_01/state.txt" ]; then
            # REAL FIX 2026-08-18, direct user report ("keyboard input is
            # passing into interact move (from nav eat/pickup) unwantededly
            # ... non interact = menu nav, interact mode = map interact, no
            # menu nav"): interact_mode lives in hero_01/state.txt, which
            # deliberately PERSISTS across relaunches (so player progress
            # isn't lost) - but chtpm_parser_pal.c's own active_index
            # (which decides whether the INTERACT-onClick element is
            # currently engaged) DOES reset to -1 on every fresh launch
            # (main()'s own real, unmodified startup code). A player who
            # quit mid-interact-mode left hero_01/state.txt's
            # interact_mode=1 behind; the next launch started nav-only
            # (active_index=-1, correct) while game-side code still
            # believed interact/map-control was engaged - a real, live
            # two-state desync, not by design. Force interact_mode=0 on
            # EVERY launch, every time, regardless of whether a fresh
            # world was just generated - this is UI/session state, not
            # game progress, and must always match active_index's own
            # always-reset behavior.
            if grep -q "^interact_mode=" "$SCRIPT_DIR/pieces/hero_01/state.txt"; then
                sed -i 's/^interact_mode=.*/interact_mode=0/' "$SCRIPT_DIR/pieces/hero_01/state.txt"
            else
                echo "interact_mode=0" >> "$SCRIPT_DIR/pieces/hero_01/state.txt"
            fi
            # REVERTED 2026-08-18, direct user correction: the real,
            # intended design is xlector-free-by-default (arrows move the
            # xlector cursor, hero stays still) UNTIL the player explicitly
            # possesses the hero via key '9' (arrows then move the hero,
            # xlector stays put where it was). Forcing possessed_id=hero
            # here at every launch broke that - it made the game start
            # ALREADY possessing, with no way to ever get back to free
            # xlector movement. possessed_id is left alone here - "none"
            # is choice.c's own real default when the field is absent,
            # which is the correct starting state.
            #
            # REAL FIX: choice.c's key '9' handler already has a working
            # "reverse-jump" possess toggle (see its own ~line 1123:
            # `else if (strcmp(last_possessed_id, "none") != 0)` -
            # re-possesses whatever was last released, jumping the
            # xlector to it). It only ever fires if last_possessed_id was
            # already set from a PRIOR real possession - nothing seeds it
            # for a fresh game, so the very first '9' press was
            # permanently a no-op (neither the "release" branch nor the
            # "reverse-jump" branch matches on a truly fresh state). Seed
            # last_possessed_id=hero (NOT possessed_id) so the FIRST '9'
            # press correctly reverse-jumps into possessing the hero,
            # reusing this existing, already-correct code path rather
            # than writing new grant-possession logic from scratch.
            # ONLY seed if the field is genuinely absent (append-if-
            # missing, never sed-overwrite) - unlike interact_mode above,
            # this IS real gameplay state that should be free to evolve
            # once the player actually starts possessing/releasing things
            # (forcing it back to "hero" on every launch would clobber a
            # real, legitimate later possession target).
            if ! grep -q "^last_possessed_id=" "$SCRIPT_DIR/pieces/hero_01/state.txt"; then
                echo "last_possessed_id=hero" >> "$SCRIPT_DIR/pieces/hero_01/state.txt"
            fi
        fi

        if [ ! -f "$SCRIPT_DIR/pieces/system/config.txt" ]; then
            mkdir -p "$SCRIPT_DIR/pieces/system"
            cat > "$SCRIPT_DIR/pieces/system/config.txt" << 'EOCONFIG'
game_id=mutaclysm-001
turn=1
hunger=100
thirst=100
stamina=100
hp=100
max_hp=100
evasion=0
defense=0
level=1
exp=0
gold=0
game_state=title
EOCONFIG
        fi
        touch "$SCRIPT_DIR/pieces/system/board.txt"
        touch "$SCRIPT_DIR/pieces/system/entities.txt"
        # No symlinks for persistent state — C processes read/write directly
        # via PRISC_PROJECT_ROOT (config.txt, board.txt, entities.txt,
        # board_manifest.txt, chunks/, terrain_legend.txt, hero_01/,
        # world_01/, xelector_01/).

        # muta_render_3d.+x is a verbatim copy of board-viewer's own
        # bv_render_3d.c, which is designed as a widget that renders
        # whichever "focused" host project bv_state.txt points at (its own
        # multi-host architecture) - it does NOT render its own project by
        # default. Real bug found+fixed 2026-08-17: without this at all,
        # muta_render_3d's own main() returned 0 immediately (empty
        # focused_project_root), writing no overlay at all.
        #
        # REAL FIX 2026-08-18, direct user correction ("stop guessing,
        # copy board-viewer's working model"): this was pointed at
        # $SESSION_DIR (the disposable per-launch session dir) - WRONG.
        # Checked piececraft-xyz's own real, confirmed-working usage
        # directly: its own OPEN_BOARD_WIDGET spawn (ops/pc_menu_input.c)
        # passes board-viewer the value of pieces/system/
        # real_project_root.txt - the PERSISTENT project root, never a
        # session dir. This is exactly why terrain_legend.txt (and every
        # other pieces/system/* file) never needed individual per-session
        # symlinks for board-viewer's own real usage: focused_project_root
        # pointing at $SCRIPT_DIR reads those files DIRECTLY from their
        # one real, persistent location, no session-dir copy/symlink
        # dance needed at all. Point it at $SCRIPT_DIR here too, matching
        # that real, working precedent exactly instead of guessing at
        # which individual files need session-symlinking one at a time.
        # REAL BUG FIX 2026-08-20, found live via direct file-relay testing
        # (camera_mode/cam_yaw update correctly in hero_01/state.txt and
        # even sync correctly via game_dispatch.c's own
        # sync_camera_to_bv_state(), but the GL view never visibly changes):
        # chtpm_parser_pal.c (shared, unmodified) does chdir(project_root_path)
        # before launching the PAL module - and PRISC_PROJECT_ROOT is now
        # $SCRIPT_DIR (post symlink-elimination refactor, was $SESSION_DIR
        # before). That means game_dispatch/muta_render_3d/compose_frame all
        # actually run with CWD=$SCRIPT_DIR now, not $SESSION_DIR - so their
        # own RELATIVE "pieces/system/bv_state.txt" reads/writes resolve to
        # $SCRIPT_DIR/pieces/system/bv_state.txt, a completely different file
        # from the one seeded below if only the session copy is written.
        # Fix: seed the copy the processes ACTUALLY use ($SCRIPT_DIR), not
        # just the disposable $SESSION_DIR one (kept too, harmless, in case
        # anything else still reads it via session-relative CWD).
        echo "focused_project_root=$SCRIPT_DIR" > "$SCRIPT_DIR/pieces/system/bv_state.txt"
        echo "focused_project_root=$SCRIPT_DIR" > "$SESSION_DIR/pieces/system/bv_state.txt"
        # REAL BUG FIX 2026-08-18, direct user diagnosis ("it maybe waiting
        # to snap to its real camera mode after key input... make it do
        # its camera mode before loop"): muta_render_3d.c reads ALL camera
        # fields (camera_mode/cam_yaw/cam_pitch/cam_pan_*/cam_z_level/
        # render_mode) from bv_state.txt - but ops/game_dispatch.c's own
        # sync_camera_to_bv_state() (the ONLY thing that ever writes those
        # fields there) only runs INSIDE the per-key any_key branch, never
        # at startup. pal/game_module.pal calls muta_render_3d ONCE before
        # entering its own per-tick loop - at that exact moment,
        # bv_state.txt has ONLY the focused_project_root line just written
        # above, no camera fields at all, so that FIRST render silently
        # fell back to muta_render_3d's own hardcoded defaults (yaw=180,
        # default_camera_mode(), etc), completely ignoring whatever REAL
        # camera_mode/cam_yaw hero_01/state.txt already had. Only once the
        # player's FIRST key got processed did sync_camera_to_bv_state()
        # ever run for the first time, and the render that followed THAT
        # key correctly picked up the real values - visually
        # indistinguishable from "the first keypress changed the camera
        # mode," exactly matching the user's own live-observed symptom.
        # Fix: seed bv_state.txt with the SAME real fields right here, at
        # launch, mirroring sync_camera_to_bv_state()'s own exact field
        # list, so the very FIRST render already shows the real,
        # persisted camera state - no snap-on-first-input.
        if [ -f "$SCRIPT_DIR/pieces/hero_01/state.txt" ]; then
            HERO_STATE="$SCRIPT_DIR/pieces/hero_01/state.txt"
            for FLD in cam_yaw cam_pitch cam_pan_x cam_pan_y cam_pan_z cam_z_level camera_mode render_mode; do
                VAL=$(grep "^${FLD}=" "$HERO_STATE" | tail -1 | cut -d= -f2-)
                if [ -n "$VAL" ]; then
                    echo "${FLD}=${VAL}" >> "$SCRIPT_DIR/pieces/system/bv_state.txt"
                    echo "${FLD}=${VAL}" >> "$SESSION_DIR/pieces/system/bv_state.txt"
                fi
            done
        fi

        mkdir -p "$SCRIPT_DIR/pieces/system/widget_cmds"
        touch "$SCRIPT_DIR/pieces/system/widget_cmds/inbox.txt"
        cat > "$SCRIPT_DIR/pieces/system/board_widget_bridge.txt" << EOF
inbox_path=pieces/system/widget_cmds/inbox.txt
kind=board_game
project_id=mutaclysm
display_name=Mutaclysm
EOF

        cat > pieces/apps/player_app/state.txt << 'EOSTATE'
module_path=system/prisc+x pal/game_module.pal
project_id=mutaclysm
active_target_id=main
EOSTATE

        export PRISC_PROJECT_ROOT="$SCRIPT_DIR"
        export PRISC_PROJECT_ID="mutaclysm"
        export NO_NET=1
        export PAL_LAYOUT="$SCRIPT_DIR/pieces/chtpm/layouts/main.chtpm"
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
            # REAL BUG FIX 2026-08-18, direct user report ("keyboard used
            # to move xelector/player... u probably tried to fix
            # something last pass" - this was actually a PRE-EXISTING
            # bug, exposed once the user compared this session's own
            # gl_mirror-based window against a real x11_mirror-based
            # window from a backup project and asked why they differ):
            # SCRIPT_DIR is already this project's own root - &.widgits
            # is a DIRECT SIBLING (both live in 44.xyz.../ together), one
            # level up, not two. The old "../.." path silently resolved
            # to a nonexistent location, so `-x "$SHARED_MIRROR"` always
            # failed and this project fell through to the OLDER,
            # non-shared ./system/gl_mirror binary on every single
            # launch - explaining both the "wrong window style" report
            # AND, per direct user instruction, the real, LONG-TERM
            # intent to standardize on x11_mirror's own real chrome
            # bar/exit button across projects (same real path-math bug
            # class already found+fixed this session for wsr-pal's own
            # emoji binaries).
            SHARED_MIRROR="$SCRIPT_DIR/../&.widgits/_shared-lib/ops/+x/x11_mirror.+x"
            if [ -z "$FORCE_GL_MIRROR" ] && [ -x "$SHARED_MIRROR" ]; then
                "$SHARED_MIRROR" "$SESSION_DIR" >/dev/null 2>&1 &
                GL_PID=$!
            elif [ -x "$SCRIPT_DIR/system/gl_mirror" ]; then
                "$SCRIPT_DIR/system/gl_mirror" >/dev/null 2>&1 &
                GL_PID=$!
            fi
            if [ -x "$SCRIPT_DIR/system/chtpm_rgb_render" ]; then
                "$SCRIPT_DIR/system/chtpm_rgb_render" >/dev/null 2>&1 &
                RGB_PID=$!
            fi
        fi

        # REAL BUG FIX 2026-08-18, direct user report ("og mutaclysm
        # actually opened from terminal and showed 2d ascii map using
        # same controls... i want it 2 even tho its in 'headless mode'"):
        # system/renderer.c (the terminal ASCII half of the real dual-
        # render standard - see TPMOS's own orchestrator.c, which ALWAYS
        # launches its terminal renderer thread alongside the GUI one,
        # no either/or branching) was built and present on disk but never
        # actually launched anywhere in this verb - only keyboard_input
        # ran in the foreground terminal (capture-only, no display). Runs
        # unconditionally (not gated on $DISPLAY/$NO_GL like the GL/RGB
        # block above) since the terminal renderer is the one path that
        # works with no X server at all - output is silenced to match
        # keyboard_input's own already-silent capture ("headless by
        # design, but the capability should still exist" per direct user
        # framing), not printed to the launching terminal.
        REND_PID=""
        if [ -x "$SCRIPT_DIR/system/renderer" ]; then
            "$SCRIPT_DIR/system/renderer" >/dev/null 2>&1 &
            REND_PID=$!
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

        trap 'kill "$ORCH_PID" "$GL_PID" "$RGB_PID" "$REND_PID" 2>/dev/null; wait "$ORCH_PID" 2>/dev/null; kill_own_module; kill_own_board_widget; kill_own_clock_daemon; rm -rf "$SESSION_DIR"' EXIT INT TERM

        "$SCRIPT_DIR/system/keyboard_input"

        kill "$ORCH_PID" "$GL_PID" "$RGB_PID" "$REND_PID" 2>/dev/null
        kill_own_module
        ;;
    kill|k|stop)
        pkill -f "system/keyboard_input" 2>/dev/null
        pkill -f "system/renderer" 2>/dev/null
        pkill -f "system/prisc\+x" 2>/dev/null
        pkill -f "system/chtpm_parser_pal" 2>/dev/null
        pkill -f "system/chtpm_rgb_render" 2>/dev/null
        pkill -f "system/gl_mirror" 2>/dev/null
        pkill -f "x11_mirror.+x.*pieces/sessions" 2>/dev/null
        SCRIPT_DIR_RE=$(printf '%s' "$SCRIPT_DIR" | sed 's/[.[\*^$()+?{|]/\\&/g')
        pkill -f "$SCRIPT_DIR_RE/system/orchestrator" 2>/dev/null
        kill_own_board_widget
        kill_own_clock_daemon
        echo "done"
        ;;
    check|verify)
        for b in system/prisc+x system/keyboard_input system/renderer \
                 system/chtpm_parser_pal system/orchestrator \
                 ops/+x/mua_menu_input.+x ops/+x/mua_compose_frame.+x \
                 ops/+x/muta_render_3d.+x ops/+x/muta_compose_frame_3d.+x \
                 ops/+x/muta_menu_input_3d.+x \
                 ops/+x/game_dispatch.+x ops/+x/camera_control.+x \
                 ops/+x/move_player.+x ops/+x/choice.+x \
                 ops/+x/end_turn.+x ops/+x/tick_monsters.+x \
                 ops/+x/pickup.+x ops/+x/drop.+x ops/+x/eat.+x \
                 ops/+x/craft.+x ops/+x/examine.+x ops/+x/save_game.+x \
                 ops/+x/toggle_emoji.+x ops/+x/compose_frame.+x \
                 ops/+x/compose_rgb_frame.+x ops/+x/dump_rgb_png.+x \
                 ops/+x/pdl_reader.+x ops/+x/title_input.+x \
                 ops/+x/compose_title_frame.+x ops/+x/generate_map.+x \
                 ops/+x/muta_widget_cmds.+x; do
            if [ -x "$SCRIPT_DIR/$b" ]; then echo "OK   $b"; else echo "MISSING $b"; fi
        done
        for b in system/chtpm_rgb_render system/gl_mirror; do
            if [ -x "$SCRIPT_DIR/$b" ]; then echo "OK   $b (optional GL mirror)"; else echo "OPTIONAL-MISS $b"; fi
        done
        if [ -x "$SCRIPT_DIR/../&.widgits/_shared-lib/ops/+x/x11_mirror.+x" ]; then
            echo "OK   shared x11_mirror (preferred display mirror)"
        else
            echo "SKIP shared x11_mirror (see &.widgits/_shared-lib/ops/build_x11_mirror.sh)"
        fi
        ;;
    help|h|-h|--help)
        echo "mutaclysm button.sh"
        echo ""
        echo "Usage: ./button.sh <action>"
        echo "  compile, c, build   - Build prisc+x + ops"
        echo "  run, r              - Play mutaclysm (interactive, needs a real terminal)"
        echo "  kill, k, stop       - Kill any lingering mutaclysm processes"
        echo "  check, verify       - Verify all binaries exist"
        echo "  help, h             - Show this help"
        ;;
    *)
        echo "Unknown action: $ACTION"
        exit 1
        ;;
esac
