#!/bin/sh
# khtpm_png_dump.sh - headless snapshot of any khtpm .chtpm window.
#
# Launches the shared renderer with --dump-and-exit: it paints one real
# frame, writes a PNG (via dump_frame_png_op.+x), a .receipt.txt, and a
# .frame.txt ASCII serialization of the laid-out Elem tree - the tpmos
# "if it's not in current_frame.txt it's not in the pixels" check - then
# quits. No live desktop needed.
#
#   khtpm_png_dump.sh <chtpm_path> [house_root] [out_dir]
#
# Defaults: house_root = nearest ancestor dir containing 44.xyz.01.00
#           (or $KHTPM_HOUSE); out_dir = /tmp
# Env:      DISPLAY (default :0), KHTPM_HOUSE
#
# Prints the three artifact paths, then cats the receipt + frame.txt.
set -u

CHTPM="${1:-}"
[ -n "$CHTPM" ] && [ -f "$CHTPM" ] || { echo "usage: khtpm_png_dump.sh <chtpm_path> [house_root] [out_dir]" >&2; exit 2; }
CHTPM="$(cd "$(dirname "$CHTPM")" && pwd)/$(basename "$CHTPM")"

HOUSE="${2:-${KHTPM_HOUSE:-}}"
if [ -z "$HOUSE" ]; then
    d="$(dirname "$CHTPM")"
    while [ "$d" != "/" ]; do
        [ -d "$d/44.xyz.01.00" ] && { HOUSE="$d/44.xyz.01.00"; break; }
        [ "$(basename "$d")" = "44.xyz.01.00" ] && { HOUSE="$d"; break; }
        d="$(dirname "$d")"
    done
fi
[ -n "$HOUSE" ] && [ -d "$HOUSE" ] || { echo "khtpm_png_dump: can't find house_root (pass it as arg 2 or set KHTPM_HOUSE)" >&2; exit 2; }
HOUSE="$(cd "$HOUSE" && pwd)"

OUT_DIR="${3:-/tmp}"
mkdir -p "$OUT_DIR"
STEM="$(basename "$CHTPM" | sed 's/\.[^.]*$//')"

RENDER="$HOUSE/*.monads/*.livedesk-taskbar/ops/+x/khtpm_core_render.+x"
[ -x "$RENDER" ] || { echo "khtpm_png_dump: missing $RENDER (build it: cd .../ops && sh build_core_render.sh)" >&2; exit 1; }

: "${DISPLAY:=:0}"; export DISPLAY

# The renderer always writes the generic dump to these fixed paths.
SRC_PNG=/tmp/entity-menu-frame.png
rm -f "$SRC_PNG" "$SRC_PNG.receipt.txt" "$SRC_PNG.frame.txt"

timeout 25 "$RENDER" "$HOUSE" "$CHTPM" --dump-and-exit >/dev/null 2>&1
rc=$?

[ -f "$SRC_PNG" ] || { echo "khtpm_png_dump: renderer produced no PNG (exit $rc)" >&2; exit 1; }

PNG="$OUT_DIR/$STEM.png"
RCP="$OUT_DIR/$STEM.receipt.txt"
FRM="$OUT_DIR/$STEM.frame.txt"
cp "$SRC_PNG" "$PNG"
[ -f "$SRC_PNG.receipt.txt" ] && cp "$SRC_PNG.receipt.txt" "$RCP"
[ -f "$SRC_PNG.frame.txt" ]   && cp "$SRC_PNG.frame.txt"   "$FRM"

echo "png    : $PNG"
echo "receipt: $RCP"
echo "frame  : $FRM"
echo "---- receipt ----"; cat "$RCP" 2>/dev/null
echo "---- frame ------"; cat "$FRM" 2>/dev/null
