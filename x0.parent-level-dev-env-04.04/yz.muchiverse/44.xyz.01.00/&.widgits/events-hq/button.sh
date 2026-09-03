#!/bin/bash
# button.sh — launch events-hq for one entity's event_pkg.
#
# Real dispatch convention (fo-menu-sys.md, confirmed live by reading
# tp_desktop_window.c's own METHOD-execution code before writing this,
# same "$1=package_dir $2=house_root" signature open_event_ez.sh
# already uses): a bare METHOD action string gets invoked automatically
# as `<action> '<package_dir>' '<house_root>' >/dev/null 2>&1 &` - NOT
# via env vars. Positional args are the primary contract; EZ_PKG_NAME/
# EZ_PKG_DIR env vars are kept as an override for direct/manual
# invocation (matches event-ez's own contract, so both editors can be
# pointed at the exact same event_pkg/ for comparison).
#
# Usage:
#   sh button.sh <package_dir> [house_root]     — real METHOD dispatch shape
#   EZ_PKG_NAME=x EZ_PKG_DIR=y sh button.sh run  — manual/test override
# REAL §5d.11 (2026-08-16, khtpm-merge-how2.md §5d) - the real, literal
# binary merge: events-hq now runs through the SAME compiled
# khtpm_core_render.+x entity-menu/taskbar-settings/db-hq
# already use, mode-selected by `<window class="events-hq-window">`
# (its own real, pre-existing class attribute - no new one needed),
# genuinely one binary, not four, verified live before this launcher
# was retargeted. khtpm_events_hq_render.c/build_events_hq.sh are kept
# as real, working reference/rollback, just no longer what this
# launcher points at. Unlike db-hq/taskbar-settings, events-hq is
# legitimately multi-instance (scoped by PKG_DIR, see same_entity_pids()
# below) - that real constraint carries over unchanged, just matching
# the new binary name now.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"

# --- PHASE 3 retarget (EVENTS-HQ-XHTPM-PORT.md §8 step 9) ---------------
# The events editor is now the STATIC events-hq.xhtpm + evhq_projector
# (button-pal.sh). Normal positional invocation routes there. Everything
# below is the pre-port dashboard.chtpm launch, kept as the rollback and
# for `EZ_PKG_DIR=... sh button.sh run` A/B comparison. To roll back:
# delete these 3 lines.
[ -z "${EZ_PKG_DIR:-}" ] && [ -n "${1:-}" ] && exec sh "$HERE/button-pal.sh" "$@"
# ----------------------------------------------------------------------

OPS_DIR="$HERE/../../*.monads/*.livedesk-taskbar/ops"
BIN="$OPS_DIR/+x/khtpm_core_render.+x"
CHTPM="$HERE/pieces/dashboard.chtpm"

if [ ! -x "$BIN" ]; then
    (cd "$OPS_DIR" && sh build_core_render.sh) || true
fi
if [ ! -x "$BIN" ]; then
    echo "events-hq: build failed, missing $BIN" >&2
    exit 1
fi

if [ -n "${EZ_PKG_DIR:-}" ]; then
    PKG_DIR="$EZ_PKG_DIR"
    LABEL="${EZ_PKG_NAME:-entity}"
    H="$(cd "$HERE/../.." && pwd)"
else
    PACKAGE_DIR="${1:?usage: button.sh <package_dir> [house_root]}"
    PACKAGE_DIR="$(cd "$PACKAGE_DIR" && pwd)"
    H="${2:-}"
    if [ -z "$H" ] || [ ! -d "$H" ]; then H="$(cd "$HERE/../.." && pwd)"; fi
    PKG_DIR="$PACKAGE_DIR/event_pkg"
    LABEL="$(basename "$PACKAGE_DIR")"
fi
mkdir -p "$PKG_DIR"

# REAL FIX 2026-08-13: same class of bug found+fixed in open-hai's
# button.sh (concurrent processes racing on shared state, full writeup
# in _.0.aigent-testing-k9.txt "SCOPE ADDENDUM 2026-08-13") also
# applies here in principle, BUT events-hq is legitimately
# multi-instance (one window per entity's event_pkg is a real,
# intended use case - editing ava's events and another entity's events
# at the same time is not a bug). A blanket kill-by-binary-name would
# break that. Scope the guard to THIS SPECIFIC PKG_DIR instead - only
# kill/replace a prior instance already open on the SAME entity, never
# touch instances open on other entities.
# pgrep exits 1 (nonzero) when nothing matches - guarded with `|| true`
# everywhere under `set -e` (a bare unguarded assignment from a
# failing command substitution silently aborts the whole script).
#
# NOT using a pgrep -f regex here on purpose - house paths routinely
# contain emoji, parens, and other regex metacharacters (this exact
# tree has both), and hand-escaping a path into a correct regex is a
# real, proven way to silently match nothing (or the wrong thing).
# Instead: get candidate PIDs by binary name only, then byte-compare
# each one's actual argv (via /proc/<pid>/cmdline, NUL-separated) for
# an EXACT match on PKG_DIR - no regex, no escaping, can't misfire.
same_entity_pids() {
    for pid in $(pgrep -f "khtpm_core_render\.\+x" 2>/dev/null || true); do
        if [ -r "/proc/$pid/cmdline" ]; then
            if tr '\0' '\n' < "/proc/$pid/cmdline" 2>/dev/null | grep -qxF "$PKG_DIR"; then
                echo "$pid"
            fi
        fi
    done
}

pids="$(same_entity_pids)"
if [ -n "$pids" ]; then
    echo "events-hq: killing existing instance already open on $LABEL: $(echo $pids | tr '\n' ' ')"
    echo "$pids" | xargs -r kill -TERM
    sleep 1
    pids="$(same_entity_pids)"
    if [ -n "$pids" ]; then
        echo "events-hq: still alive after TERM, escalating to KILL: $(echo $pids | tr '\n' ' ')"
        echo "$pids" | xargs -r kill -KILL
        sleep 1
    fi
fi

setsid nohup "$BIN" "$H" "$CHTPM" "$PKG_DIR" "$LABEL" \
    >/tmp/events-hq-"$LABEL".log 2>&1 < /dev/null &
disown 2>/dev/null || true
sleep 1

pids="$(same_entity_pids)"
n="$(echo "$pids" | grep -c . || true)"
if [ "$n" = "1" ]; then
    echo "events-hq launched for $LABEL (PID $pids, EZ_PKG_DIR=$PKG_DIR) log=/tmp/events-hq-$LABEL.log"
elif [ "$n" -gt 1 ] 2>/dev/null; then
    echo "events-hq: WARNING - $n instances alive for $LABEL after launch (expected 1): $(echo $pids | tr '\n' ' ')" >&2
else
    echo "events-hq: FAILED to launch for $LABEL - check the log:" >&2
    cat /tmp/events-hq-"$LABEL".log 2>/dev/null >&2
    exit 1
fi
