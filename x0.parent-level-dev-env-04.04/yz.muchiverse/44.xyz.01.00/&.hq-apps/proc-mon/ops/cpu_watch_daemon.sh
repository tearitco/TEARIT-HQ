#!/bin/sh
# cpu_watch_daemon.sh - real, ALWAYS-RUNNING background CPU-spike
# watcher for proc-mon (2026-09-28, direct instruction: "id like it to
# be toggleable. and lets have it on always for now, and check it
# later to see if it caught anything since throttling seems to be
# spiking every 2 minutes for about a minute").
#
# Unlike mon_refresh.sh (proc-mon's own <module>, which only runs
# while the proc-mon WINDOW is open - see bug_bounty.md 2026-09-28's
# "if there is some feature..." thread for why that's not enough to
# catch a spike nobody was watching for), this is a standalone daemon
# meant to run continuously, same shape as world_manager.pal - started
# once, independent of any window being open. proc-mon's own window
# just DISPLAYS what this already caught; it doesn't do the catching.
#
# Real fix for the exact blind spot that let world_manager_tick.c's
# bug hide from `top`/`ps` (BUG-LOG.md / bug_bounty.md 2026-09-28):
# samples a matched process's own utime+stime+cutime+cstime delta over
# a real wall-clock window, not `ps`'s own `pcpu` (a decayed lifetime
# average of ONLY that PID's own time, blind to reaped children's
# cutime/cstime - exactly what hid the world_manager bug). A forked-
# and-reaped child's CPU cost shows up automatically in the PARENT's
# own cutime/cstime the moment it's reaped, so watching just the
# matched top-level PIDs (no separate child-walking needed) already
# catches the same bug class world_manager had.
#
# Toggle: #.desktop/proc_mon_cpu_watch.pdl, "enabled=1"/"enabled=0",
# checked once per sample cycle (self-healing - flip it and the next
# cycle picks it up, no restart needed). Created with enabled=1 the
# first time this daemon runs if it doesn't already exist.
#
# Usage: cpu_watch_daemon.sh <house_root>
set -u

HOUSE_ROOT="${1:-}"
[ -n "$HOUSE_ROOT" ] && [ -d "$HOUSE_ROOT" ] || { echo "cpu_watch_daemon: need house_root as argv[1]" >&2; exit 1; }

SAMPLE_SEC=5
THRESHOLD_PCT=30
TOGGLE="$HOUSE_ROOT/#.desktop/proc_mon_cpu_watch.pdl"
LOG_DIR="$HOUSE_ROOT/&.hq-apps/proc-mon/state"
LOG="$LOG_DIR/cpu_spikes.log"
STATE_DIR="$LOG_DIR/.cpu_watch_prev"
mkdir -p "$LOG_DIR" "$STATE_DIR"
[ -f "$TOGGLE" ] || echo "enabled=1" > "$TOGGLE"
[ -f "$LOG" ] || : > "$LOG"

# REAL, NEW - single-instance guard (direct live check found this
# daemon's own launch had none, ironic given this whole feature exists
# to catch stray/duplicate house processes - k9 doc's own "confirm
# zero stray processes before launching" rule applies to this file
# too). PID-file based, self-healing: a stale file (process no longer
# alive) is overwritten, not trusted blindly.
PIDFILE="$LOG_DIR/.cpu_watch_daemon.pid"
if [ -f "$PIDFILE" ]; then
    OLD_PID=$(cat "$PIDFILE" 2>/dev/null)
    if [ -n "$OLD_PID" ] && kill -0 "$OLD_PID" 2>/dev/null; then
        echo "cpu_watch_daemon: already running as pid $OLD_PID - exiting" >&2
        exit 0
    fi
fi
echo "$$" > "$PIDFILE"
trap 'rm -f "$PIDFILE"' EXIT

# Same real house-process patterns proc-mon's own mon_scan.sh already
# uses (duplicated, not sourced - this is a standalone daemon, and
# mon_scan.sh's PATTERNS aren't exposed as an importable chunk without
# side effects; matches this house's own per-project-duplication
# convention for this exact reason).
PATTERNS_RAW='prisc\+x(\.\+x)? .*\.pal|khtpm_core_render\.\+x|khtpm_entity\.\+x|khtpm_taskbar_manager_main|_hq_manager\.\+x|khtpm_hq_manager\.\+x|swatch_picker_manager|bv_render_3d|bv_gpu_raymarch|pchq_board_projector|chtpm_parser_pal'
# REAL FIX (found live, first test run) - same real ERE-sanitization
# mon_scan.sh's own awk_ere() does and for the exact same reason ("so
# mawk/gawk don't warn"): raw \+ / \. in an ERE means "literal char",
# which not every awk flavor honors the same way - convert to bracket
# classes first. Without this, world_manager.pal itself (the actual
# bug this daemon exists to catch) was silently NOT matched at all -
# confirmed live, its pid never appeared in the tracked state dir.
PATTERNS=$(printf '%s' "$PATTERNS_RAW" | sed 's/\\+/[+]/g; s/\\\./[.]/g')

hz=$(getconf CLK_TCK)

is_enabled() {
    [ -f "$TOGGLE" ] || return 0
    grep -q '^enabled=1' "$TOGGLE" 2>/dev/null
}

trap 'exit 0' TERM INT HUP

while :; do
    if ! is_enabled; then
        sleep "$SAMPLE_SEC"
        continue
    fi

    PS_SNAP=$(ps -eo pid,args 2>/dev/null)
    printf '%s\n' "$PS_SNAP" | awk -v pat="$PATTERNS" 'NR>1 {
        args=""; for(i=2;i<=NF;i++) args=args (i>2?" ":"") $i
        if (args ~ pat) print $1"|"args
    }' | while IFS='|' read -r pid args; do
        [ -z "$pid" ] && continue
        stat="/proc/$pid/stat"
        [ -r "$stat" ] || continue
        # fields 14-17: utime stime cutime cstime (real, cutime/cstime
        # inclusive - the whole point, see header comment)
        fields=$(awk '{print $14,$15,$16,$17}' "$stat" 2>/dev/null)
        [ -z "$fields" ] && continue
        set -- $fields
        u=$1; s=$2; cu=$3; cs=$4
        total=$((u + s + cu + cs))
        prevfile="$STATE_DIR/$pid"
        if [ -f "$prevfile" ]; then
            read -r prev_total prev_time < "$prevfile"
            now_time=$(date +%s)
            dt=$((now_time - prev_time))
            [ "$dt" -le 0 ] && dt=1
            delta=$((total - prev_total))
            pct=$(awk -v d="$delta" -v hz="$hz" -v dt="$dt" 'BEGIN{printf "%.1f", (d/hz)/dt*100}')
            over=$(awk -v p="$pct" -v t="$THRESHOLD_PCT" 'BEGIN{print (p+0 > t) ? 1 : 0}')
            if [ "$over" = 1 ]; then
                short=$(printf '%s' "$args" | sed -E "s#$HOUSE_ROOT/##g" | cut -c1-100)
                echo "$(date '+%Y-%m-%d %H:%M:%S') | pid=$pid | ${pct}% | $short" >> "$LOG"
            fi
        fi
        echo "$total $(date +%s)" > "$prevfile"
    done

    # prune stale per-pid state for processes that exited
    for f in "$STATE_DIR"/*; do
        [ -f "$f" ] || continue
        p=$(basename "$f")
        kill -0 "$p" 2>/dev/null || rm -f "$f"
    done

    sleep "$SAMPLE_SEC"
done
