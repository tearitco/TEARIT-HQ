#!/bin/sh
# Pin muta system tools into 205.ttg-tactics/system/ (copy binaries if present)
set -e
DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
HOUSE=$(CDPATH= cd -- "$DIR/.." && pwd)
MUTA=$(ls -d "$HOUSE"/101.mutaclsym* 2>/dev/null | head -1)
OUT="$DIR/system"
mkdir -p "$OUT"
echo "SYSTEM_ORIGIN=$MUTA" > "$DIR/SYSTEM_ORIGIN.txt"
if [ -z "$MUTA" ]; then
  echo "no muta found — skip vendor"
  exit 0
fi
for b in gl_mirror renderer keyboard_input chtpm_rgb_render; do
  if [ -x "$MUTA/system/$b" ]; then
    cp -f "$MUTA/system/$b" "$OUT/$b"
    echo "vendored $b"
  else
    echo "skip $b (not built in muta)"
  fi
done
# also try build gl_mirror from muta sources if missing
if [ ! -x "$OUT/gl_mirror" ] && [ -f "$MUTA/system/gl_mirror.c" ]; then
  gcc -O2 -o "$OUT/gl_mirror" "$MUTA/system/gl_mirror.c" -lGL -lGLU -lglut -lm 2>/dev/null && echo "built gl_mirror" || echo "gl_mirror build failed"
fi
if [ ! -x "$OUT/renderer" ] && [ -f "$MUTA/system/renderer.c" ]; then
  gcc -O2 -o "$OUT/renderer" "$MUTA/system/renderer.c" 2>/dev/null && echo "built renderer" || true
fi
if [ ! -x "$OUT/keyboard_input" ] && [ -f "$MUTA/system/keyboard_input.c" ]; then
  gcc -O2 -o "$OUT/keyboard_input" "$MUTA/system/keyboard_input.c" 2>/dev/null && echo "built keyboard_input" || true
fi
if [ ! -x "$OUT/chtpm_rgb_render" ] && [ -f "$MUTA/system/chtpm_rgb_render.c" ]; then
  gcc -O2 -o "$OUT/chtpm_rgb_render" "$MUTA/system/chtpm_rgb_render.c" -lm 2>/dev/null && echo "built chtpm_rgb_render" || true
fi
echo "vendor done"
