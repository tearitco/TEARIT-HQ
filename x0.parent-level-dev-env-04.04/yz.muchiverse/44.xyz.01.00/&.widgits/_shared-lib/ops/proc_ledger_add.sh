#!/bin/sh
# proc_ledger_add.sh <house_root> <pid> <name> - register a process I launched in the proc ledger so the taskbar's quit/restart REAPS it.
# Appends the real 5-field line  <pid> <pgid> 0 <starttime> <name>  to <house_root>/#.desktop/livedesk_proc_list.txt.
# WHY the real starttime: the C reaper (kh_proc_reap_all, kh_proc_registry.h) re-reads field 22 of /proc/<pid>/stat and SKIPS a line whose NON-ZERO starttime differs (PID-reuse guard).
# A starttime of 0 means "unknown" and is NOT checked (tested 2026-10-06: a "<pid> <pid> 0 0 name" line is reaped), so the old hand-written lines did work for kill_all, but they
# give the reaper nothing to protect a recycled pid with. NOTE: this ledger is NOT why the hotbar survived a restart: `button.sh restart` does not use the reaper at all
# (see $.crypts/close_on_restart.pdl).
# pgid and starttime are read from /proc (Linux). If the process is already gone nothing is written. Exit 0 always (a launcher must not fail on this).
HOUSE_ROOT="${1:-}"; PID="${2:-}"; NAME="${3:-proc}"
[ -n "$HOUSE_ROOT" ] && [ -n "$PID" ] || exit 0
case "$PID" in *[!0-9]*) exit 0 ;; esac
[ -r "/proc/$PID/stat" ] || exit 0
# /proc/<pid>/stat: "pid (comm) state ppid pgrp ..." - comm may hold spaces/parens, so cut at the LAST ") ": then state=1 ppid=2 pgrp=3 ... starttime=20
FIELDS="$(sed 's/^.*) //' "/proc/$PID/stat" 2>/dev/null)"
PGID="$(echo "$FIELDS" | awk '{print $3}')"; START="$(echo "$FIELDS" | awk '{print $20}')"
case "$PGID$START" in ''|*[!0-9]*) exit 0 ;; esac
LEDGER="$HOUSE_ROOT/#.desktop/livedesk_proc_list.txt"
mkdir -p "$(dirname "$LEDGER")" 2>/dev/null
printf '%s %s 0 %s %s\n' "$PID" "$PGID" "$START" "$NAME" >> "$LEDGER" 2>/dev/null
exit 0
