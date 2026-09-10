#!/bin/sh
# button.sh <house_root>
# Launcher: resolve paths, single-instance guard, launch the shared
# khtpm_core_render on template-hq.xhtpm. The xhtpm's <module> starts
# refresh.sh. Same shape as &.hq-apps/stats-hq/button-pal.sh and
# &.hq-apps/mon-hq/button.sh - copy this file verbatim for a new app and
# change the three names.
set -u
APP="template-hq"                    # <-- change: xhtpm stem
HOUSE_ROOT="${1:-}"
[ -n "$HOUSE_ROOT" ] && [ -d "$HOUSE_ROOT" ] || { echo "$APP: need house_root as argv[1]" >&2; exit 1; }
HOUSE_ROOT="$(cd "$HOUSE_ROOT" && pwd)"

HERE="$(cd "$(dirname "$0")" && pwd)"
XHTPM="$HERE/$APP.xhtpm"
BIN="$HOUSE_ROOT/*.monads/*.livedesk-taskbar/ops/+x/khtpm_core_render.+x"

chmod +x "$HERE"/*.sh 2>/dev/null || true
[ -x "$BIN" ] || (cd "$HOUSE_ROOT/*.monads/*.livedesk-taskbar/ops" && sh build_core_render.sh) || true
[ -x "$BIN" ] || { echo "$APP: missing $BIN" >&2; exit 1; }
[ -f "$XHTPM" ] || { echo "$APP: missing $XHTPM" >&2; exit 1; }
mkdir -p "$HERE/state"

# single instance: match renderer comm + our xhtpm in cmdline. NEVER a
# bare `pkill -f <script>` - it self-matches the caller's shell
# (pkill-f-self-match-footgun).
for p in $(pgrep -f "khtpm_core_render\.\+x .*$APP\.xhtpm" 2>/dev/null || true); do
    [ "$p" = "$$" ] && continue
    case "$(cat /proc/$p/comm 2>/dev/null)" in sh|bash|dash|zsh) continue ;; esac
    if tr '\0' ' ' < "/proc/$p/cmdline" 2>/dev/null | grep -q "$APP\.xhtpm"; then
        kill "$p" 2>/dev/null || true
        for c in $(pgrep -P "$p" 2>/dev/null || true); do kill "$c" 2>/dev/null || true; done
    fi
done
sleep 1

# seed one frame so the window has content on first paint
sh "$HERE/refresh.sh" --once "$HERE" >/dev/null 2>&1 || true

setsid nohup "$BIN" "$HOUSE_ROOT" "$XHTPM" >/dev/null 2>&1 < /dev/null &
echo "$APP launched"
