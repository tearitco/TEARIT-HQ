#!/bin/bash
# scripts/build.sh - compile agy-editor (house pal-native, login-signup shape)
set -e
SCRIPT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$SCRIPT_DIR"

mkdir -p ops/+x system

CFLAGS="-Wall -Wextra -O2"

echo "--- Building system processes ---"
gcc $CFLAGS "system/prisc+x.c" -o "system/prisc+x"
gcc $CFLAGS "system/keyboard_input.c" -o "system/keyboard_input"
gcc $CFLAGS "system/renderer.c" -o "system/renderer"

echo "--- Building chtpm_parser_pal ---"
gcc $CFLAGS -Wno-unused-result -Wno-stringop-truncation "system/chtpm_parser_pal.c" -o "system/chtpm_parser_pal"

echo "--- Building editor ops ---"
gcc $CFLAGS -o "ops/+x/editor_menu_input.+x" "ops/editor_menu_input.c"
gcc $CFLAGS -o "ops/+x/editor_compose_frame.+x" "ops/editor_compose_frame.c"
gcc $CFLAGS -o "ops/+x/editor_widget_cmds.+x" "ops/editor_widget_cmds.c"
gcc $CFLAGS -o "ops/+x/ledger_append.+x" "ops/ledger_append.c"

echo "--- Copying GL/RGB system binaries (§35 GL-primary — generic, no editor-specific logic) ---"
# NOTE: editor lives directly under the house root (one level up from
# here), unlike &.widgits/* projects which sit one level deeper — do
# not copy &.widgits/file-menu/scripts/build.sh's own "../.." verbatim.
WSR="$(cd "$SCRIPT_DIR/.." && pwd)/014.wsr-pal💸️📌️+2"
if [ -d "$WSR" ]; then
    cp "$WSR/system/chtpm_rgb_render" system/chtpm_rgb_render 2>/dev/null || true
    cp "$WSR/system/gl_mirror" system/gl_mirror 2>/dev/null || true
    chmod +x system/chtpm_rgb_render system/gl_mirror 2>/dev/null || true
    echo "copied chtpm_rgb_render + gl_mirror from wsr-pal"

    # Generic on-demand emoji asset generator (PITFALL 57/!.pal-2do.txt
    # 2DO 1 real fix, 2026-07-30) — chtpm_rgb_render.c's own
    # ensure_emoji_asset_generated() shells out to these two ops,
    # resolved relative to THIS project's own PRISC_PROJECT_ROOT, so
    # each project needs its own copy (matching the chtpm_rgb_render/
    # gl_mirror pattern above exactly).
    mkdir -p ops/+x
    cp "$WSR/ops/+x/emoji_gen_atlas.+x" ops/+x/emoji_gen_atlas.+x 2>/dev/null || true
    cp "$WSR/ops/+x/emoji_xtract.+x" ops/+x/emoji_xtract.+x 2>/dev/null || true
    chmod +x ops/+x/emoji_gen_atlas.+x ops/+x/emoji_xtract.+x 2>/dev/null || true
    echo "copied emoji_gen_atlas.+x + emoji_xtract.+x from wsr-pal"

    # Font glyph registry — REQUIRED for chtpm_rgb_render, project-local
    # by design, never shared/symlinked from wsr-pal directly (see
    # &.widgits/file-menu/fm-widget-fix.md's own "Missing font glyph
    # registry" root-cause writeup: without pieces/registry/fonts/ascii/
    # <code>/glyph.txt present in THIS project's own tree, every
    # character renders as invisible — all glyph pixels zero, GL window
    # comes up solid black even though the pipeline is otherwise fully
    # working). Same copy-from-wsr-pal-once, own-a-local-copy pattern
    # file-menu's own build.sh already uses.
    echo "--- Copying font glyph registry ---"
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
