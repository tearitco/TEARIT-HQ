#!/bin/bash
# button.sh <house_root> - launch vm-rv-li's khtpm window.
# One-process-launches-a-child shape (same as co-lab-hai/button.sh):
# this script only starts the shared renderer (khtpm_core_render.+x); the
# renderer's generic launch_module() forks vm_rv_li_manager.+x as a real
# child tied to the window's lifetime. Closing the window stops the
# manager (which reaps/terminates the emulator) too.
set -e
APP="vm-rv-li"
HOUSE_ROOT="${1:-}"
[ -n "$HOUSE_ROOT" ] && [ -d "$HOUSE_ROOT" ] || { echo "$APP: need house_root as argv[1]" >&2; exit 1; }
HOUSE_ROOT="$(cd "$HOUSE_ROOT" && pwd)"

HERE="$(cd "$(dirname "$0")" && pwd)"
XHTPM="$HERE/$APP.xhtpm"
RENDER_OPS_DIR="$HOUSE_ROOT/_.monads/_.livedesk-taskbar/ops"
BIN="$RENDER_OPS_DIR/+x/khtpm_core_render.+x"
MANAGER_BIN="$HERE/+x/vm_rv_li_manager.+x"

chmod +x "$HERE"/*.sh "$HERE/ops"/*.sh 2>/dev/null || true
if [ ! -x "$BIN" ]; then
    (cd "$RENDER_OPS_DIR" && sh build_core_render.sh) || true
fi
[ -x "$BIN" ] || { echo "$APP: missing $BIN" >&2; exit 1; }
if [ ! -x "$MANAGER_BIN" ]; then
    (cd "$HERE" && sh build.sh) || true
fi
[ -x "$MANAGER_BIN" ] || { echo "$APP: missing $MANAGER_BIN" >&2; exit 1; }
[ -f "$XHTPM" ] || { echo "$APP: missing $XHTPM" >&2; exit 1; }

AUDIT_DIR="$HOUSE_ROOT/#.desktop/vm_rv_li"
mkdir -p "$AUDIT_DIR"

# single-instance guard: kill any prior renderer+manager for this app.
for p in $(pgrep -f "khtpm_core_render\.\+x .*$APP\.xhtpm" 2>/dev/null || true); do
    [ "$p" = "$$" ] && continue
    case "$(cat /proc/$p/comm 2>/dev/null)" in sh|bash|dash|zsh) continue ;; esac
    if tr '\0' ' ' < "/proc/$p/cmdline" 2>/dev/null | grep -q "$APP\.xhtpm"; then
        kill "$p" 2>/dev/null || true
        for c in $(pgrep -P "$p" 2>/dev/null || true); do kill "$c" 2>/dev/null || true; done
    fi
done
for p in $(pgrep -f "vm_rv_li_manager\.\+x" 2>/dev/null || true); do
    kill "$p" 2>/dev/null || true
    for c in $(pgrep -P "$p" 2>/dev/null || true); do kill "$c" 2>/dev/null || true; done
done
sleep 1

setsid nohup "$BIN" "$HOUSE_ROOT" "$XHTPM" >"$AUDIT_DIR/vm-rv-li.log" 2>&1 < /dev/null &
echo "$APP launched (renderer pid $!)"
