#!/bin/sh
# Open the hq-ftp placeholder window. Not a file transfer.
# Usage: open_hq_ftp.sh <house_root>
set -e
HOUSE_ROOT="${1:-}"
if [ -z "$HOUSE_ROOT" ] || [ ! -d "$HOUSE_ROOT" ]; then
  echo "open_hq_ftp.sh: need house_root" >&2
  exit 1
fi
HOUSE_ROOT="$(cd "$HOUSE_ROOT" && pwd)"
HERE="$(cd "$(dirname "$0")" && pwd)"
XHTPM="$HERE/hq-ftp.xhtpm"
RENDER_OPS_DIR="$HOUSE_ROOT"/*.monads/*.livedesk-taskbar/ops
BIN="$(cd $RENDER_OPS_DIR && pwd)/+x/khtpm_core_render.+x"
[ -x "$BIN" ] || { echo "open_hq_ftp.sh: no renderer at $BIN" >&2; exit 1; }
# one window: replace a previous placeholder if it is still up
for p in $(pgrep -f "khtpm_core_render\.\+x .*hq-ftp\.xhtpm" 2>/dev/null || true); do
  kill "$p" 2>/dev/null || true
done
setsid nohup "$BIN" "$HOUSE_ROOT" "$XHTPM" >/dev/null 2>&1 < /dev/null &
echo "hq-ftp placeholder launched"
