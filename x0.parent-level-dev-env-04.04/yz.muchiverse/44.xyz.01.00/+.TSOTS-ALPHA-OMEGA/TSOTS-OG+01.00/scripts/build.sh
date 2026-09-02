#!/bin/bash
# scripts/build.sh - compile TSOTS (house pal-native, adapted from
# 102.agy-txt/scripts/build.sh's own real shape - same pattern, own ops).
set -e
SCRIPT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$SCRIPT_DIR"

mkdir -p ops/+x system

CFLAGS="-Wall -Wextra -O2"

echo "--- Copying system processes (from wsr-pal) ---"
WSR="$(cd "$SCRIPT_DIR/../.." && pwd)/014.wsr-pal💸️📌️+2"
if [ -d "$WSR" ]; then
    cp "$WSR/system/prisc+x" system/prisc+x 2>/dev/null || true
    cp "$WSR/system/keyboard_input" system/keyboard_input 2>/dev/null || true
    cp "$WSR/system/renderer" system/renderer 2>/dev/null || true
    cp "$WSR/system/chtpm_parser_pal" system/chtpm_parser_pal 2>/dev/null || true
    chmod +x system/prisc+x system/keyboard_input system/renderer system/chtpm_parser_pal 2>/dev/null || true
    echo "copied system/ from wsr-pal"
else
    echo "WARN: wsr-pal not found - system/ not copied"
fi

echo "--- Building TSOTS ops ---"
gcc $CFLAGS -o "ops/+x/tsots_compose.+x" "ops/tsots_compose.c"
gcc $CFLAGS -o "ops/+x/tsots_input.+x" "ops/tsots_input.c"
gcc $CFLAGS -o "ops/+x/tsots_deal.+x" "ops/tsots_deal.c"

echo "--- Copying GL/RGB system binaries (§35 GL-primary) ---"
if [ -d "$WSR" ]; then
    cp "$WSR/system/chtpm_rgb_render" system/chtpm_rgb_render 2>/dev/null || true
    cp "$WSR/system/gl_mirror" system/gl_mirror 2>/dev/null || true
    chmod +x system/chtpm_rgb_render system/gl_mirror 2>/dev/null || true
    echo "copied chtpm_rgb_render + gl_mirror from wsr-pal"

    echo "--- Copying font glyph registry (required, project-local by design) ---"
    mkdir -p "$SCRIPT_DIR/pieces/registry/fonts/ascii"
    for dir in "$WSR/pieces/registry/fonts/ascii/"*/; do
        [ -d "$dir" ] || continue
        code="$(basename "$dir")"
        mkdir -p "$SCRIPT_DIR/pieces/registry/fonts/ascii/$code"
        cp "$dir/glyph.txt" "$SCRIPT_DIR/pieces/registry/fonts/ascii/$code/glyph.txt"
    done
    echo "glyphs: $(ls "$SCRIPT_DIR/pieces/registry/fonts/ascii/" | wc -l)"
else
    echo "WARN: wsr-pal not found, GL/RGB binaries not linked (ASCII-only fallback stays available)"
fi

echo "build ok"
