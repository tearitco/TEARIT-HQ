#!/bin/sh
# Toys: sh button.sh run     HQ-apps: sh button.sh <house_root>
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
ACTION="${1:-run}"
if [ "$ACTION" = "run" ] || [ "$ACTION" = "r" ] || [ "$ACTION" = "start" ]; then
  HOUSE_ROOT="$(cd "$HERE/../.." && pwd)"
elif [ -n "$1" ] && [ -d "$1" ]; then
  HOUSE_ROOT="$(cd "$1" && pwd)"
else
  echo "media-daw: usage: button.sh run | button.sh <house_root>" >&2
  exit 1
fi
XHTPM="$HERE/media-daw.xhtpm"
RENDER_OPS="$HOUSE_ROOT/*.monads/*.livedesk-taskbar/ops"
BIN="$RENDER_OPS/+x/khtpm_core_render.+x"
[ -x "$BIN" ] || (cd "$RENDER_OPS" && sh build_core_render.sh) || true
[ -x "$BIN" ] || { echo "media-daw: missing $BIN" >&2; exit 1; }
mkdir -p "$HERE/state"
for p in $(pgrep -f "khtpm_core_render\.\+x .*media-daw\.xhtpm" 2>/dev/null || true); do
  kill "$p" 2>/dev/null || true
done
sleep 0.3
setsid nohup "$BIN" "$HOUSE_ROOT" "$XHTPM" >/tmp/media-daw.log 2>&1 < /dev/null &
echo "media-daw launched (static xhtpm, HOUSE=$HOUSE_ROOT)"
