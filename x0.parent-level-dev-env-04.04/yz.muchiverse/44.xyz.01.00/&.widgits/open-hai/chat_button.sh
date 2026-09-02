#!/bin/bash
# chat_button.sh — launch a PER-INSTANCE chat backed by the SAME shared
# open-hai binaries (2026-08-24, cursword chat). Passes --data-root so
# sessions/state/audit/pidfile redirect under <data_root> (the calling
# entity pal's own chat/ dir) - this instance runs next to plain
# open-hai with its OWN session history: same interface/binary rule,
# separate data. Scoped single-instance: kills ONLY other instances
# sharing THIS exact data root; never touches plain open-hai or another
# entity's chat instance (button.sh in turn only kills instances
# WITHOUT --data-root, so neither launcher can murder the other's).
# Usage: chat_button.sh <house_root> <data_root> [title]
#
# Same proven patterns as button.sh / run_khtpm_strip.sh:
# - pgrep -f full-cmdline matching (reliable despite emoji house path)
# - every pgrep call guarded with `|| true` (exits 1 on zero matches,
#   which would silently abort the script under set -e)
# - TERM then escalate to KILL, confirm exactly one PID after launch.
set -e
HOUSE_ROOT="${1:-}"
DATA_ROOT="${2:-}"
TITLE="${3:-}"
if [ -z "$HOUSE_ROOT" ] || [ ! -d "$HOUSE_ROOT" ] || [ -z "$DATA_ROOT" ]; then
    echo "open-hai chat_button.sh: need house_root argv[1] + data_root argv[2] (+ optional title argv[3])" >&2
    exit 1
fi
HOUSE_ROOT="$(cd "$HOUSE_ROOT" && pwd)"
mkdir -p "$DATA_ROOT"
DATA_ROOT="$(cd "$DATA_ROOT" && pwd)"

HERE="$(cd "$(dirname "$0")" && pwd)"
OPS_DIR="$HERE/ops"
BIN="$OPS_DIR/+x/khtpm_open_hai_render.+x"

if [ ! -x "$BIN" ]; then
    (cd "$OPS_DIR" && sh build_open_hai.sh) || true
fi
if [ ! -x "$BIN" ]; then
    echo "open-hai chat_button.sh: build failed, missing $BIN" >&2
    exit 1
fi

AUDIT_DIR="$DATA_ROOT/audit"
mkdir -p "$AUDIT_DIR"

# PIDs of render instances bound to THIS data root only. /proc cmdline
# is NUL-separated - join tokens with spaces first so the flag and its
# value can be matched as one literal string; trailing space guards
# against prefix collisions between similar data roots.
instance_pids() {
    local p joined
    for p in $(pgrep -f "khtpm_open_hai_render\.\+x" 2>/dev/null || true); do
        joined="$(tr '\0' ' ' < "/proc/$p/cmdline" 2>/dev/null || true)"
        case "$joined" in
            *"--data-root $DATA_ROOT "*) echo "$p" ;;
        esac
    done
    return 0
}

pids="$(instance_pids)"
if [ -n "$pids" ]; then
    echo "open-hai chat_button.sh: killing existing instance(s) for $DATA_ROOT: $(echo $pids | tr '\n' ' ')"
    echo "$pids" | xargs -r kill -TERM
    sleep 1
    pids="$(instance_pids)"
    if [ -n "$pids" ]; then
        echo "open-hai chat_button.sh: still alive after TERM, escalating to KILL: $(echo $pids | tr '\n' ' ')"
        echo "$pids" | xargs -r kill -KILL
        sleep 1
    fi
fi

# POSIX note: build the argv in the positional params (no bash arrays -
# house scripts get run with plain `sh`, which is dash here; original
# HOUSE_ROOT/DATA_ROOT/TITLE are already saved in vars above).
set -- "$HOUSE_ROOT" --data-root "$DATA_ROOT"
if [ -n "$TITLE" ]; then
    set -- "$@" --title "$TITLE"
fi

setsid nohup "$BIN" "$@" \
    >"$AUDIT_DIR/open-hai.log" 2>&1 < /dev/null &
disown 2>/dev/null || true
sleep 1

pids="$(instance_pids)"
n="$(echo "$pids" | grep -c . || true)"
if [ "$n" = "1" ]; then
    echo "chat launched (PID $pids, data=$DATA_ROOT, log=$AUDIT_DIR/open-hai.log)"
elif [ "$n" -gt 1 ] 2>/dev/null; then
    echo "open-hai chat_button.sh: WARNING - $n instances alive after launch (expected 1): $(echo $pids | tr '\n' ' ')" >&2
else
    echo "open-hai chat_button.sh: FAILED to launch - check the log:" >&2
    cat "$AUDIT_DIR/open-hai.log" 2>/dev/null >&2
    exit 1
fi
