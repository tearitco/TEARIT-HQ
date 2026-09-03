#!/bin/sh
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
ACTION="${1:-run}"
if [ "$ACTION" = "run" ] || [ "$ACTION" = "r" ] || [ "$ACTION" = "start" ]; then
  HOUSE_ROOT="$(cd "$HERE/../.." && pwd)"
elif [ -n "$1" ] && [ -d "$1" ]; then
  HOUSE_ROOT="$(cd "$1" && pwd)"
else
  echo "media-vid: usage: button.sh run | button.sh <house_root>" >&2
  exit 1
fi
XHTPM="$HERE/media-vid.xhtpm"
RENDER_OPS="$HOUSE_ROOT/*.monads/*.livedesk-taskbar/ops"
BIN="$RENDER_OPS/+x/khtpm_core_render.+x"
[ -x "$BIN" ] || (cd "$RENDER_OPS" && sh build_core_render.sh) || true
[ -x "$BIN" ] || { echo "media-vid: missing $BIN" >&2; exit 1; }
mkdir -p "$HERE/state"
for p in $(pgrep -f "khtpm_core_render\.\+x .*media-vid\.xhtpm" 2>/dev/null || true); do
  kill "$p" 2>/dev/null || true
done
sleep 0.3
setsid nohup "$BIN" "$HOUSE_ROOT" "$XHTPM" >/tmp/media-vid.log 2>&1 < /dev/null &
echo "media-vid launched (static xhtpm, HOUSE=$HOUSE_ROOT)"
