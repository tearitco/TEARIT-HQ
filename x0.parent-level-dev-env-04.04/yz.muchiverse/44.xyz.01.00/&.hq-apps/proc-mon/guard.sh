#!/bin/sh
# guard.sh start|boot|stop|status - launcher for the always-on CPU guard (ops/cpu_guard.c, 2026-10-26).
# Design, thresholds and the freeze history: ops/cpu_guard.c header and BOOK 03-pitfalls/hq-cpu-safety.md section 8.
#
# Started at boot by a LAUNCH row in $.crypts/autostart.pdl. It is deliberately started as "./cpu_guard.+x <house>" from its own folder:
# `button.sh quit|reset` kills every process whose command line contains <house>/...&.hq-apps/.../+x/..., and the guard must SURVIVE
# those (the machine can freeze while the house is closed or restarting). Stop it only with `guard.sh stop`.
HERE="$(cd "$(dirname "$0")" && pwd)"
HOUSE="$(cd "$HERE/../.." && pwd)"
BIN_DIR="$HERE/ops/+x"
SRC="$HERE/ops/cpu_guard.c"
STATE="$HERE/state"

running() { pidof 'cpu_guard.+x' 2>/dev/null; }

case "${1:-status}" in
    start|boot)
        if [ -n "$(running)" ]; then echo "cpu_guard already running (pid $(running))"; exit 0; fi
        if [ ! -x "$BIN_DIR/cpu_guard.+x" ] || [ "$SRC" -nt "$BIN_DIR/cpu_guard.+x" ]; then
            mkdir -p "$BIN_DIR"
            nice -n 10 gcc -O2 -Wall -Wextra -Wno-format-truncation -o "$BIN_DIR/cpu_guard.+x" "$SRC" || { echo "cpu_guard: build failed" >&2; exit 1; }
        fi
        cd "$BIN_DIR" || exit 1
        setsid nohup ./cpu_guard.+x "$HOUSE" </dev/null >/dev/null 2>&1 &
        sleep 1
        if [ -n "$(running)" ]; then echo "cpu_guard started (pid $(running))"; else echo "cpu_guard did not start" >&2; exit 1; fi
        ;;
    stop)
        p="$(running)"
        [ -n "$p" ] && kill $p && echo "cpu_guard stopped ($p)" || echo "cpu_guard was not running"
        ;;
    status)
        p="$(running)"
        if [ -n "$p" ]; then echo "cpu_guard running (pid $p)"; else echo "cpu_guard NOT running"; fi
        [ -f "$STATE/cpu_guard_last.txt" ] && echo "last sample: $(cat "$STATE/cpu_guard_last.txt")"
        [ -f "$STATE/cpu_guard_alert.txt" ] && { echo "ALERT waiting for review:"; cat "$STATE/cpu_guard_alert.txt"; }
        [ -f "$STATE/cpu_guard.log" ] && { echo "--- log tail"; tail -5 "$STATE/cpu_guard.log" | cut -c1-200; }
        ;;
    *) echo "usage: guard.sh start|boot|stop|status" >&2; exit 2 ;;
esac
