#!/bin/sh
# button.sh - launch export-hq as its own detached X11 process, same
# one-process-launches-a-child shape as signup-hq: this only starts the
# shared renderer (khtpm_core_render.+x); its generic launch_module()
# starts export_hq_manager.+x as its child, tied to the window's
# lifetime.
#   button.sh <house_root>
set -e
HOUSE_ROOT="${1:-}"
[ -n "$HOUSE_ROOT" ] && [ -d "$HOUSE_ROOT" ] || { echo "export-hq button.sh: need house_root as argv[1]" >&2; exit 1; }
HOUSE_ROOT="$(cd "$HOUSE_ROOT" && pwd)"

HERE="$(cd "$(dirname "$0")" && pwd)"
XHTPM="$HERE/export-hq.xhtpm"
RENDER_OPS="$HOUSE_ROOT/*.monads/*.livedesk-taskbar/ops"
BIN="$RENDER_OPS/+x/khtpm_core_render.+x"
MGR="$HERE/+x/export_hq_manager.+x"

[ -x "$BIN" ] || (cd "$RENDER_OPS" && sh build_core_render.sh) || true
[ -x "$BIN" ] || { echo "export-hq button.sh: missing $BIN" >&2; exit 1; }
[ -x "$MGR" ] || (cd "$HERE" && sh build.sh) || true
[ -x "$MGR" ] || { echo "export-hq button.sh: missing $MGR" >&2; exit 1; }
[ -f "$XHTPM" ] || { echo "export-hq button.sh: missing $XHTPM" >&2; exit 1; }

AUDIT="$HOUSE_ROOT/&.hq-apps/export-hq/audit"
mkdir -p "$AUDIT"

pids() { { pgrep -f "khtpm_core_render\.\+x.*export-hq\.xhtpm" 2>/dev/null; pgrep -f "export_hq_manager\.\+x" 2>/dev/null; } | grep -v '^$' || true; }
p="$(pids)"
if [ -n "$p" ]; then
    echo "export-hq button.sh: killing existing instance(s): $(echo $p | tr '\n' ' ')"
    echo "$p" | xargs -r kill -TERM; sleep 1
    p="$(pids)"; [ -n "$p" ] && { echo "$p" | xargs -r kill -KILL; sleep 1; }
fi

mkdir -p "$HOUSE_ROOT/#.desktop/export_hq"
: > "$HOUSE_ROOT/#.desktop/export_hq/request.txt"

setsid nohup "$BIN" "$HOUSE_ROOT" "$XHTPM" >"$AUDIT/export-hq.log" 2>&1 < /dev/null &
printf '%s %s 0 0 export-hq\n' "$!" "$!" >> "$HOUSE_ROOT/#.desktop/livedesk_proc_list.txt" 2>/dev/null || true
disown 2>/dev/null || true
sleep 1

r="$(pgrep -f "khtpm_core_render\.\+x.*export-hq\.xhtpm" 2>/dev/null | grep -c . || true)"
m="$(pgrep -f "export_hq_manager\.\+x" 2>/dev/null | grep -c . || true)"
if [ "$r" = "1" ] && [ "$m" = "1" ]; then
    echo "export-hq launched (renderer $(pgrep -f "khtpm_core_render\.\+x.*export-hq\.xhtpm"), manager $(pgrep -f "export_hq_manager\.\+x"), log=$AUDIT/export-hq.log)"
else
    echo "export-hq button.sh: launch check odd (renderer=$r manager=$m) - log:" >&2
    cat "$AUDIT/export-hq.log" 2>/dev/null >&2
    [ "$r" -ge 1 ] 2>/dev/null || exit 1
fi
