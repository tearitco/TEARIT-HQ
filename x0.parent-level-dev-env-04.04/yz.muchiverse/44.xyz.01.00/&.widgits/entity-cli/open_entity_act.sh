#!/bin/sh
# Build an Act menu from skills.pdl and open it.
# A row is: SKILL | Label | command
# command is attack, move, use, or a shell word stored on the row.
# Usage: open_entity_act.sh <entity_dir> <house_root> [x] [y]
#
# REAL FIX 2026-09-24, direct live report ("the act button opens new
# window in entirely wrong location. it should open in same position
# as last window"): optional [x] [y] (the calling window's own real
# ${WIN_X}/${WIN_Y}, khtpm_core_render.c's kh_get_var() builtins) are
# forwarded to the new window's launch as its own real starting
# position - the same real <house_root> <chtpm_path> [x] [y] contract
# that file's own launch_khtpm_menu() already uses (2026-08-16). Absent
# = old behavior (renderer's own generic default spot), not a hard
# requirement - every existing caller with no x/y still works.
set -e
ENT="${1:-}"
HOUSE="${2:-}"
X="${3:-}"
Y="${4:-}"
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
      # REAL FIX 2026-09-26, direct live report ("act says warning
      # malformed template"): this used to emit a backslash-quote
      # inside the action="..." XML attribute - XML has no backslash
      # escaping, so the parser sees a real, unescaped closing quote
      # there and the rest of that attribute value lands as stray
      # bytes outside any attribute, tripping khtpm_core_render.c own
      # NOT-WELL-FORMED / g_window_malformed check (see parse_chtpm()
      # own comment there for the general shape of this class of bug).
      # &quot; is the real, correct XML escape for a quote inside an
      # already-double-quoted attribute value - pre-existing bug, not
      # introduced by this same session Act-position fix.
      printf "    <item label=\"%s\" action=\"sh -c '\''exec &quot;%s/&.widgits/entity-cli/ops/act_row.sh&quot; &quot;%s&quot; &quot;%s&quot;'\''\"/>\n", lab, "'"$HOUSE"'", cmd, "'"$ENT"'"
    }
  ' "$SRC"
  echo '    <item label="Back" action="CLOSE"/>'
  echo '  </page>'
  echo '</window>'
} > "$OUT"
RENDER_OPS_DIR="$HOUSE"/*.monads/*.livedesk-taskbar/ops
BIN="$(cd $RENDER_OPS_DIR && pwd)/+x/khtpm_core_render.+x"
[ -x "$BIN" ] || { echo "open_entity_act.sh: no renderer" >&2; exit 1; }
if [ -n "$X" ] && [ -n "$Y" ]; then
  setsid nohup "$BIN" "$HOUSE" "$OUT" "$X" "$Y" >/dev/null 2>&1 < /dev/null &
else
  setsid nohup "$BIN" "$HOUSE" "$OUT" >/dev/null 2>&1 < /dev/null &
fi
echo "act menu launched"
