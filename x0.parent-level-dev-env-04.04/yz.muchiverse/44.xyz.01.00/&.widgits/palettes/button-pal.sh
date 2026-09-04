#!/bin/sh
# button-pal.sh <category> [house_root]  - PARALLEL launcher for the
# static-template palettes windows (emojis | elements). The old
# palettes_menu.sh launch_cat() path stays as rollback.
#
# Launches ONLY the shared renderer against palettes-<cat>.xhtpm; its
# kh_launch_window_modules() forks BOTH <module>s in the template:
#   palettes_manager.+x args="<cat>"   (unmodified, publishes state)
#   palettes_projector.+x id="<cat>"   (state -> state/palettes-<cat>_ui.txt)
set -e
CAT="${1:?usage: button-pal.sh <category> [house_root]}"

HERE="$(cd "$(dirname "$0")" && pwd)"
HOUSE="${2:-}"
if [ -z "$HOUSE" ] || [ ! -d "$HOUSE" ]; then HOUSE="$(cd "$HERE/../.." && pwd)"; fi
HOUSE="$(cd "$HOUSE" && pwd)"

# emojis/elements/piececraft/debug/rmmv have real ported templates;
# every other category (cdda/df/kenney/paint/generate/user-pallet/...)
# gets the generic "not implemented yet" stub. NOTHING routes to the old
# palettes_menu.sh g_is_palettes C path any more.
case "$CAT" in
    emojis|elements|piececraft|debug|rmmv) XHTPM="$HERE/palettes-$CAT.xhtpm" ;;
    *)  CAT=stub; XHTPM="$HERE/palettes-stub.xhtpm" ;;
esac
OPS="$HOUSE/*.monads/*.livedesk-taskbar/ops"
BIN="$OPS/+x/khtpm_core_render.+x"
MGR="$OPS/+x/palettes_manager.+x"
PROJ="$HERE/ops/+x/palettes_projector.+x"

[ -x "$BIN" ]  || (cd "$OPS" && sh build_core_render.sh) || true
[ -x "$MGR" ]  || (cd "$OPS" && sh build_palettes_manager.sh) || true
[ -x "$PROJ" ] || (cd "$HERE/ops" && sh build_palettes_projector.sh) || true
for f in "$BIN" "$MGR" "$PROJ" "$XHTPM"; do
    [ -e "$f" ] || { echo "palettes-pal: missing $f" >&2; exit 1; }
done
mkdir -p "$HERE/state"

for p in $(pgrep -f "khtpm_core_render\.\+x .*palettes-$CAT\.xhtpm" 2>/dev/null || true) \
         $(pgrep -f "palettes_projector\.\+x .* $CAT\$" 2>/dev/null || true); do
    kill "$p" 2>/dev/null || true
done
sleep 1

setsid nohup "$BIN" "$HOUSE" "$XHTPM" >/tmp/palettes-pal-"$CAT".log 2>&1 < /dev/null &
disown 2>/dev/null || true
sleep 1
if pgrep -f "khtpm_core_render\.\+x .*palettes-$CAT\.xhtpm" >/dev/null; then
    echo "palettes-pal ($CAT) launched"
else
    echo "palettes-pal ($CAT): FAILED - see /tmp/palettes-pal-$CAT.log" >&2
    cat /tmp/palettes-pal-"$CAT".log 2>/dev/null >&2
    exit 1
fi
