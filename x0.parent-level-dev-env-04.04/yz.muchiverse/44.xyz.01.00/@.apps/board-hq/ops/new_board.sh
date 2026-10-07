#!/bin/sh
# new_board.sh <name> - stamp board-<name>.xhtpm (+ board-<name>.css) from the ONE template board.xhtpm / board.css, and create state/<name>/ with a starter board.pdl if missing.
# BOARD_TEMPLATE=<file> overrides the template (verify.sh --selftest uses a broken one).
# Re-run it after editing board.xhtpm / board.css to restyle that instance (it overwrites only the generated board-<name>.* files, never state/<name>/board.pdl).
set -eu
NAME="${1:?usage: new_board.sh <name>}"
case "$NAME" in *[!A-Za-z0-9_-]*|"") echo "name: letters, digits, _ and - only" >&2; exit 2 ;; esac
HERE="$(cd "$(dirname "$0")/.." && pwd)"
awk -v n="$NAME" '{ while ((i = index($0, "@NAME@")) > 0) $0 = substr($0, 1, i - 1) n substr($0, i + 6); print }' "${BOARD_TEMPLATE:-$HERE/board.xhtpm}" > "$HERE/board-$NAME.xhtpm"
cp "$HERE/board.css" "$HERE/board-$NAME.css"
mkdir -p "$HERE/state/$NAME"
[ -f "$HERE/state/$NAME/board.pdl" ] || printf '# board.pdl for the "%s" board - see ops/board_vars_op.c for the format\nBOARD  | title    | %s\nBOARD  | subtitle | (edit state/%s/board.pdl)\n' "$NAME" "$NAME" "$NAME" > "$HERE/state/$NAME/board.pdl"
echo "stamped board-$NAME.xhtpm"
