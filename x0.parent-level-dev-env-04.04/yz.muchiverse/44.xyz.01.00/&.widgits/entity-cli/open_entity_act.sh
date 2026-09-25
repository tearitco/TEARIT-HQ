#!/bin/sh
# Build an Act menu from skills.pdl and open it.
# A row is: SKILL | Label | command
# command is attack, move, use, or a shell word stored on the row.
# Usage: open_entity_act.sh <entity_dir> <house_root>
set -e
ENT="${1:-}"
HOUSE="${2:-}"
[ -d "$ENT" ] && [ -d "$HOUSE" ] || { echo "open_entity_act.sh: need entity and house" >&2; exit 1; }
HERE="$(cd "$(dirname "$0")" && pwd)"
SRC="$ENT/skills.pdl"
[ -f "$SRC" ] || SRC="$HERE/skills.pdl"
mkdir -p "$HERE/state"
OUT="$HERE/state/act.xhtpm"
{
  echo '<window label="Act" class="entity-menu">'
  echo '  <page name="main">'
  awk -F'|' '
    $1 ~ /SKILL/ {
      gsub(/^[ \t]+|[ \t]+$/, "", $2)
      gsub(/^[ \t]+|[ \t]+$/, "", $3)
      if ($2 == "") next
      lab=$2; cmd=$3
      gsub(/&/, "\\&amp;", lab)
      printf "    <item label=\"%s\" action=\"sh -c '\''exec \\\"%s/&.widgits/entity-cli/ops/act_row.sh\\\" \\\"%s\\\" \\\"%s\\\"'\''\"/>\n", lab, "'"$HOUSE"'", cmd, "'"$ENT"'"
    }
  ' "$SRC"
  echo '    <item label="Back" action="CLOSE"/>'
  echo '  </page>'
  echo '</window>'
} > "$OUT"
RENDER_OPS_DIR="$HOUSE"/*.monads/*.livedesk-taskbar/ops
BIN="$(cd $RENDER_OPS_DIR && pwd)/+x/khtpm_core_render.+x"
[ -x "$BIN" ] || { echo "open_entity_act.sh: no renderer" >&2; exit 1; }
setsid nohup "$BIN" "$HOUSE" "$OUT" >/dev/null 2>&1 < /dev/null &
echo "act menu launched"
