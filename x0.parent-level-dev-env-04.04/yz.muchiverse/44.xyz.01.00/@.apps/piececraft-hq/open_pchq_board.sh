#!/bin/sh
# open_pchq_board.sh — launch piececraft-hq's board window the x11-hq
# standard way: resolve paths, single-instance guard (kill any existing
# board window + projector, TERM->KILL), make sure a board-viewer engine
# session exists for the <canvas> to mirror, then launch the shared
# khtpm_core_render against pchq-board.xhtpm and record its PID in the
# proc-ledger so a taskbar quit reaps it.
#
# Design: PIECECRAFT-HQ-LAUNCH-STANDARDIZE.md. Modelled on
# &.hq-apps/stats-hq/open_stats_hq.sh. This REPLACES `button.sh run` as
# the taskbar entry point (dispatch: livedesk:open-piececraft-hq).
# `button.sh run` stays the terminal/dev path.
#
# Usage: open_pchq_board.sh [<house_root>]
#   - HQ-menu dispatch (livedesk:open-piececraft-hq) passes <house_root>.
#   - toys-menu dispatch (livedesk:open-toy:.../open_pchq_board.sh)
#     passes the literal "run" - in that case walk up to the house root.
set -u

PKG="$(cd "$(dirname "$0")" && pwd)"

HOUSE_ROOT="${1:-}"
if [ -z "$HOUSE_ROOT" ] || [ ! -d "$HOUSE_ROOT" ]; then
    # not a directory (e.g. "run" from the toys menu, or nothing) -
    # walk up from this script to the dir holding #.desktop + &.widgits.
    _d="$PKG"
    while [ "$_d" != "/" ] && { [ ! -d "$_d/#.desktop" ] || [ ! -d "$_d/&.widgits" ]; }; do
        _d="$(dirname "$_d")"
    done
    HOUSE_ROOT="$_d"
fi
if [ ! -d "$HOUSE_ROOT/#.desktop" ]; then
    echo "open_pchq_board: could not resolve house root (arg='${1:-}')" >&2
    exit 1
fi
HOUSE_ROOT="$(cd "$HOUSE_ROOT" && pwd)"

# pc-hq sessions (PCHQ-SESSIONS-DESIGN.md): PCHQ_PKG = the DATA root of one pc-hq instance (a game's own world / hero / xelector / maps / engine sessions); the PROGRAMS stay in $PKG (shared, no symlinks).
# Unset = the default instance (this folder), exactly as before. Everything that used to be "the" board is now scoped to the instance: the single-instance guard, the stale-session reaping, the engine check.
DATA="$PKG"
if [ -n "${PCHQ_PKG:-}" ]; then
    mkdir -p "$PCHQ_PKG" && DATA="$(cd "$PCHQ_PKG" && pwd)"
    [ "$DATA" = "$PKG" ] || [ -f "$DATA/pieces/system/.instance-ready" ] || sh "$PKG/ops/pchq_instance_init.sh" "$DATA" || { echo "open_pchq_board: could not initialise instance $DATA" >&2; exit 1; }
fi
INST="$(basename "$DATA")"
# which instance does a running pc-hq process belong to? Its environment carries PCHQ_PKG (empty = the default instance).
same_instance() { _v="$(tr '\0' '\n' < "/proc/$1/environ" 2>/dev/null | sed -n 's/^PCHQ_PKG=//p' | head -1)"; [ -n "$_v" ] && _v="$(cd "$_v" 2>/dev/null && pwd)"; [ "${_v:-$PKG}" = "$DATA" ]; }

OPS_DIR="$HOUSE_ROOT/_.monads/_.livedesk-taskbar/ops"
BIN="$OPS_DIR/+x/khtpm_core_render.+x"
PROJECTOR="$PKG/ops/+x/pchq_board_projector.+x"
# MILESTONE A: default is the sidebar+panel board window. Set
# PCHQ_BOARD_HASCANVAS=1 to fall back to the old flat has_canvas
# template while the reframe is being shaken out.
if [ -n "${PCHQ_BOARD_HASCANVAS:-}" ] && [ -f "$PKG/pchq-board.hascanvas.xhtpm" ]; then
    BOARD_TPL="$PKG/pchq-board.hascanvas.xhtpm"
