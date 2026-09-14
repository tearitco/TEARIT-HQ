#!/bin/sh
# Real, deliberate departure from the usual mr_show_choices.+x command -
# direct instruction ("there needs to be a cancel button. and it should
# have same theme as other context menu"): this launches castle's own
# real menu.chtpm via the SAME launch_khtpm_menu() mechanism every
# entity's own right-click menu already uses (real fork+exec of
# khtpm_core_render.+x against a real .chtpm, theme/CSS pipeline via
# entity_menu_default.css) - not a separate popup style.
cd "$(dirname "$0")/../../.." || exit 1
ENT="$PWD"
D="$ENT"
while [ "$D" != "/" ] && [ ! -d "$D/xyzfs" ]; do D="$(dirname "$D")"; done
HOUSE_ROOT="$D"
BIN="$HOUSE_ROOT/*.monads/*.livedesk-taskbar/ops/+x/khtpm_core_render.+x"
# Gameplay menu (Civ I city options), NOT the standard right-click
# menu.chtpm - see menu_gameplay.chtpm's own header comment.
CHTPM="$ENT/menu_gameplay.chtpm"
X=300
Y=300
if [ -f "$ENT/desktop_pos.txt" ]; then
  ENT_X=$(awk -F= '/^x=/{print $2}' "$ENT/desktop_pos.txt")
  ENT_Y=$(awk -F= '/^y=/{print $2}' "$ENT/desktop_pos.txt")
  [ -n "$ENT_X" ] && X=$((ENT_X + 40))
  [ -n "$ENT_Y" ] && Y=$((ENT_Y + 40))
fi
exec "$BIN" "$HOUSE_ROOT" "$CHTPM" "$X" "$Y"
