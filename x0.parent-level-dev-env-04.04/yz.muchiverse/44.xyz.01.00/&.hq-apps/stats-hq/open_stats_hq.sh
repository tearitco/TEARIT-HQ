#!/bin/bash
# open_stats_hq.sh — launch stats-hq as its own detached X11 process.
#
# REAL FIX 2026-08-25 — full TPMOS-compliant rebuild
# (au11-hq/TPMOS-COMPLIANCE-DEBT.md's own worst finding). This script
# used to do the ENTIRE job by itself: inline `grep -oE` scraping of
# session-stats .txt files, hand-`printf`'d <tabbar>/<tab> XML spliced
# into dashboard.template.chtpm at every launch, then a launch of the
# OLD standalone khtpm_hq_render.c - no manager, no testable Op, tabs
# that rendered but never responded to a click. All of that business
# logic is now owned by a real, separate, compiled, independently
# testable manager (stats_hq_manager.c) that dashboard.chtpm's own real
# <module src="..."/> tag launches itself - this script's job shrinks to
# exactly what open_db_hq.sh's own does: resolve paths, single-instance
# guard, launch the shared renderer. Same shape, not reinvented.
#
# Usage: open_stats_hq.sh <house_root> [session_id unused - see below]
set -e
HOUSE_ROOT="${1:-}"
if [ -z "$HOUSE_ROOT" ] || [ ! -d "$HOUSE_ROOT" ]; then
    echo "open_stats_hq: need house_root as argv[1]" >&2
    exit 1
fi
HOUSE_ROOT="$(cd "$HOUSE_ROOT" && pwd)"

SELF_DIR="$(cd "$(dirname "$0")" && pwd)"
# 2026-09-03 static-xhtpm port: route to button-pal.sh (stats-hq-pal.xhtpm).
# STATS_ROLLBACK=1 keeps the old dashboard.chtpm path.
[ -z "${STATS_ROLLBACK:-}" ] && [ -x "$SELF_DIR/button-pal.sh" ] && exec sh "$SELF_DIR/button-pal.sh" "$HOUSE_ROOT"

# REAL Stage 5-style single-binary merge (2026-08-25, matching db-hq's
# own 2026-08-16 §5d.10 migration exactly): stats-hq now runs through
# the SAME compiled khtpm_core_render.+x db-hq/events-hq/chat-hai
# already use, mode-selected by `<window class="stats-hq">` in
# dashboard.chtpm. khtpm_hq_render.c/build_db_hq.sh are kept only as
# stats-hq's own former reference (au11-hq/khtpm-merge-how2.md's own
# "kept live for stats-hq" note is now stale - see TPMOS-COMPLIANCE-
# DEBT.md for the real, current status).
OPS_DIR="$HOUSE_ROOT/*.monads/*.livedesk-taskbar/ops"
BIN="$OPS_DIR/+x/khtpm_core_render.+x"
MGR_BIN="$OPS_DIR/+x/stats_hq_manager.+x"
CHTPM="$HOUSE_ROOT/&.hq-apps/stats-hq/dashboard.chtpm"

if [ ! -x "$BIN" ]; then
    (cd "$OPS_DIR" && sh build_core_render.sh) || true
fi
if [ ! -x "$BIN" ]; then
    echo "open_stats_hq: build failed, missing $BIN" >&2
    exit 1
fi
if [ ! -x "$MGR_BIN" ]; then
    (cd "$OPS_DIR" && sh build_stats_hq_manager.sh) || true
fi
if [ ! -x "$MGR_BIN" ]; then
    echo "open_stats_hq: manager build failed, missing $MGR_BIN" >&2
    exit 1
fi

# pgrep exits 1 (nonzero) when nothing matches - guarded with `|| true`
# everywhere under `set -e`, same real precedent open_db_hq.sh's own
# comment documents (_.0.aigent-testing-k9.txt "SCOPE ADDENDUM
# 2026-08-13"). Matches by the real chtpm PATH too, not just the binary
# name, since khtpm_core_render.+x is a real, genuinely shared
# binary - a bare binary-name match would incorrectly kill/confuse
# itself with any other legitimately-open mode using the same exe.
stats_hq_pids() { pgrep -f "khtpm_core_render\.\+x .*stats-hq/dashboard\.chtpm" 2>/dev/null || true; }
stats_hq_mgr_pids() { pgrep -f "stats_hq_manager\.\+x" 2>/dev/null || true; }

pids="$(stats_hq_pids) $(stats_hq_mgr_pids)"
pids="$(echo "$pids" | tr ' ' '\n' | grep -v '^$' || true)"
if [ -n "$pids" ]; then
    echo "open_stats_hq: killing existing instance(s): $(echo $pids | tr '\n' ' ')"
    echo "$pids" | xargs -r kill -TERM
    sleep 1
    pids="$(stats_hq_pids) $(stats_hq_mgr_pids)"
    pids="$(echo "$pids" | tr ' ' '\n' | grep -v '^$' || true)"
    if [ -n "$pids" ]; then
        echo "open_stats_hq: still alive after TERM, escalating to KILL: $(echo $pids | tr '\n' ' ')"
        echo "$pids" | xargs -r kill -KILL
        sleep 1
    fi
fi

setsid nohup "$BIN" "$HOUSE_ROOT" "$CHTPM" \
    >/tmp/stats-hq.log 2>&1 < /dev/null &
# REAL FIX 2026-08-25 (au11-hq direct request: "why cant u write that
# final pid to a file and reconsume it?") - see open_db_hq.sh's own
# identical line for the full rationale.
echo $! >> "$HOUSE_ROOT/#.desktop/livedesk_launched_pids.txt" 2>/dev/null || true
disown 2>/dev/null || true
sleep 1

shell_pids="$(stats_hq_pids)"
mgr_pids="$(stats_hq_mgr_pids)"
shell_n="$(echo "$shell_pids" | grep -c . || true)"
mgr_n="$(echo "$mgr_pids" | grep -c . || true)"
if [ "$shell_n" = "1" ] && [ "$mgr_n" = "1" ]; then
    echo "stats-hq launched (shell PID $shell_pids, self-spawned manager PID $mgr_pids, logs=/tmp/stats-hq.log)"
else
    echo "open_stats_hq: unexpected process count after launch (shell=$shell_n manager=$mgr_n, expected 1/1):" >&2
    echo "  shell pids: $shell_pids" >&2
    echo "  manager pids: $mgr_pids" >&2
    cat /tmp/stats-hq.log 2>/dev/null >&2
    exit 1
fi