else
    BOARD_TPL="$PKG/pchq-board.xhtpm"
fi
# PCHQ_BOARD_TPL=<path to a template named pchq-board.xhtpm> launches that template instead (the in-game layouts
# sandbox, @.apps/layout-studio/sandbox/); the name must stay pchq-board.xhtpm so the kill/match patterns still hit.
if [ -n "${PCHQ_BOARD_TPL:-}" ] && [ -f "$PCHQ_BOARD_TPL" ]; then BOARD_TPL="$PCHQ_BOARD_TPL"; fi
# which folder the running board treats as its package dir (state/ lives there): pc_entity_ctx.sh publishes an
# in-board context menu there when <dir>/state/ctx_overlay.on exists (IN-GAME-LAYOUTS-PLAN.md, context menus)
if [ "$DATA" = "$PKG" ]; then
    dirname "$BOARD_TPL" > "$HOUSE_ROOT/#.desktop/pchq_ctx_dir.txt" 2>/dev/null || true
else
    mkdir -p "$DATA/pieces/system"; dirname "$BOARD_TPL" > "$DATA/pieces/system/ctx_dir.txt" 2>/dev/null || true   # a second instance must not repoint the default board's context menus
fi

# ── locations: generated at EVERY launch from this script's own folder (TPMOS pattern: its location_kvp is generated,
# never checked in; here pieces/system/locations.pdl). Nothing below depends on where the checkout lives: a saved pointer
# is house-relative and resolved against house_root (_shared-lib/khtpm_locations.c). house_root.txt, which many ops read,
# is refreshed here too (it used to be a tracked file with one machine's absolute path in it).
mkdir -p "$DATA/pieces/system"
_UH="$(ls -d "$HOUSE_ROOT"/xyzfs/users/*/home/livedesk 2>/dev/null | head -1)"
{
    printf 'SECTION      | KEY                | VALUE\n----------------------------------------\n'
    printf 'LOCATION     | house_root         | %s\n' "$HOUSE_ROOT"
    printf 'LOCATION     | pchq_root          | %s\n' "$DATA"
    printf 'LOCATION     | board_dir          | %s\n' "$(dirname "$BOARD_TPL")"
    printf 'LOCATION     | desk_user_home     | %s\n' "$_UH"
} > "$DATA/pieces/system/locations.pdl" 2>/dev/null || true
printf '%s\n' "$HOUSE_ROOT" > "$DATA/pieces/system/house_root.txt" 2>/dev/null || true
# a template that carries the in-board menu row (id="ctx") needs its state files; <dir>/state/ is gitignored for the
# live board, so create them when missing (the menu stays hidden until pc_entity_ctx.sh writes ctx_visible=1)
if grep -q 'id="ctx"' "$BOARD_TPL" 2>/dev/null; then
    _CD="$(dirname "$BOARD_TPL")/state"; mkdir -p "$_CD"
    [ -f "$_CD/ctx.txt" ] || printf 'ctx_visible=0\n' > "$_CD/ctx.txt"
    [ -f "$_CD/ctx_menu.chtpm" ] || printf '<window class="entity-menu"><page name="main"><text label="menu"/></page></window>\n' > "$_CD/ctx_menu.chtpm"
    [ -f "$_CD/ctx_overlay.on" ] || echo 1 > "$_CD/ctx_overlay.on"
fi

# ── build-on-demand (same shape as open_stats_hq.sh) ─────────────────
if [ ! -x "$BIN" ]; then
    (cd "$OPS_DIR" && sh build_core_render.sh) >/dev/null 2>&1 || true
fi
if [ ! -x "$BIN" ]; then
    echo "open_pchq_board: missing $BIN (build_core_render.sh failed)" >&2
    exit 1
fi
[ -x "$PROJECTOR" ] || \
    sh "$PKG/ops/build_pchq_board_projector.sh" >/dev/null 2>&1 || true

