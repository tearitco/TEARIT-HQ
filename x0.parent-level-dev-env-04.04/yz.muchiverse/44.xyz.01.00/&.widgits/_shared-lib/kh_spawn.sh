#!/bin/sh
# kh_spawn.sh <house_root> <owner_pid> <name> -- <cmd> [args...]
#
# The shell-side funnel: start a detached, long-lived house process and REGISTER IT in the process registry (#.desktop/proc_registry.pdl, ops/proc_reg_op.c)
# BEFORE it does any work. Use this instead of a bare `setsid nohup X &` (AGENTS.md: every process the house starts is registered in a file).
#
#   owner_pid : the process whose death ends this one (the app's root pid, the window's pid, $$ of a launcher). 0 = no owner: the row is `keep`
#               (survives its owner on purpose; ended by a house quit or an explicit reap). The default lifetime is `owner`.
#   name      : the registry label.
#   env       : KH_ROLE (daemon|manager|projector|engine|clock|worker|sound|tool, default daemon), KH_APP (default -), KH_LIFETIME (owner|keep|house to override).
#
# How: `setsid nohup sh -c 'register $$; exec "$@"'`. The shell registers ITS OWN pid, then exec's the command, and exec keeps the pid, so the row names the real
# process (not a wrapper) and there is no window between "started" and "registered" for a parent to die in. The op is the whole implementation
# (ops/proc_reg_op.c): there is no header and nothing is linked. If the op is not built the old v1 ledger line is written instead (a launcher must not fail on this).
# Prints the pid. Plan: PROC-REGISTRY-STANDARD-AND-REFACTOR-PLAN-2026-11-02.md
set -u
house="$1"; owner="$2"; name="$3"; shift 3
[ "${1:-}" = "--" ] && shift
[ -n "${1:-}" ] || { echo "kh_spawn.sh: no command" >&2; exit 2; }
HERE="$(cd "$(dirname "$0")" && pwd)"
REG="$HERE/ops/+x/proc_reg_op.+x"
case "$owner" in ''|*[!0-9]*) owner=0 ;; esac
if [ -n "${KH_LIFETIME:-}" ]; then life="$KH_LIFETIME"; elif [ "$owner" -gt 0 ]; then life=owner; else life=keep; fi

if [ -x "$REG" ]; then
    setsid nohup sh -c 'reg=$1; h=$2; lt=$3; ow=$4; ro=$5; ap=$6; nm=$7; shift 7
        "$reg" "$h" register $$ "$lt" "$ow" "$ro" "$ap" "$nm" "$*" >/dev/null 2>&1
        exec "$@"' kh_spawn "$REG" "$house" "$life" "$owner" "${KH_ROLE:-daemon}" "${KH_APP:--}" "$name" "$@" >/dev/null 2>&1 &
    pid=$!
else
    setsid nohup "$@" >/dev/null 2>&1 &
    pid=$!
    printf '%s %s %s 0 %s\n' "$pid" "$pid" "$owner" "$name" >> "$house/#.desktop/livedesk_proc_list.txt"
fi
echo "$pid"
