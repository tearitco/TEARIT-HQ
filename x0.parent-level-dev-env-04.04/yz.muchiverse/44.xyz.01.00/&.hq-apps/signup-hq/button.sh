#!/bin/sh
# button.sh - launch signup-hq as its own detached X11 process, the
# same one-process-launches-a-child shape as co-lab-hai / network:
# this only starts the shared renderer (khtpm_core_render.+x); the
# renderer's generic launch_module() starts signup_hq_manager.+x as its
# child, tied to the window's lifetime.
#   button.sh <house_root>
set -e
HOUSE_ROOT="${1:-}"
[ -n "$HOUSE_ROOT" ] && [ -d "$HOUSE_ROOT" ] || { echo "signup-hq button.sh: need house_root as argv[1]" >&2; exit 1; }
HOUSE_ROOT="$(cd "$HOUSE_ROOT" && pwd)"

HERE="$(cd "$(dirname "$0")" && pwd)"
# signup-hq.xhtpm is a STATIC template (x11-hq style; the projector
# writes state/ui.txt, never this file) - no bootstrap/restore dance.
XHTPM="$HERE/signup-hq.xhtpm"
RENDER_OPS="$HOUSE_ROOT/*.monads/*.livedesk-taskbar/ops"
BIN="$RENDER_OPS/+x/khtpm_core_render.+x"
MGR="$HERE/+x/signup_hq_manager.+x"

[ -x "$BIN" ] || (cd "$RENDER_OPS" && sh build_core_render.sh) || true
[ -x "$BIN" ] || { echo "signup-hq button.sh: missing $BIN" >&2; exit 1; }
[ -x "$MGR" ] || (cd "$HERE" && sh build.sh) || true
[ -x "$MGR" ] || { echo "signup-hq button.sh: missing $MGR" >&2; exit 1; }
[ -f "$XHTPM" ] || { echo "signup-hq button.sh: missing $XHTPM" >&2; exit 1; }

AUDIT="$HOUSE_ROOT/&.hq-apps/signup-hq/audit"
mkdir -p "$AUDIT"

pids() { { pgrep -f "khtpm_core_render\.\+x.*signup-hq\.xhtpm" 2>/dev/null; pgrep -f "signup_hq_manager\.\+x" 2>/dev/null; } | grep -v '^$' || true; }
p="$(pids)"
if [ -n "$p" ]; then
    echo "signup-hq button.sh: killing existing instance(s): $(echo $p | tr '\n' ' ')"
    echo "$p" | xargs -r kill -TERM; sleep 1
    p="$(pids)"; [ -n "$p" ] && { echo "$p" | xargs -r kill -KILL; sleep 1; }
fi

# reset the request file so a stale line can't fire on launch
mkdir -p "$HOUSE_ROOT/#.desktop/signup_hq"
: > "$HOUSE_ROOT/#.desktop/signup_hq/request.txt"

setsid nohup "$BIN" "$HOUSE_ROOT" "$XHTPM" >"$AUDIT/signup-hq.log" 2>&1 < /dev/null &
echo $! >> "$HOUSE_ROOT/#.desktop/livedesk_launched_pids.txt" 2>/dev/null || true
disown 2>/dev/null || true
sleep 1

r="$(pgrep -f "khtpm_core_render\.\+x.*signup-hq\.xhtpm" 2>/dev/null | grep -c . || true)"
m="$(pgrep -f "signup_hq_manager\.\+x" 2>/dev/null | grep -c . || true)"
if [ "$r" = "1" ] && [ "$m" = "1" ]; then
    echo "signup-hq launched (renderer $(pgrep -f "khtpm_core_render\.\+x.*signup-hq\.xhtpm"), manager $(pgrep -f "signup_hq_manager\.\+x"), log=$AUDIT/signup-hq.log)"
else
    echo "signup-hq button.sh: launch check odd (renderer=$r manager=$m) - log:" >&2
    cat "$AUDIT/signup-hq.log" 2>/dev/null >&2
    [ "$r" -ge 1 ] 2>/dev/null || exit 1
fi