# ── board-viewer build-on-demand (2026-10-01) ─────────────────────────
# .gitignore:9 is `*.+x`, so EVERY compiled binary in the house is
# untracked build output. A `git clean -xdf`, a branch switch, or a fresh
# clone therefore empties board-viewer's ops/+x/ while git still reports
# a perfectly clean tree - which is why this looked like it correlated
# with merging two near-identical branches (the merge was innocent; it
# just pruned ignored files).
#
# When ops/+x/ is empty, THREE separate guards fail SILENTLY and the
# board opens blank with no error anywhere:
#   1. board-viewer/button.sh's `if [ -x ./ops/+x/ledger_append.+x ]`
#      skips ONLINE registration, so the live session never reaches the
#      ledger and is undiscoverable;
#   2. pchq_board_projector.c's `popen("...ledger_peers.+x... 2>/dev/null")`
#      swallows the missing binary, yielding no_session=1;
#   3. with no session, canvas_raw resolves empty, so the renderer's
#      canvas blits nothing - the 2D/3D view simply never appears.
#
# So build board-viewer's ops the same way we already build BIN and
# PROJECTOR above: if any binary it needs is missing, rebuild the lot.
BV_OPS="$HOUSE_ROOT/&.widgits/board-viewer/ops"
BV_MISSING=""
for _bv in ledger_append ledger_peers bv_render_2d bv_render_3d bv_compose_frame; do
    [ -x "$BV_OPS/+x/$_bv.+x" ] || BV_MISSING="$_bv "
done
if [ -n "$BV_MISSING" ]; then
    echo "open_pchq_board: board-viewer ops missing ($BV_MISSING)- building" >&2
    (cd "$HOUSE_ROOT/&.widgits/board-viewer" && sh scripts/build.sh) \
        >/tmp/pchq_board_viewer_build.log 2>&1 || true
    # Re-check and FAIL LOUDLY if still absent. Better a clear refusal to
    # open than a silently blank board that looks like a rendering bug.
    for _bv in $BV_MISSING; do
        if [ ! -x "$BV_OPS/+x/$_bv.+x" ]; then
            echo "open_pchq_board: board-viewer $_bv.+x STILL missing after build" >&2
            echo "open_pchq_board: see /tmp/pchq_board_viewer_build.log" >&2
            exit 1
        fi
    done
    echo "open_pchq_board: board-viewer ops rebuilt ok" >&2
fi

if [ ! -f "$BOARD_TPL" ]; then
    echo "open_pchq_board: missing $BOARD_TPL" >&2
    exit 1
fi

# ── always load at level 1 ────────────────────────────────────────
# Owner 2026-10-05: the floor is level 0 and a board always opens at level 1 (the layer the hero, xelector and
# entities live on), never up in the sky where the last session left the xelector. The world publishes
# floor_z in board_manifest.txt (pc_generate_chunk.c); reset the hero and xelector to floor_z + 1 before
# the engine starts. A world without floor_z is left exactly as it was.
_FZ="$(sed -n 's/^floor_z=//p' "$DATA/pieces/system/board_manifest.txt" 2>/dev/null | head -1)"
case "$_FZ" in
    ""|*[!0-9]*) ;;
    *) _LV=$((_FZ + 1))
       for _p in xelector_01 hero_01; do
           _f="$DATA/pieces/$_p/state.txt"
           [ -f "$_f" ] && sed -i "s/^pos_z=.*/pos_z=$_LV/" "$_f"
       done ;;
esac

# ── reap stale/orphaned engine sessions ────────────────────────────
# `button.sh run`'s own EXIT trap (rm -rf $SESSION_DIR + kill_own_*)
# only fires if THAT button.sh exits/gets SIGTERM cleanly. A strip
# restart or a hard kill orphans the whole stack (chtpm_parser_pal x2,
# renderer x2, prisc+x, board-viewer widget) - reparented to init,
# every one still spinning its 60Hz poll loop forever. That was the
# real "pc-hq is laggy" bottleneck (dozens of orphan pollers starving
# the live engine). Reap any engine proc whose cwd is under one of
# this project's session dirs and whose parent is gone (ppid 1), then
# drop the dead session dir.
SESS_BASES="$DATA/pieces/sessions
$HOUSE_ROOT/&.widgits/board-viewer/pieces/sessions"
_live_sessions=""
for _p in $(pgrep -f "keyboard_input|system/orchestrator|board-viewer/button.sh run-widget" 2>/dev/null); do
    _c="$(readlink "/proc/$_p/cwd" 2>/dev/null)"
    case "$_c" in *"/pieces/sessions/"*) _live_sessions="$_live_sessions $(basename "$_c")" ;; esac
