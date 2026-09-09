#!/bin/sh
# kill_hq_windows.sh — emergency kill-switch for stuck "-hq" window apps
# (db-hq, events-hq, chat-hai, stats-hq, palettes, entity-menu, taskbar-
# settings), reachable from the taskbar's own HQ menu (direct request
# 2026-08-25, prompted by a real stuck stats-hq window this session that
# had no working close button - au11-hq/TPMOS-COMPLIANCE-DEBT.md).
#
# Deliberately does NOT touch the taskbar itself
# (khtpm_strip_parser.+x / khtpm_taskbar_manager_main.+x) - those are the
# desktop shell, not an "-hq window"; killing them would take down the
# whole taskbar, not just a stuck popup. Scope is every OTHER real
# process this family can spawn: both renderer binaries that host -hq
# window modes, plus their real manager processes (khtpm_hq_manager.+x,
# khtpm_events_hq_manager.+x, khtpm_open_hai_manager.+x - see
# TPMOS-COMPLIANCE-DEBT.md for which of these are the real, compliant
# manager pattern).
#
# Same graceful-TERM-then-KILL escalation open_db_hq.sh/open_stats_hq.sh
# already use for their own single-instance guards - not reinvented.
#
# Usage: kill_hq_windows.sh <house_root>  (real, required now - see below,
# not the "currently unused" placeholder this file started with).
#
# REAL FIX 2026-08-25 #1 (direct live report: "it(kill) doesn't work yet") -
# this script was originally written with a bash array (`pats=(...)`,
# `"${pats[@]}"`), but every hq_menu_N_cmd row in this house's own
# livedesk_taskbar.pdl is dispatched via plain `sh <path>` (see
# run_khtpm_strip.sh/open_cli.sh's own identical `sh ...` invocation
# convention), and `/bin/sh` on this system is dash, not bash - dash has
# no array support at all (`Syntax error: "(" unexpected`, confirmed by
# direct reproduction: `sh kill_hq_windows.sh` failed instantly). The
# generic dispatch path also redirects stderr to /dev/null
# (`setsid nohup sh -c '<cmd>' >/dev/null 2>&1 &`), so this failure was
# completely silent from the menu - only found by running the script by
# hand. Real fix: plain POSIX sh throughout, no arrays - a newline-
# separated pattern LIST instead, walked with a portable `while read`
# loop.
#
# REAL FIX 2026-08-25 #2 (direct live report: "it worked on db-hq but not
# on toys: mutaclysm-neo... isn't there a way for making it work for all
# launched thru tb? pid record in pdl?"): the fixed name-pattern list
# above can never cover arbitrary toys/future launches. Every real launch
# this house makes is recorded in the proc-ledger
# #.desktop/livedesk_proc_list.txt  (5-field `pid pgid master starttime
# name`, or a legacy 4-field `pid pgid starttime name` line - field 1 is
# always the PID, field 2 always its process-GROUP id). Killing the whole
# GROUP via `kill -TERM -$pgid` reaches every real descendant a launch
# spawns (its own wrapper shell, button.sh, the final window binary), not
# just the leader. The fixed name-pattern list below is kept as a
# redundant safety net.
#
# DROP 2026-09-09 (PROC-LIFECYCLE-CONSOLIDATE-REGISTRIES.md §2): this
# script used to read the now-removed bare-PID file
# #.desktop/livedesk_launched_pids.txt. It reads the one proc-ledger
# now. The C reaper (kh_proc_reap_all) owns the /proc-starttime PID-reuse
# guard; this emergency net stays deliberately simple (kill -0 liveness).
set -u
HOUSE_ROOT="${1:-}"
REGISTRY=""
[ -n "$HOUSE_ROOT" ] && REGISTRY="$HOUSE_ROOT/#.desktop/livedesk_proc_list.txt"

pat_list='
khtpm_hq_render\.\+x
khtpm_core_render\.\+x
khtpm_hq_manager\.\+x
khtpm_events_hq_manager\.\+x
khtpm_open_hai_manager\.\+x
'

named_pids="$(echo "$pat_list" | while IFS= read -r pat; do
    [ -z "$pat" ] && continue
    pgrep -f "$pat" 2>/dev/null
done | tr ' ' '\n' | grep -v '^$' | sort -u || true)"

# Parse the proc-ledger: field 1 = PID, field 2 = process-GROUP id.
# reg_pids  = whitespace list of live PIDs (for liveness/escalation)
# reg_pgids = whitespace list of their group ids (for `kill -TERM -$pgid`)
reg_pids=""
reg_pgids=""
if [ -n "$REGISTRY" ] && [ -f "$REGISTRY" ]; then
    while read -r f1 f2 _rest; do
        case "$f1" in ''|\#*) continue ;; esac
        case "$f1" in *[!0-9]*) continue ;; esac
        [ -z "$f2" ] && f2="$f1"
        case "$f2" in *[!0-9]*) f2="$f1" ;; esac
        kill -0 "$f1" 2>/dev/null || continue
        reg_pids="$reg_pids $f1"
        reg_pgids="$reg_pgids $f2"
    done < "$REGISTRY"
fi

killed_any=0

if [ -n "$named_pids" ]; then
    echo "kill_hq_windows: killing (name-pattern, single PID): $(echo $named_pids | tr '\n' ' ')"
    echo "$named_pids" | xargs -r kill -TERM
    killed_any=1
fi

if [ -n "$reg_pgids" ]; then
    echo "kill_hq_windows: killing (ledger, whole process group):$reg_pgids"
    for pgid in $reg_pgids; do
        kill -TERM "-$pgid" 2>/dev/null || true
    done
    for pid in $reg_pids; do
        kill -TERM "$pid" 2>/dev/null || true
    done
    killed_any=1
fi

if [ "$killed_any" = "0" ]; then
    echo "kill_hq_windows: nothing to kill"
else
    sleep 1
    still_named=""
    for pid in $named_pids; do kill -0 "$pid" 2>/dev/null && still_named="$still_named $pid"; done
    if [ -n "$still_named" ]; then
        echo "kill_hq_windows: still alive after TERM, escalating to KILL:$still_named"
        for pid in $still_named; do kill -KILL "$pid" 2>/dev/null; done
    fi
    for pgid in $reg_pgids; do
        kill -KILL "-$pgid" 2>/dev/null || true
    done
    for pid in $reg_pids; do
        kill -0 "$pid" 2>/dev/null && kill -KILL "$pid" 2>/dev/null
    done
fi

# NOTE: the proc-ledger is NOT pruned here. Unlike the old bare-PID file
# (which had no owner), khtpm_taskbar_manager owns livedesk_proc_list.txt
# — it prunes on init (kh_proc_registry_prune) and rewrites on quit
# (kh_proc_reap_*). Pruning it from this emergency script would race the
# manager and could truncate the extra ledger columns.

echo "kill_hq_windows: done"
