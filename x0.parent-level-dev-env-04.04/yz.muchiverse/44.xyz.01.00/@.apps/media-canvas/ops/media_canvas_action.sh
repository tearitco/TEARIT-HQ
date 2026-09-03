#!/bin/sh
# Writes state/ui.txt keys. Does not compile, does not touch the xhtpm.
# Usage: media_canvas_action.sh <view|tool> <value>
set -e
HERE="$(cd "$(dirname "$0")/.." && pwd)"
UI="$HERE/state/ui.txt"
cmd="${1:-}"
val="${2:-}"
[ -f "$UI" ] || exit 1
view=$(grep '^view=' "$UI" | cut -d= -f2-)
tool=$(grep '^tool=' "$UI" | cut -d= -f2-)
[ -n "$view" ] || view=2d
[ -n "$tool" ] || tool=brush
case "$cmd" in
  view)
    view="$val"
    ;;
  tool)
    tool="$val"
    ;;
  *)
    exit 0
    ;;
esac
is_2d=0; is_3d=0
[ "$view" = 2d ] && is_2d=1
[ "$view" = 3d ] && is_3d=1
status="view=$view tool=$tool"
tmp="$UI.tmp"
{
  echo "view=$view"
  echo "is_2d=$is_2d"
  echo "is_3d=$is_3d"
  echo "tool=$tool"
  echo "status=$status"
  echo "n_layers=1"
  echo "layer_0_text=Layer 1"
  echo "canvas_sprite="
} > "$tmp"
mv "$tmp" "$UI"
