#!/bin/bash
# button.sh — launch co-lab-hai as its own detached X11 process.
# Usage: button.sh <house_root>
#
# Real, one-process-launches-a-child shape (same as network-browser's
# own button.sh): this script only ever starts the shared renderer
# (khtpm_core_render.+x); the renderer's own generic launch_module()
# starts colab_hai_manager.+x as its real child, tied to its lifetime -
# closing the window stops the manager too, no separate PID to track.
set -e
HOUSE_ROOT="${1:-}"
if [ -z "$HOUSE_ROOT" ] || [ ! -d "$HOUSE_ROOT" ]; then
    echo "co-lab-hai button.sh: need house_root as argv[1]" >&2
    exit 1
fi
HOUSE_ROOT="$(cd "$HOUSE_ROOT" && pwd)"

HERE="$(cd "$(dirname "$0")" && pwd)"
# co-lab-hai.xhtpm is a STATIC template (x11-hq style) - the manager
# writes state/ui.txt, never this file, so no bootstrap/restore dance.
XHTPM="$HERE/co-lab-hai.xhtpm"

RENDER_OPS_DIR="$HOUSE_ROOT/*.monads/*.livedesk-taskbar/ops"
BIN="$RENDER_OPS_DIR/+x/khtpm_core_render.+x"
MANAGER_BIN="$HERE/+x/colab_hai_manager.+x"

if [ ! -x "$BIN" ]; then
    (cd "$RENDER_OPS_DIR" && sh build_core_render.sh) || true
fi
if [ ! -x "$BIN" ]; then
    echo "co-lab-hai button.sh: build failed, missing $BIN" >&2
    exit 1
fi
if [ ! -x "$MANAGER_BIN" ]; then
    (cd "$HERE" && sh build.sh) || true
fi
if [ ! -x "$MANAGER_BIN" ]; then
    echo "co-lab-hai button.sh: build failed, missing $MANAGER_BIN" >&2
    exit 1
fi

if [ ! -f "$XHTPM" ]; then
    echo "co-lab-hai button.sh: missing template $XHTPM" >&2
    exit 1
fi

AUDIT_DIR="$HOUSE_ROOT/&.hq-apps/co-lab-hai/audit"
mkdir -p "$AUDIT_DIR"

colab_hai_pids() {
    pgrep -f "khtpm_core_render\.\+x.*co-lab-hai\.xhtpm" 2>/dev/null || true
}
# REAL FIX 2026-09-02 (found live, testing this app's own first cut):
# parent_still_alive() in colab_hai_manager.c checks a SHARED
# module_parent.pid file, not "is MY specific renderer PID alive" - if
# an old renderer dies and a NEW one launches before the old manager's
# own next poll tick, the old manager can see the NEW renderer's PID in
# that file and wrongly conclude its own (different, dead) parent is
# still alive, becoming a real orphan that never exits. Two managers
# then race to write the same co-lab-hai.chtpm - confirmed live (one
# showed a stale "Pending: 1" after the other had already approved it).
# Real fix here, matching chat-hai's own button.sh precedent (explicit
# kill of every real child process by name, not just the renderer):
# kill the manager by name too, don't rely on it self-exiting.
colab_hai_manager_pids() {
    pgrep -f "colab_hai_manager\.\+x" 2>/dev/null || true
}

pids="$(printf '%s\n%s\n' "$(colab_hai_pids)" "$(colab_hai_manager_pids)")"
pids="$(echo "$pids" | grep -v '^$' || true)"
if [ -n "$pids" ]; then
    echo "co-lab-hai button.sh: killing existing instance(s): $(echo $pids | tr '\n' ' ')"
    echo "$pids" | xargs -r kill -TERM
    sleep 1
    pids="$(printf '%s\n%s\n' "$(colab_hai_pids)" "$(colab_hai_manager_pids)")"
    pids="$(echo "$pids" | grep -v '^$' || true)"
    if [ -n "$pids" ]; then
        echo "co-lab-hai button.sh: still alive after TERM, escalating to KILL: $(echo $pids | tr '\n' ' ')"
        echo "$pids" | xargs -r kill -KILL
        sleep 1
    fi
fi

setsid nohup "$BIN" "$HOUSE_ROOT" "$XHTPM" \
    >"$AUDIT_DIR/co-lab-hai.log" 2>&1 < /dev/null &
echo $! >> "$HOUSE_ROOT/#.desktop/livedesk_launched_pids.txt" 2>/dev/null || true
disown 2>/dev/null || true
sleep 1

pids="$(colab_hai_pids)"
n="$(echo "$pids" | grep -c . || true)"
mgr_n="$(colab_hai_manager_pids | grep -c . || true)"
if [ "$n" = "1" ] && [ "$mgr_n" = "1" ]; then
    echo "co-lab-hai launched (PID $pids, manager PID $(colab_hai_manager_pids), log=$AUDIT_DIR/co-lab-hai.log)"
elif [ "$mgr_n" -gt 1 ] 2>/dev/null; then
    echo "co-lab-hai button.sh: WARNING - $mgr_n manager instances alive (expected 1): $(colab_hai_manager_pids | tr '\n' ' ')" >&2
elif [ "$n" -gt 1 ] 2>/dev/null; then
    echo "co-lab-hai button.sh: WARNING - $n instances alive after launch (expected 1): $(echo $pids | tr '\n' ' ')" >&2
else
    echo "co-lab-hai button.sh: FAILED to launch - check the log:" >&2
    cat "$AUDIT_DIR/co-lab-hai.log" 2>/dev/null >&2
    exit 1
fi
