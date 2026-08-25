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
# above can never cover arbitrary toys/future launches. khtpm_taskbar_
# manager.c now records the PID of every real launch it makes (via the
# new ktb_system_recorded() helper) into #.desktop/livedesk_launched_
# pids.txt. Each recorded PID is a `setsid`-created process-GROUP LEADER
# (a real, pre-existing property of every launch site's own "setsid
# nohup ..." shape, not new) - killing the whole GROUP via `kill -TERM
# -$pid` (negative PID) reaches every real descendant a launch spawns
# (its own wrapper shell, button.sh, the final window binary), not just
# the one recorded PID. The fixed name-pattern list below is kept as a
# redundant safety net for anything already running before this registry
# existed - not the primary mechanism anymore.
set -u
HOUSE_ROOT="${1:-}"
REGISTRY=""
[ -n "$HOUSE_ROOT" ] && REGISTRY="$HOUSE_ROOT/#.desktop/livedesk_launched_pids.txt"

pat_list='
khtpm_hq_render\.\+x
khtpm_entity_menu_render\.\+x
khtpm_hq_manager\.\+x
khtpm_events_hq_manager\.\+x
khtpm_open_hai_manager\.\+x
'

named_pids="$(echo "$pat_list" | while IFS= read -r pat; do
    [ -z "$pat" ] && continue
    pgrep -f "$pat" 2>/dev/null
done | tr ' ' '\n' | grep -v '^$' | sort -u || true)"

reg_pids=""
if [ -n "$REGISTRY" ] && [ -f "$REGISTRY" ]; then
    reg_pids="$(tr -d ' \t' < "$REGISTRY" | grep -v '^$' | sort -u || true)"
fi

killed_any=0

if [ -n "$named_pids" ]; then
    echo "kill_hq_windows: killing (name-pattern, single PID): $(echo $named_pids | tr '\n' ' ')"
    echo "$named_pids" | xargs -r kill -TERM
    killed_any=1
fi

if [ -n "$reg_pids" ]; then
    echo "kill_hq_windows: killing (registry, whole process group): $(echo $reg_pids | tr '\n' ' ')"
    for pid in $reg_pids; do
        kill -0 "$pid" 2>/dev/null && kill -TERM "-$pid" 2>/dev/null
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
    for pid in $reg_pids; do
        kill -0 "$pid" 2>/dev/null && kill -KILL "-$pid" 2>/dev/null
    done
fi

# Prune the registry down to still-alive PIDs only, so it doesn't grow
# unboundedly across a long session (real, not hypothetical - this file
# is append-only at write time by design, see ktb_system_recorded()).
if [ -n "$REGISTRY" ] && [ -f "$REGISTRY" ]; then
    tmp="$REGISTRY.tmp.$$"
    : > "$tmp"
    for pid in $reg_pids; do
        kill -0 "$pid" 2>/dev/null && echo "$pid" >> "$tmp"
    done
    mv "$tmp" "$REGISTRY"
fi

echo "kill_hq_windows: done"
