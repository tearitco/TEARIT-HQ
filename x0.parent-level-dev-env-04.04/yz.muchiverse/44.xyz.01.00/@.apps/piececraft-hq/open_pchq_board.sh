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

OPS_DIR="$HOUSE_ROOT/*.monads/*.livedesk-taskbar/ops"
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
if [ ! -f "$BOARD_TPL" ]; then
    echo "open_pchq_board: missing $BOARD_TPL" >&2
    exit 1
fi

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
SESS_BASES="$PKG/pieces/sessions
$HOUSE_ROOT/&.widgits/board-viewer/pieces/sessions"
_live_sessions=""
for _p in $(pgrep -f "keyboard_input|system/orchestrator|board-viewer/button.sh run-widget" 2>/dev/null); do
    _c="$(readlink "/proc/$_p/cwd" 2>/dev/null)"
    case "$_c" in *"/pieces/sessions/"*) _live_sessions="$_live_sessions $(basename "$_c")" ;; esac
done
for _p in $(pgrep -f "chtpm_parser_pal|system/renderer|prisc\+x|system/orchestrator" 2>/dev/null); do
    _c="$(readlink "/proc/$_p/cwd" 2>/dev/null)" || continue
    case "$_c" in
        "$PKG/pieces/sessions/"*|"$HOUSE_ROOT/&.widgits/board-viewer/pieces/sessions/"*) : ;;
        *) continue ;;
    esac
    _b="$(basename "$_c")"
    case " $_live_sessions " in *" $_b "*) continue ;; esac   # part of a live session
    _pp="$(awk '{print $4}' "/proc/$_p/stat" 2>/dev/null)"
    [ "$_pp" = "1" ] || [ "$_pp" = "1003" ] || continue        # orphaned only
    kill -KILL "$_p" 2>/dev/null || true
done
# drop session dirs with nothing live in them
echo "$SESS_BASES" | while IFS= read -r _base; do
    [ -d "$_base" ] || continue
    for _d in "$_base"/*/; do
        [ -d "$_d" ] || continue
        case " $_live_sessions " in *" $(basename "$_d") "*) continue ;; esac
        rm -rf "$_d" 2>/dev/null || true
    done
done

# Reap an orphaned `bash board-viewer/button.sh run-widget <this pkg>`
# whose session dir is gone (a hard-killed test / strip restart leaves
# the bash wrapper alive; ledger_peers still reports it ONLINE because
# its PID lives, so the projector latches a deleted bv_session -> blank
# canvas). If no live prisc VM has a cwd under an existing board-viewer
# session dir, the widget is dead weight - kill it.
for _w in $(pgrep -f "board-viewer/button.sh run-widget .*/piececraft-hq" 2>/dev/null); do
    _ok=0
    for _p in $(pgrep -f "prisc\+x pal/main_module" 2>/dev/null); do
        _c="$(readlink "/proc/$_p/cwd" 2>/dev/null)"
        case "$_c" in
            "$HOUSE_ROOT/&.widgits/board-viewer/pieces/sessions/"*) [ -d "$_c" ] && _ok=1 ;;
        esac
    done
    [ "$_ok" = 1 ] || { echo "open_pchq_board: reaping orphan board-viewer widget $_w (no live session)"; kill -KILL "$_w" 2>/dev/null || true; }
done

# ── single-instance guard: clean-restart the board window ────────────
# Match the shared binary by its OWN chtpm path (bare-name match would
# hit every other khtpm_core_render window) + the projector.
board_pids() { pgrep -f "khtpm_core_render\.\+x .*pchq-board\.xhtpm" 2>/dev/null || true; }
proj_pids()  { pgrep -f "pchq_board_projector\.\+x" 2>/dev/null || true; }

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
    pgrep -f "board-viewer/button.sh run-widget .*/piececraft-hq" >/dev/null 2>&1 || return 1
    for _p in $(pgrep -f "prisc\+x pal/main_module" 2>/dev/null); do
        _c="$(readlink "/proc/$_p/cwd" 2>/dev/null)"
        case "$_c" in
            "$HOUSE_ROOT/&.widgits/board-viewer/pieces/sessions/"*) [ -d "$_c" ] && return 0 ;;
        esac
    done
    return 1
}
if ! engine_up; then
    # serialize concurrent launches (rapid double-click) through a
    # short-lived mkdir lock so we start exactly one engine.
    LOCK="$PKG/pieces/system/.engine-start.lock"
    mkdir -p "$PKG/pieces/system" 2>/dev/null || true
    if mkdir "$LOCK" 2>/dev/null; then
        trap 'rmdir "$LOCK" 2>/dev/null || true' EXIT INT TERM
        if ! engine_up; then
            echo "open_pchq_board: starting engine session (button.sh engine mode, detached)"
            # PCHQ_ENGINE_MODE=1: button.sh blocks on the orchestrator
            # instead of foregrounding a (tty-less, instantly-exiting)
            # keyboard_input reader - that early exit is what fired the
            # rm -rf trap and left the board window blank. Its teardown
            # trap still runs on a real SIGTERM (taskbar reap) / Ctrl-C.
            setsid nohup env PCHQ_ENGINE_MODE=1 sh -c 'sh "$0" run' "$PKG/button.sh" >/dev/null 2>&1 &
            _eng=$!
            # button.sh's own EXIT/INT/TERM trap does a clean rm -rf +
            # kill_own_* - nothing ever SIGTERM'd it under the old
            # taskbar launch, so it orphaned. Register its group in the
            # proc-ledger: a taskbar quit (ktb_reap_launched) now
            # kill(-pgid, SIGTERM)s it -> its trap fires -> clean.
            printf '%s\n' "$_eng" > "$PKG/pieces/system/engine.pid" 2>/dev/null || true
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
    rmdir "$LOCK" 2>/dev/null || true
    trap - EXIT INT TERM
fi

# ── launch the board window — standard x11-hq shape ─────────────────
setsid nohup "$BIN" "$HOUSE_ROOT" "$BOARD_TPL" piececraft-hq \
    >/tmp/pchq-board.log 2>&1 < /dev/null &
printf '%s %s 0 0 pchq-board\n' "$!" "$!" \
    >> "$HOUSE_ROOT/#.desktop/livedesk_proc_list.txt" 2>/dev/null || true
disown 2>/dev/null || true

echo "open_pchq_board: board window launched"
