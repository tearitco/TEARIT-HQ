#!/bin/sh
# Toys row "Robot Trap Auto". Opens the HTML reference (rta-v3.html) in the desktop browser.
# The house board port is not this script. Usage: sh button.sh run.
[ "${1:-run}" = run ] || exit 0
HERE="$(cd "$(dirname "$0")" && pwd)"
HTML="$(cat "$HERE/reference.path" 2>/dev/null)"
[ -f "$HTML" ] || { echo "rta: reference html missing ($HTML)" >&2; exit 1; }
if command -v xdg-open >/dev/null 2>&1; then
    xdg-open "$HTML"
else
    echo "rta: $HTML"
fi
