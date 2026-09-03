#!/bin/sh
# Toys launch:  sh button.sh run
# HQ-apps launch: sh button.sh <house_root>
# HOUSE is derived from this dir (@.apps/media-canvas → house_root), never hardcoded.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
ACTION="${1:-run}"
if [ "$ACTION" = "run" ] || [ "$ACTION" = "r" ] || [ "$ACTION" = "start" ]; then
  HOUSE_ROOT="$(cd "$HERE/../.." && pwd)"
elif [ -n "$1" ] && [ -d "$1" ]; then
  HOUSE_ROOT="$(cd "$1" && pwd)"
else
  echo "media-canvas: usage: button.sh run | button.sh <house_root>" >&2
  exit 1
fi
XHTPM="$HERE/media-canvas.xhtpm"
RENDER_OPS="$HOUSE_ROOT/*.monads/*.livedesk-taskbar/ops"
BIN="$RENDER_OPS/+x/khtpm_core_render.+x"
[ -x "$BIN" ] || (cd "$RENDER_OPS" && sh build_core_render.sh) || true
[ -x "$BIN" ] || { echo "media-canvas: missing $BIN" >&2; exit 1; }
[ -f "$XHTPM" ] || { echo "media-canvas: missing $XHTPM" >&2; exit 1; }
mkdir -p "$HERE/state"
for p in $(pgrep -f "khtpm_core_render\.\+x .*media-canvas\.xhtpm" 2>/dev/null || true); do
  kill "$p" 2>/dev/null || true
done
sleep 0.3
setsid nohup "$BIN" "$HOUSE_ROOT" "$XHTPM" >/tmp/media-canvas.log 2>&1 < /dev/null &
echo "media-canvas launched (static xhtpm, HOUSE=$HOUSE_ROOT)"