done
for _p in $(pgrep -f "chtpm_parser_pal|system/renderer|prisc\+x|system/orchestrator" 2>/dev/null); do
    _c="$(readlink "/proc/$_p/cwd" 2>/dev/null)" || continue
    case "$_c" in
        "$DATA/pieces/sessions/"*|"$HOUSE_ROOT/&.widgits/board-viewer/pieces/sessions/"*) : ;;
        *) continue ;;
    esac
    _b="$(basename "$_c")"
    case " $_live_sessions " in *" $_b "*) continue ;; esac   # part of a live session
    # REAL FIX 2026-09-10, live-caught bug (mon_scan.sh flagged a real
    # stray stack this check kept missing - orchestrator/chtpm_parser_
    # pal/prisc+x rooted under `sh button.sh run`, itself parented to a
    # dead tool-shell pid 1003, but the LEAF proc's own immediate ppid
    # is some mid-tree pid, never 1 or 1003 directly). Checking only
    # $_p's own ppid missed every multi-level-deep engine child - the
    # whole `button.sh run` stack is one real X session (setsid), so
    # walk up to the SESSION LEADER (sid) and check ITS ppid instead;
    # that catches the entire tree regardless of how deep $_p sits in
    # it, not just direct children of the dead parent.
    _sid="$(ps -o sid= -p "$_p" 2>/dev/null | tr -d ' ')"
    [ -n "$_sid" ] || continue
    _leader_pp="$(ps -o ppid= -p "$_sid" 2>/dev/null | tr -d ' ')"
    # setsid parents the engine shell to init on purpose. Parent 1 while
    # that shell is alive is not an orphan (2026-10-29: a second open
    # during the first second SIGKILLed the new stack, pane stayed on
    # loading...). Orphan means the leader is gone, or it is alive and
    # is not the engine shell.
    if kill -0 "$_sid" 2>/dev/null; then
        _lcmd="$(ps -o cmd= -p "$_sid" 2>/dev/null || true)"
        case "$_lcmd" in
            *button.sh*|*orchestrator*) continue ;;
        esac
        [ "$_leader_pp" = "1" ] || [ "$_leader_pp" = "1003" ] || continue
    fi
    kill -KILL "$_p" 2>/dev/null || true
done
# drop session dirs with nothing live in them
echo "$SESS_BASES" | while IFS= read -r _base; do
    [ -d "$_base" ] || continue
    for _d in "$_base"/*/; do
        [ -d "$_d" ] || continue
        _bn="$(basename "$_d")"
        case " $_live_sessions " in *" $_bn "*) continue ;; esac
        # A session created this same second is the launch in progress.
        # Its orchestrator is not in pgrep yet. Do not delete it.
        _sts="${_bn%%-*}"
        case "$_sts" in ''|*[!0-9]*) ;; *)
            _age=$(( $(date +%s) - _sts ))
            [ "$_age" -ge 0 ] && [ "$_age" -lt 5 ] && continue ;;
        esac
        rm -rf "$_d" 2>/dev/null || true
    done
done

# Reap an orphaned `bash board-viewer/button.sh run-widget <this pkg>`
# whose session dir is gone (a hard-killed test / strip restart leaves
# the bash wrapper alive; ledger_peers still reports it ONLINE because
# its PID lives, so the projector latches a deleted bv_session -> blank
# canvas). If no live prisc VM has a cwd under an existing board-viewer
# session dir, the widget is dead weight - kill it.
for _w in $(pgrep -f "piececraft-hq/button.sh run(-widget)? [^ ]*/$INST( |\$)" 2>/dev/null); do
    _c="$(readlink "/proc/$_w/cwd" 2>/dev/null)" || _c=""
    case "$_c" in
        *'(deleted)') ;;
        *) [ -n "$_c" ] && [ -d "$_c" ] && continue ;;
    esac
    echo "open_pchq_board: reaping orphan board-viewer widget $_w (session dir gone)"
    kill -KILL "$_w" 2>/dev/null || true
