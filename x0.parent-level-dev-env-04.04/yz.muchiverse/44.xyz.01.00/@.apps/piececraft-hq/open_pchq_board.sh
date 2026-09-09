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
# Usage: open_pchq_board.sh <house_root>
set -u

HOUSE_ROOT="${1:-}"
if [ -z "$HOUSE_ROOT" ] || [ ! -d "$HOUSE_ROOT" ]; then
    echo "open_pchq_board: need house_root as argv[1]" >&2
    exit 1
fi
HOUSE_ROOT="$(cd "$HOUSE_ROOT" && pwd)"
PKG="$(cd "$(dirname "$0")" && pwd)"

OPS_DIR="$HOUSE_ROOT/*.monads/*.livedesk-taskbar/ops"
BIN="$OPS_DIR/+x/khtpm_core_render.+x"
PROJECTOR="$PKG/ops/+x/pchq_board_projector.+x"
BOARD_TPL="$PKG/pchq-board.xhtpm"

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
engine_up() {
    pgrep -f "board-viewer/button.sh run-widget .*/piececraft-hq" >/dev/null 2>&1
}
if ! engine_up; then
    # serialize concurrent launches (rapid double-click) through a
    # short-lived mkdir lock so we start exactly one engine.
    LOCK="$PKG/pieces/system/.engine-start.lock"
    mkdir -p "$PKG/pieces/system" 2>/dev/null || true
    if mkdir "$LOCK" 2>/dev/null; then
        trap 'rmdir "$LOCK" 2>/dev/null || true' EXIT INT TERM
        if ! engine_up; then
            echo "open_pchq_board: starting engine session (button.sh run, detached)"
            setsid nohup sh -c 'sh "$0" run' "$PKG/button.sh" >/dev/null 2>&1 &
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
