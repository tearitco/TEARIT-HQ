#!/bin/sh
# sql_hq_action.sh <verb> [arg]   (the renderer also appends '<pkg>' '<house>' - ignored)
# The one write path for sql-hq.xhtpm's buttons. Resolves its own paths
# from $0 so the appended args don't matter.
set -u
VERB="${1:-}"
ARG="${2:-}"

SELF_DIR="$(cd "$(dirname "$0")" && pwd)"          # &.hq-apps/sql-hq/ops
PKG="$(cd "$SELF_DIR/.." && pwd)"                   # &.hq-apps/sql-hq
HOUSE="$(cd "$PKG/../.." && pwd)"                   # 44.xyz.01.00
ENGINE="$SELF_DIR/+x/sql_hq.+x"
STATE="$PKG/state"
MAN="$STATE/manifest.pdl"
EDITOR_BUF="$PKG/text_area_editor.txt"
GRID="$STATE/result.grid.txt"
STATUS="$STATE/status.txt"
ACTIVE="$STATE/active_table.txt"
MACROS="$PKG/sql_hq_macros.pdl"
mkdir -p "$STATE/history"
[ -f "$MAN" ] || printf '# sql-hq workspace manifest\n' > "$MAN"

say() { printf '%s\n' "$*" > "$STATUS"; }

tbl_name() {   # basename without ext, sanitised
    b=$(basename "$1"); b=${b%.*}
    printf '%s' "$b" | tr -c 'A-Za-z0-9_' '_'
}
tbl_kind() { case "$1" in *.pdl) echo pdl ;; *) echo csv ;; esac; }

add_file() {
    f="$1"; [ -f "$f" ] || return 1
    n=$(tbl_name "$f"); k=$(tbl_kind "$f")
    grep -v "| table:$n |" "$MAN" > "$MAN.tmp" 2>/dev/null || true
    mv "$MAN.tmp" "$MAN" 2>/dev/null || true
    printf 'SECTION | table:%s | %s|%s\n' "$n" "$f" "$k" >> "$MAN"
    printf '%s\n' "$n" > "$ACTIVE"
}

case "$VERB" in
  run)
      [ -f "$EDITOR_BUF" ] || { say "editor empty"; exit 0; }
      cp "$EDITOR_BUF" "$STATE/query.sql"
      "$ENGINE" run "$STATE" "$STATE/query.sql" "$GRID" 2>>"$STATE/engine.err"
      ts=$(date +%Y%m%d-%H%M%S)
      cp "$STATE/query.sql" "$STATE/history/$ts.sql" 2>/dev/null || true
      rows=$(grep -c '^' "$GRID" 2>/dev/null || echo 0)
      if grep -q '^-- error' "$GRID" 2>/dev/null; then say "error - see grid"; else say "ran ok ($rows grid lines)"; fi
      ;;
  commit)
      "$ENGINE" commit "$STATE" >> "$STATE/engine.err" 2>&1
      say "committed - source files updated"
      ;;
  rollback)
      rm -f "$STATE/dirty.txt"
      say "rolled back - uncommitted edits discarded; Run again to refresh"
      ;;
  open|open-folder)
      # blocks until the user picks a file in the File Explorer widget
      START="$HOUSE"
      PICK="$(sh "$HOUSE/&.widgits/file-explorer/fe-pick.sh" LOAD "$START" 2>/dev/null)"
      [ -n "$PICK" ] || { say "open cancelled"; exit 0; }
      if [ "$VERB" = "open-folder" ]; then
          d=$(dirname "$PICK"); nadd=0
          for f in "$d"/*.csv "$d"/*.pdl; do [ -f "$f" ] && { add_file "$f"; nadd=$((nadd+1)); }; done
          say "opened folder: $nadd file(s) from $(basename "$d")"
      else
          add_file "$PICK" && say "opened $(basename "$PICK")" || say "open failed: $PICK"
      fi
      ;;
  set-table)
      [ -n "$ARG" ] && { printf '%s\n' "$ARG" > "$ACTIVE"; say "active table: $ARG"; }
      ;;
  close)
      [ -n "$ARG" ] || exit 0
      grep -v "| table:$ARG |" "$MAN" > "$MAN.tmp" 2>/dev/null || true
      mv "$MAN.tmp" "$MAN" 2>/dev/null || true
      [ "$(cat "$ACTIVE" 2>/dev/null)" = "$ARG" ] && : > "$ACTIVE"
      say "closed table $ARG"
      ;;
  macro)
      [ -n "$ARG" ] || exit 0
      snip=$(awk -F'\\|' -v m="macro:$ARG" '
          { gsub(/^ +| +$/,"",$2) }
          $2==m { s=$4; gsub(/^ +/,"",s); print s; exit }' "$MACROS")
      [ -n "$snip" ] || { say "no such macro: $ARG"; exit 0; }
      # append the snippet to the editor buffer; the reparse the status
      # write triggers re-hydrates the <text_area> from this file.
      printf '\n%s\n' "$snip" >> "$EDITOR_BUF"
      say "inserted: $ARG"
      ;;
  export)
      [ -s "$GRID" ] || { say "nothing to export"; exit 0; }
      # pick the destination with the shared File Explorer widget (SAVE mode)
      out="$(sh "$HOUSE/&.widgits/file-explorer/fe-pick.sh" SAVE "$HOME" 2>/dev/null)"
      [ -n "$out" ] || { say "export cancelled"; exit 0; }
      case "$out" in *.csv) : ;; *) out="$out.csv" ;; esac
      # aligned grid -> csv (drop the --- separator + the "(N rows)" footer)
      sed -e '2d' -e 's/ *| */,/g' -e '/^([0-9]* rows*)/d' "$GRID" > "$out" 2>/dev/null
      say "exported -> $out"
      ;;
  clear)
      : > "$EDITOR_BUF"; : > "$GRID"; say "cleared"
      ;;
  *)
      say "unknown verb: $VERB"
      ;;
esac
