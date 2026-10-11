#!/bin/sh
# open_concept_bank_hq.sh - launch the concept-bank-hq X11-HQ window.
# Shape = &.hq-apps/chat-hai/button-pal.sh: this script launches ONE
# process (the shared renderer on concept-bank-hq.xhtpm); the template's
# single <module> (ops/+x/concept_bank_manager.+x) is forked by the
# renderer's own kh_launch_window_modules() and SIGTERM'd on close.
#
#   sh open_concept_bank_hq.sh <house_root>
#
# Wired into the taskbar HQ menu via
# _.monads/_.livedesk-taskbar/ops/open_concept_bank.sh (glob-safe shim; a leading
# '&' in a .pdl cmd is shell job-control). Design: CONCEPT-BANK-HQ-DESIGN.md.
set -e

HOUSE_ROOT="${1:-}"
if [ -z "$HOUSE_ROOT" ] || [ ! -d "$HOUSE_ROOT" ]; then
    echo "open_concept_bank_hq.sh: need house_root as argv[1]" >&2
    exit 1
fi
HOUSE_ROOT="$(cd "$HOUSE_ROOT" && pwd)"

HERE="$(cd "$(dirname "$0")" && pwd)"
XHTPM="$HERE/concept-bank-hq.xhtpm"
MGR="$HERE/ops/+x/concept_bank_manager.+x"
RENDER_OPS_DIR="$HOUSE_ROOT"/_.monads/_.livedesk-taskbar/ops
BIN="$(cd $RENDER_OPS_DIR && pwd)/+x/khtpm_core_render.+x"

[ -x "$BIN" ] || (cd $RENDER_OPS_DIR && sh build_core_render.sh) || true
[ -x "$MGR" ] || (cd "$HERE/ops" && sh build_concept_bank_manager.sh) || true
for f in "$BIN" "$MGR" "$XHTPM"; do
    [ -e "$f" ] || { echo "open_concept_bank_hq.sh: missing $f" >&2; exit 1; }
done

LOG_DIR="$HERE/audit"
mkdir -p "$LOG_DIR"

mine() { pgrep -f "khtpm_core_render\.\+x.*concept-bank-hq\.xhtpm" 2>/dev/null || true; }
strays() { pgrep -f "concept-bank-hq/ops/\+x/concept_bank_manager\.\+x" 2>/dev/null || true; }

pids="$(mine; strays)"
pids="$(echo "$pids" | grep -v '^$' || true)"
if [ -n "$pids" ]; then
    echo "open_concept_bank_hq.sh: killing existing instance(s): $(echo $pids | tr '\n' ' ')"
    echo "$pids" | xargs -r kill -TERM
    sleep 1
    pids="$(mine; strays)"
    pids="$(echo "$pids" | grep -v '^$' || true)"
    [ -n "$pids" ] && { echo "$pids" | xargs -r kill -KILL; sleep 1; }
fi

setsid nohup "$BIN" "$HOUSE_ROOT" "$XHTPM" \
    >"$LOG_DIR/concept-bank-hq.log" 2>&1 < /dev/null &
printf '%s %s 0 0 concept-bank-hq\n' "$!" "$!" >> "$HOUSE_ROOT/#.desktop/livedesk_proc_list.txt" 2>/dev/null || true
disown 2>/dev/null || true
sleep 1

n="$(mine | grep -c . || true)"
if [ "$n" -ge 1 ] 2>/dev/null; then
    echo "concept-bank-hq launched (renderer pid $(mine | tr '\n' ' '), log=$LOG_DIR/concept-bank-hq.log)"
else
    echo "open_concept_bank_hq.sh: FAILED to launch - check the log:" >&2
    cat "$LOG_DIR/concept-bank-hq.log" 2>/dev/null >&2
    exit 1
fi
