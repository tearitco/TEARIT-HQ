#!/bin/bash
# scripts/build.sh - compile agy-txt (house pal-native, adapted from
# 102.editor-📄️00.00's own scripts/build.sh - same shape, own ops).
set -e
SCRIPT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$SCRIPT_DIR"

# Resolve shared-lib canonical source (symlink-free)
_sr="$PWD"; while [ ! -d "$_sr/&.widgits/_shared-lib" ] && [ "$_sr" != "/" ]; do _sr="$(dirname "$_sr")"; done
_SS="$_sr/&.widgits/_shared-lib"

mkdir -p ops/+x system

CFLAGS="-Wall -Wextra -O2"

echo "--- Copying system processes (from wsr-pal, same pattern as file-menu's own build.sh) ---"
WSR="$(cd "$SCRIPT_DIR/.." && pwd)/014.wsr-pal💸️📌️+2"
if [ -d "$WSR" ]; then
    cp "$WSR/system/prisc+x" system/prisc+x 2>/dev/null || true
    cp "$WSR/system/keyboard_input" system/keyboard_input 2>/dev/null || true
    cp "$WSR/system/renderer" system/renderer 2>/dev/null || true
    cp "$WSR/system/chtpm_parser_pal" system/chtpm_parser_pal 2>/dev/null || true
    chmod +x system/prisc+x system/keyboard_input system/renderer system/chtpm_parser_pal 2>/dev/null || true
    echo "copied system/ from wsr-pal"
else
    echo "WARN: wsr-pal not found - building system/ from source instead"
    gcc $CFLAGS "$_SS/system/prisc+x.c" -o "system/prisc+x" 2>/dev/null || true
fi

echo "--- Building agy-txt ops ---"
gcc $CFLAGS -o "ops/+x/agy_compose_stub.+x" "ops/agy_compose_stub.c"
gcc $CFLAGS -o "ops/+x/agy_edit_key.+x" "ops/agy_edit_key.c"
gcc $CFLAGS -o "ops/+x/agy_compose_view.+x" "ops/agy_compose_view.c"
gcc $CFLAGS -o "ops/+x/agy_widget_cmds.+x" "ops/agy_widget_cmds.c"
gcc $CFLAGS -o "ops/+x/agy_scan_dir.+x" "ops/agy_scan_dir.c"

echo "--- Building agy-txt's real native browser manager (PITFALL 65 rebuild) ---"
mkdir -p manager/+x
gcc $CFLAGS -pthread -o "manager/+x/agy_browser_manager.+x" "manager/agy_browser_manager.c"

echo "--- Copying GL/RGB system binaries (§35 GL-primary) ---"
if [ -d "$WSR" ]; then
    cp "$WSR/system/chtpm_rgb_render" system/chtpm_rgb_render 2>/dev/null || true
    cp "$WSR/system/gl_mirror" system/gl_mirror 2>/dev/null || true
    chmod +x system/chtpm_rgb_render system/gl_mirror 2>/dev/null || true
    echo "copied chtpm_rgb_render + gl_mirror from wsr-pal"

    mkdir -p ops/+x
    cp "$WSR/ops/+x/emoji_gen_atlas.+x" ops/+x/emoji_gen_atlas.+x 2>/dev/null || true
    cp "$WSR/ops/+x/emoji_xtract.+x" ops/+x/emoji_xtract.+x 2>/dev/null || true
    chmod +x ops/+x/emoji_gen_atlas.+x ops/+x/emoji_xtract.+x 2>/dev/null || true
    echo "copied emoji_gen_atlas.+x + emoji_xtract.+x from wsr-pal"

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
