#!/bin/bash
# open_db_hq.sh — launch db-hq (the CSS-styled database window) as its own
# detached X11 process, same launch shape as open_event_ez.sh.
# Usage: open_db_hq.sh <house_root>
#
# REAL FIX 2026-08-13: this used to launch unconditionally, no matter
# how many db-hq instances were already running. The SAME live bug was
# found+fixed first in open-hai's button.sh (five concurrent processes
# all racing on shared state, root cause + full writeup in
# _.0.aigent-testing-k9.txt "SCOPE ADDENDUM 2026-08-13") - applying the
# identical proven fix here rather than waiting to hit it live in
# db-hq too. db-hq is single-instance-per-house (one dashboard window
# at a time is the intended UX, matching open-hai/taskbar), so a
# blanket kill-before-launch by binary name is correct here - unlike
# events-hq, which is legitimately multi-instance (one per entity).
#
# REAL FIX 2026-08-16 (Stage 2d shell/manager split, au11-hq/khtpm-
# merge-how2.md + local-2do-15.txt's own "Stage 2d, REDONE correctly"
# entry): db-hq is now TWO cooperating processes, not one - the shell
# (khtpm_hq_render.c, draws the window) and a separate manager
# (khtpm_hq_manager.c, owns common_events/ scanning + the "open in
# editor" spawn action, talking to the shell only through
# #.desktop/db_hq_common_events.state.txt / db_hq_action.txt).
#
# REAL FIX 2026-08-16, same day, correction ("explain to me your plan
# and why its different from the tpmos/wraith examples"): this used to
# launch the manager directly from THIS script - not the real mechanism.
# dashboard.chtpm now has a real <module src="..."/> tag, and
# khtpm_hq_render.c's own main() reads it and fork()+execv()s the
# manager itself (see that file's launch_module(), ported verbatim from
# wraith_parser_alpha.c's own launch_module()) - this script now only
# launches the SHELL. The kill-before-relaunch guard below still covers
# the manager's binary name too, purely as a safety net for an orphaned
# manager from a crash/older build - not the normal launch path anymore.
set -e
HOUSE_ROOT="${1:-}"
if [ -z "$HOUSE_ROOT" ] || [ ! -d "$HOUSE_ROOT" ]; then
    echo "open_db_hq: need house_root as argv[1]" >&2
    exit 1
fi
HOUSE_ROOT="$(cd "$HOUSE_ROOT" && pwd)"

# REAL Stage 5 §5d.10 (2026-08-16, khtpm-merge-how2.md §5d) - the real,
# literal binary merge: db-hq now runs through the SAME compiled
# khtpm_entity_menu_render.+x entity-menu/taskbar-settings already use,
# mode-selected by `<window class="db-hq">` in dashboard.chtpm -
# genuinely one binary, not three, verified live before this launcher
# was retargeted. khtpm_hq_render.c/build_db_hq.sh are kept as real,
# working reference/rollback, just no longer what this launcher points
# at. The manager (khtpm_hq_manager.+x) is UNCHANGED - still a separate
# real process, still launched by the shell itself via the <module>
# tag's own real fork()+execl() (dbhq_launch_module() in
# khtpm_entity_menu_render.c, ported verbatim from the original).
OPS_DIR="$HOUSE_ROOT/*.monads/*.livedesk-taskbar/ops"
BIN="$OPS_DIR/+x/khtpm_entity_menu_render.+x"
MGR_BIN="$OPS_DIR/+x/khtpm_hq_manager.+x"
CHTPM="$HOUSE_ROOT/&.hq-apps/db-hq/dashboard.chtpm"

if [ ! -x "$BIN" ]; then
    (cd "$OPS_DIR" && sh build_entity_menu.sh) || true
fi
if [ ! -x "$BIN" ]; then
    echo "open_db_hq: build failed, missing $BIN" >&2
    exit 1
fi
if [ ! -x "$MGR_BIN" ]; then
    (cd "$OPS_DIR" && sh build_db_hq_manager.sh) || true
fi
if [ ! -x "$MGR_BIN" ]; then
    echo "open_db_hq: manager build failed, missing $MGR_BIN" >&2
    exit 1
fi

mkdir -p "$HOUSE_ROOT/common_events"

# pgrep exits 1 (nonzero) when nothing matches - guarded with `|| true`
# everywhere under `set -e` (a bare unguarded assignment from a failing
# command substitution silently aborts the whole script - lost real
# time to this exact bug fixing open-hai's button.sh first).
# REAL, deliberately specific - matches by the real chtpm PATH too, not
# just the binary name, since khtpm_entity_menu_render.+x is a real,
# genuinely shared binary now (same real precedent as button_taskbar_
# settings.sh's own settings_pids()) - a bare binary-name match here
# would incorrectly kill/confuse itself with any other, unrelated,
# legitimately-open entity right-click menu or taskbar-settings window
# using the exact same executable.
db_hq_pids() { pgrep -f "khtpm_entity_menu_render\.\+x .*dashboard\.chtpm" 2>/dev/null || true; }
db_hq_mgr_pids() { pgrep -f "khtpm_hq_manager\.\+x" 2>/dev/null || true; }

pids="$(db_hq_pids) $(db_hq_mgr_pids)"
pids="$(echo "$pids" | tr ' ' '\n' | grep -v '^$' || true)"
if [ -n "$pids" ]; then
    echo "open_db_hq: killing existing instance(s): $(echo $pids | tr '\n' ' ')"
    echo "$pids" | xargs -r kill -TERM
    sleep 1
    pids="$(db_hq_pids) $(db_hq_mgr_pids)"
    pids="$(echo "$pids" | tr ' ' '\n' | grep -v '^$' || true)"
    if [ -n "$pids" ]; then
        echo "open_db_hq: still alive after TERM, escalating to KILL: $(echo $pids | tr '\n' ' ')"
        echo "$pids" | xargs -r kill -KILL
        sleep 1
    fi
fi

setsid nohup "$BIN" "$HOUSE_ROOT" "$CHTPM" \
    >/tmp/db-hq.log 2>&1 < /dev/null &
# REAL FIX 2026-08-25 (au11-hq direct request: "why cant u write that
# final pid to a file and reconsume it?") - the taskbar's own
# ktb_system_recorded() wrapper only sees the PID of the setsid'd
# `open_db_hq.sh` INVOCATION itself (this whole script), not the real
# window binary it launches here - THIS script's own $! (the actual
# `$BIN` process, its own real session/group leader since it's re-
# setsid'd right above) is the one the kill-switch actually needs.
echo $! >> "$HOUSE_ROOT/#.desktop/livedesk_launched_pids.txt" 2>/dev/null || true
disown 2>/dev/null || true
sleep 1

shell_pids="$(db_hq_pids)"
mgr_pids="$(db_hq_mgr_pids)"
shell_n="$(echo "$shell_pids" | grep -c . || true)"
mgr_n="$(echo "$mgr_pids" | grep -c . || true)"
if [ "$shell_n" = "1" ] && [ "$mgr_n" = "1" ]; then
    echo "db-hq launched (shell PID $shell_pids, self-spawned manager PID $mgr_pids, logs=/tmp/db-hq.log)"
else
    echo "open_db_hq: unexpected process count after launch (shell=$shell_n manager=$mgr_n, expected 1/1 - manager should have been self-spawned by the shell's own <module> tag):" >&2
    echo "  shell pids: $shell_pids" >&2
    echo "  manager pids: $mgr_pids" >&2
    cat /tmp/db-hq.log 2>/dev/null >&2
    exit 1
fi