done

# ── single-instance guard: clean-restart the board window ────────────
# Match the shared binary by its OWN chtpm path (bare-name match would
# hit every other khtpm_core_render window) + the projector.
# A window that runs EXACTLY this launch'"'"'s custom template (a game'"'"'s own board, PCHQ_BOARD_TPL) is this game'"'"'s board whatever its environment says: one started without PCHQ_PKG (owner 2026-10-12, a cdda-hq board with no
# engine session and no INT, left open beside the good one) used to survive the replace because same_instance() only matches the environment.
board_pids() { for _bp in $(pgrep -f "khtpm_core_render\.\+x .*pchq-board\.xhtpm" 2>/dev/null); do
    if same_instance "$_bp"; then echo "$_bp"; elif [ -n "${PCHQ_BOARD_TPL:-}" ] && tr '\0' '\n' < "/proc/$_bp/cmdline" 2>/dev/null | grep -qxF "$BOARD_TPL"; then echo "$_bp"; fi
done; true; }
proj_pids()  { for _bp in $(pgrep -f "pchq_board_projector\.\+x" 2>/dev/null); do same_instance "$_bp" && echo "$_bp"; done; true; }

existing="$(board_pids) $(proj_pids)"
existing="$(echo "$existing" | tr ' ' '\n' | grep -v '^$' | sort -u || true)"
if [ -n "$existing" ]; then
    echo "open_pchq_board: replacing board window: $(echo $existing | tr '\n' ' ')"
    echo "$existing" | xargs -r kill -TERM 2>/dev/null || true
    sleep 1
    still="$(board_pids) $(proj_pids)"
    still="$(echo "$still" | tr ' ' '\n' | grep -v '^$' | sort -u || true)"
    [ -n "$still" ] && { echo "$still" | xargs -r kill -KILL 2>/dev/null || true; sleep 1; }
fi

