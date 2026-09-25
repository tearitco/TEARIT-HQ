#!/bin/sh
# Usage: open_entity_cli.sh <entity_dir> <house_root>
set -e
ENT="${1:-}"
HOUSE="${2:-}"
[ -d "$ENT" ] && [ -d "$HOUSE" ] || { echo "open_entity_cli.sh: need entity and house" >&2; exit 1; }
HERE="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$HERE/state"
printf '%s\n' "$ENT" > "$HERE/state/target_entity.txt"
RENDER_OPS_DIR="$HOUSE"/*.monads/*.livedesk-taskbar/ops
BIN="$(cd $RENDER_OPS_DIR && pwd)/+x/khtpm_core_render.+x"
[ -x "$BIN" ] || { echo "open_entity_cli.sh: no renderer" >&2; exit 1; }
for p in $(pgrep -f "khtpm_core_render\.\+x .*entity-cli\.xhtpm" 2>/dev/null || true); do
  kill "$p" 2>/dev/null || true
done
setsid nohup "$BIN" "$HOUSE" "$HERE/entity-cli.xhtpm" >/dev/null 2>&1 < /dev/null &
echo "entity cli-io launched"
