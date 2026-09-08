#!/bin/sh
# button.sh <house_root> - launch sql-hq (static template + projector,
# shared khtpm_core_render.+x). class="sql-hq database-window".
set -e
HOUSE_ROOT="${1:-}"
[ -n "$HOUSE_ROOT" ] && [ -d "$HOUSE_ROOT" ] || { echo "sql-hq: need house_root as argv[1]" >&2; exit 1; }
HOUSE_ROOT="$(cd "$HOUSE_ROOT" && pwd)"
HERE="$(cd "$(dirname "$0")" && pwd)"
XHTPM="$HERE/sql-hq.xhtpm"
BIN="$HOUSE_ROOT/*.monads/*.livedesk-taskbar/ops/+x/khtpm_core_render.+x"
ENGINE="$HERE/ops/+x/sql_hq.+x"
PROJ="$HERE/ops/+x/sql_hq_projector.+x"

[ -x "$BIN" ]    || (cd "$HOUSE_ROOT/*.monads/*.livedesk-taskbar/ops" && sh build_core_render.sh) || true
[ -x "$ENGINE" ] || sh "$HERE/ops/build_sql_hq.sh" || true
[ -x "$PROJ" ]   || sh "$HERE/ops/build_sql_hq.sh" || true
[ -x "$BIN" ]    || { echo "sql-hq: missing $BIN" >&2; exit 1; }
[ -x "$ENGINE" ] || { echo "sql-hq: missing $ENGINE" >&2; exit 1; }
[ -x "$PROJ" ]   || { echo "sql-hq: missing $PROJ" >&2; exit 1; }
[ -f "$XHTPM" ]  || { echo "sql-hq: missing $XHTPM" >&2; exit 1; }
mkdir -p "$HERE/state/history"
[ -s "$HERE/state/manifest.pdl" ] || printf '# sql-hq workspace manifest\n' > "$HERE/state/manifest.pdl"

for p in $(pgrep -f "khtpm_core_render\.\+x .*sql-hq\.xhtpm" 2>/dev/null || true) \
         $(pgrep -f "sql_hq_projector\.\+x" 2>/dev/null || true); do
    kill "$p" 2>/dev/null || true
done
sleep 1

setsid nohup "$BIN" "$HOUSE_ROOT" "$XHTPM" >/dev/null 2>&1 < /dev/null &
echo "sql-hq launched"
