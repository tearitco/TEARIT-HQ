#!/bin/sh
# kh_spawn.sh <house_root> <master_pid> <name> -- <cmd> [args...]
#
# The shell-side of the kh_spawn funnel
# (PROC-LIFECYCLE-ORCHESTRATOR-TEARDOWN.md): start a detached, long-lived
# house child and record it in the master-ledger so the taskbar's quit
# reaper reaches it. Use this instead of a bare `setsid nohup ... &`.
#
#   master_pid : the owning process (e.g. $$ of the launcher, or the
#                app's own root pid). 0 = "orchestrator-owned"
#                (reaped only by a house-wide quit, not a per-app close).
#   name       : ledger label.
#
# The recorded line is  <pid> <pid> <master_pid> 0 <name>  — starttime 0
# means "liveness-only guard" (the C funnel records a real /proc
# start-time; the shell path can't cheaply, and 0 is handled).
set -u
house="$1"; master="$2"; name="$3"; shift 3
[ "${1:-}" = "--" ] && shift
[ -n "${1:-}" ] || { echo "kh_spawn.sh: no command" >&2; exit 2; }

setsid nohup "$@" >/dev/null 2>&1 &
pid=$!
printf '%s %s %s 0 %s\n' "$pid" "$pid" "$master" "$name" \
    >> "$house/#.desktop/livedesk_proc_list.txt"
echo "$pid"
