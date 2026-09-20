#!/bin/sh
# Place a pal dir from File Explorer onto the desk (tic-tac-toe overlay).
# Usage: fe_place_on_desk.sh <house_root> <fe_package_dir> <src_path>
set -u
HOUSE="${1:-}"
PKG="${2:-}"
SRC="${3:-}"
[ -n "$HOUSE" ] && [ -n "$PKG" ] && [ -n "$SRC" ] || exit 1
[ -e "$SRC" ] || exit 1

REG="$HOUSE/&.widgits/_shared-lib/ops/+x/kh_proc_register_op.+x"
[ -x "$REG" ] && "$REG" "$HOUSE" "$$" fe_place_on_desk >/dev/null 2>&1 || true

CLICK="$PKG/fe_place_click.txt"
rm -f "$CLICK"
export FE_PLACE_CLICK="$CLICK"
# Place target rule: while the overlay is up, an open window that published a drop
# zone (another Inventory, ...) highlights under the pointer; a click inside it moves
# the item into that window's folder. A click anywhere else places it on the desk.
# The source's own folder is never a target.
export FE_PLACE_ZONES=1
FE_PLACE_SKIP_DIR="$(dirname "$SRC")"; export FE_PLACE_SKIP_DIR
FE_PLACE_NAME="$(basename "$SRC")"; export FE_PLACE_NAME
# Hand X focus back to this explorer window before the overlay grabs the keyboard, so Esc
# reaches the placer (the right-click popup that had focus is gone by now).
FP=$(cat "$PKG/module_parent.pid" 2>/dev/null)
case "$FP" in ""|*[!0-9]*) ;; *) export FE_PLACE_FOCUS_PID="$FP" ;; esac
ARM="$HOUSE/&.widgits/tile-picker/ops/+x/tp_arm_placer_rmmv.+x"
[ -x "$ARM" ] || exit 1
"$ARM" "$PKG" "$HOUSE" || exit 1
[ -f "$CLICK" ] || exit 0

ZDEST=$(grep '^zone_dest=' "$CLICK" | sed 's/^zone_dest=//')
if [ -n "$ZDEST" ] && [ -d "$ZDEST" ]; then
    zbase=$(basename "$SRC")
    case "$ZDEST" in "$SRC"|"$SRC"/*) exit 0 ;; esac
    [ "$SRC" = "$ZDEST/$zbase" ] && exit 0
    mv "$SRC" "$ZDEST/$zbase" || exit 1
    exit 0
fi

x=$(grep '^x=' "$CLICK" | sed 's/^x=//')
y=$(grep '^y=' "$CLICK" | sed 's/^y=//')
[ -n "$x" ] && [ -n "$y" ] || exit 1
g=64
x=$(( (x / g) * g ))
y=$(( (y / g) * g ))

base=$(basename "$SRC")
if [ "$(basename "$(dirname "$SRC")")" = "inventory" ]; then
    pals=$(dirname "$(dirname "$(dirname "$SRC")")")
    DEST="$pals/$base"
else
    DEST="$SRC"
fi

if [ "$SRC" != "$DEST" ]; then
    mkdir -p "$(dirname "$DEST")"
    mv "$SRC" "$DEST" || exit 1
fi
printf 'x=%s\ny=%s\n' "$x" "$y" > "$DEST/desktop_pos.txt"

ENT="$HOUSE/"*.monads/*.livedesk-taskbar/ops/+x/khtpm_entity.+x
[ -x "$ENT" ] || ENT="$HOUSE/"*.monads/*.livedesk-taskbar/ops/+x/khtpm_core_render.+x
setsid nohup "$ENT" "$DEST" >/dev/null 2>&1 < /dev/null &
exit 0
