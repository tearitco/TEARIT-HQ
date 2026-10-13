#!/bin/bash
# button.sh - the TPMOS x11-hq toy (@.apps/tpmos-hq/toy.pdl). Usage: sh button.sh [run|start|stop|status]
#   run    (detached) start the REAL TPMOS (its own button.sh run, under a pseudo-terminal) if it is not already running, then open the x11-hq window that reads its frames and relays keys
#   start  only start TPMOS      stop  stop the TPMOS processes this toy started      status  say what is running
# Which TPMOS folder: display.pdl (tpmos_root). TPMOS's run_orchestrator.sh begins with `pkill -9 -f` patterns that match ANY process whose command line holds
# pieces/<..>/+x/<..>.+x or projects/<..>/+x/<..>.+x (this house's board sessions do), so start refuses, and names the pids, when such a process exists
# (override with TPMOS_HQ_FORCE=1). Stop never uses pkill: it signals only processes whose working directory IS the TPMOS folder.
set -u
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
HOUSE_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
BIN="$HOUSE_ROOT/_.monads/_.livedesk-taskbar/ops/+x/khtpm_core_render.+x"
XHTPM="$SCRIPT_DIR/tpmos-hq.xhtpm"
ACTION="${1:-run}"
mkdir -p "$SCRIPT_DIR/state" "$SCRIPT_DIR/audit"
rel="$(awk -F'|' '$1 ~ /DISPLAY/ && $2 ~ /tpmos_root/ {gsub(/^ +| +$/,"",$3); print $3}' "$SCRIPT_DIR/display.pdl")"
case "$rel" in /*) TPMOS="$rel" ;; *) TPMOS="$HOUSE_ROOT/$rel" ;; esac
TPMOS="$(cd "$TPMOS" 2>/dev/null && pwd)" || { echo "tpmos-hq: tpmos_root in display.pdl does not exist"; exit 1; }
TPMOS_COMMS="script orchestrator.+x keyboard_input. chtpm_parser.+x renderer.+x gl_renderer.+x joystick_input."

tpmos_pids() {      # TPMOS's own processes: the pty wrapper / known comms with cwd == the TPMOS folder, plus anything whose executable lives under the TPMOS folder (its project modules)
    ps -eo pid=,comm= | while read -r pid comm; do
        exe="$(readlink "/proc/$pid/exe" 2>/dev/null)"
        case "$exe" in "$TPMOS"/*) echo "$pid"; continue ;; esac
        case " $TPMOS_COMMS " in *" $comm "*) [ "$(readlink "/proc/$pid/cwd" 2>/dev/null)" = "$TPMOS" ] && echo "$pid" ;; esac
    done
}
tpmos_running() { ps -eo pid=,comm= | while read -r pid comm; do [ "$comm" = "orchestrator.+x" ] && [ "$(readlink "/proc/$pid/cwd" 2>/dev/null)" = "$TPMOS" ] && echo "$pid"; done | grep -q .; }

start_tpmos() {
    if tpmos_running; then echo "TPMOS already running"; return 0; fi
    if [ ! -x "$TPMOS/pieces/chtpm/plugins/+x/orchestrator.+x" ]; then
        echo "TPMOS is not compiled: run  bash '$TPMOS/button.sh' compile  first"; return 1
    fi
    command -v script >/dev/null 2>&1 || { echo "tpmos-hq: needs 'script' (util-linux) to give TPMOS a terminal"; return 1; }
    mine=" $(tpmos_pids | tr '\n' ' ') "      # TPMOS's own leftovers are not a reason to refuse: starting TPMOS clears them anyway
    hit=""
    for p in $(ps -eo pid=,args= | grep -E 'pieces/.*/[+]x/.*[.][+]x|projects/.*/[+]x/.*[.][+]x|pal_watchdog[.]sh' | grep -v grep | awk '{print $1}'); do
        case "$mine" in *" $p "*) ;; *) hit="$hit$p " ;; esac
    done
    if [ -n "$hit" ] && [ "${TPMOS_HQ_FORCE:-0}" != 1 ]; then
        echo "tpmos-hq: NOT starting TPMOS: its start script would pkill -9 these running processes: $hit (set TPMOS_HQ_FORCE=1 to start anyway)"; return 1
    fi
    ( cd "$TPMOS" && setsid nohup nice -n 10 script -qfc "bash ./button.sh run" /dev/null > "$SCRIPT_DIR/audit/tpmos.log" 2>&1 < /dev/null & )
    for _ in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15; do sleep 1; tpmos_running && { echo "TPMOS started"; return 0; }; done
    echo "tpmos-hq: TPMOS did not come up (see audit/tpmos.log)"; return 1
}
stop_tpmos() {
    p="$(tpmos_pids | tr '\n' ' ')"
    if [ -z "$p" ]; then echo "TPMOS not running"; return 0; fi
    kill -TERM $p 2>/dev/null; sleep 2; echo "TPMOS stopped (pids $p)"
}
open_window() {
    [ -x "$SCRIPT_DIR/ops/+x/tpmos_hq.+x" ] || sh "$SCRIPT_DIR/ops/build_tpmos_hq.sh" >/dev/null 2>&1 || true
    "$SCRIPT_DIR/ops/+x/tpmos_hq.+x" status >/dev/null      # ui.txt must exist before the window loads
    old="$(ps -eo pid,comm,args | awk '$2=="khtpm_core_rend" && index($0,"tpmos-hq.xhtpm"){print $1}')"
    [ -n "$old" ] && echo "$old" | xargs -r kill -TERM && sleep 1
    setsid nohup "$BIN" "$HOUSE_ROOT" "$XHTPM" >"$SCRIPT_DIR/audit/tpmos-hq.log" 2>&1 < /dev/null &
    printf '%s %s 0 0 tpmos-hq\n' "$!" "$!" >> "$HOUSE_ROOT/#.desktop/livedesk_proc_list.txt" 2>/dev/null || true
    disown 2>/dev/null || true
    sleep 2; echo "tpmos-hq window launched"
}
case "$ACTION" in
    run)    # detached: the menu that launched this returns at once; _run does the slow part (TPMOS takes a few seconds to come up)
            setsid nohup sh "$0" _run > "$SCRIPT_DIR/audit/run.log" 2>&1 < /dev/null &
            echo "tpmos-hq starting" ;;
    _run)   start_tpmos; open_window ;;
    start)  start_tpmos ;;
    stop)   stop_tpmos ;;
    status) if tpmos_running; then echo "TPMOS running (pids: $(tpmos_pids | tr '\n' ' '))"; else echo "TPMOS not running"; fi ;;
    *)      echo "usage: sh button.sh run|start|stop|status" ;;
esac