# ── ensure a board-viewer engine session exists ─────────────────────
# The <canvas> mirrors <bv_session>/pieces/display/rgb_frame_3d_overlay
# .raw; the projector discovers it via ledger_peers. The board-viewer
# widget for this host is started (once) by `button.sh run`. Its live
# cmdline is: bash .../board-viewer/button.sh run-widget <this PKG>.
# "up" = a board-viewer run-widget for this host AND a live prisc VM in
# an ON-DISK board-viewer session (a bash wrapper alone is not enough -
# see the orphan-reap above).
engine_up() {
    pgrep -f "piececraft-hq/button.sh run(-widget)? [^ ]*/$INST( |\$)" >/dev/null 2>&1 || return 1
    for _p in $(pgrep -f "prisc\+x pal/main_module" 2>/dev/null); do
        _c="$(readlink "/proc/$_p/cwd" 2>/dev/null)"
        case "$_c" in
            "$HOUSE_ROOT/&.widgits/board-viewer/pieces/sessions/"*) [ -d "$_c" ] && return 0 ;;
        esac
    done
    return 1
}
# One engine launch; its output is KEPT in $ENGLOG (until 2026-10-26 it went to /dev/null: a launch that died left the board window on "loading..." with nothing to read). Sets _eng to its pid.
start_engine() {
    if [ "$DATA" = "$PKG" ]; then setsid nohup env PCHQ_ENGINE_MODE=1 PCHQ_FOCUS_PROJECT_ROOT="$DATA" sh -c 'sh "$0" run "$1"' "$PKG/button.sh" "$DATA" >>"$ENGLOG" 2>&1 &
    else setsid nohup env PCHQ_ENGINE_MODE=1 PCHQ_PKG="$DATA" PCHQ_FOCUS_PROJECT_ROOT="$DATA" sh -c 'sh "$0" run "$1"' "$PKG/button.sh" "$DATA" >>"$ENGLOG" 2>&1 &
    fi
    _eng=$!
}
ENGLOG="/tmp/pchq-engine-$INST.log"
if ! engine_up; then
    # serialize concurrent launches (rapid double-click) through a
    # short-lived mkdir lock so we start exactly one engine.
    LOCK="$DATA/pieces/system/.engine-start.lock"
    mkdir -p "$DATA/pieces/system" 2>/dev/null || true
    if mkdir "$LOCK" 2>/dev/null; then
        trap 'rmdir "$LOCK" 2>/dev/null || true' EXIT INT TERM
        if ! engine_up; then
            echo "open_pchq_board: starting engine session (button.sh engine mode, detached)"
            # PCHQ_ENGINE_MODE=1: button.sh blocks on the orchestrator
            # instead of foregrounding a (tty-less, instantly-exiting)
            # keyboard_input reader - that early exit is what fired the
            # rm -rf trap and left the board window blank. Its teardown
            # trap still runs on a real SIGTERM (taskbar reap) / Ctrl-C.
            : > "$ENGLOG" 2>/dev/null || true
            start_engine
            # button.sh's own EXIT/INT/TERM trap does a clean rm -rf +
            # kill_own_* - nothing ever SIGTERM'd it under the old
            # taskbar launch, so it orphaned. Register its group in the
            # proc-ledger: a taskbar quit (ktb_reap_launched) now
            # kill(-pgid, SIGTERM)s it -> its trap fires -> clean.
            printf '%s\n' "$_eng" > "$DATA/pieces/system/engine.pid" 2>/dev/null || true
            printf '%s %s 0 0 pchq-engine\n' "$_eng" "$_eng" \
                >> "$HOUSE_ROOT/#.desktop/livedesk_proc_list.txt" 2>/dev/null || true
        fi
    fi
    # wait up to ~8s for the widget session to register
    i=0
    while [ "$i" -lt 40 ]; do
        engine_up && break
        sleep 0.2
        i=$((i + 1))
    done
    # SELF-HEAL (2026-10-26: a board sat on "loading..." for good because its engine session never came up and nothing said so): a slow start gets 12 s more, then ONE retry; every step is told on stderr (the app's board log) and the launch output is in $ENGLOG.
    if ! engine_up && [ -n "${_eng:-}" ]; then
        echo "open_pchq_board: engine not registered after 8 s (launch output: $ENGLOG); waiting 12 s more" >&2
        i=0; while [ "$i" -lt 60 ]; do engine_up && break; sleep 0.2; i=$((i + 1)); done
        if ! engine_up; then
            echo "open_pchq_board: still no engine after 20 s; ending that launch and retrying once" >&2
            kill "$_eng" 2>/dev/null || true; sleep 3
            start_engine; printf '%s\n' "$_eng" > "$DATA/pieces/system/engine.pid" 2>/dev/null || true
            i=0; while [ "$i" -lt 100 ]; do engine_up && break; sleep 0.2; i=$((i + 1)); done
            if engine_up; then echo "open_pchq_board: engine up after the retry" >&2; else echo "open_pchq_board: engine still not up after the retry; read $ENGLOG" >&2; fi
        fi
    fi
    rmdir "$LOCK" 2>/dev/null || true
    trap - EXIT INT TERM
fi

# ── launch the board window — standard x11-hq shape ─────────────────
if [ "$DATA" = "$PKG" ]; then
    setsid nohup "$BIN" "$HOUSE_ROOT" "$BOARD_TPL" piececraft-hq >/tmp/pchq-board.log 2>&1 < /dev/null &
    printf '%s %s 0 0 pchq-board\n' "$!" "$!" >> "$HOUSE_ROOT/#.desktop/livedesk_proc_list.txt" 2>/dev/null || true
else   # another instance: its window carries PCHQ_PKG in its environment (that is how same_instance() tells the windows apart)
    setsid nohup env PCHQ_PKG="$DATA" "$BIN" "$HOUSE_ROOT" "$BOARD_TPL" piececraft-hq >"/tmp/pchq-board-$INST.log" 2>&1 < /dev/null &
    printf '%s %s 0 0 pchq-board-%s\n' "$!" "$!" "$INST" >> "$HOUSE_ROOT/#.desktop/livedesk_proc_list.txt" 2>/dev/null || true
fi
disown 2>/dev/null || true

echo "open_pchq_board: board window launched"
