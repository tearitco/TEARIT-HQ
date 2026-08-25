#!/bin/bash
# scripts/build.sh - build file-menu widget ops
set -e
SCRIPT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$SCRIPT_DIR"

mkdir -p ops/+x

CFLAGS="-Wall -Wextra -O2"

echo "--- Building file-menu ops ---"
gcc $CFLAGS -o "ops/+x/fm_set_focus.+x" "ops/fm_set_focus.c"
gcc $CFLAGS -o "ops/+x/fm_enqueue_cmd.+x" "ops/fm_enqueue_cmd.c"
gcc $CFLAGS -o "ops/+x/fm_compose_frame.+x" "ops/fm_compose_frame.c"
gcc $CFLAGS -o "ops/+x/fm_menu_input.+x" "ops/fm_menu_input.c"
gcc $CFLAGS -o "ops/+x/fm_scan_dir.+x" "ops/fm_scan_dir.c"
gcc $CFLAGS -o "ops/+x/ledger_append.+x" "ops/ledger_append.c"
gcc $CFLAGS -o "ops/+x/ledger_peers.+x" "ops/ledger_peers.c"

echo "--- Copying system binaries (local copies for dev) ---"
WSR="$(cd "$SCRIPT_DIR/../.." && pwd)/014.wsr-pal💸️📌️+2"
if [ -d "$WSR" ]; then
    mkdir -p system
    cp "$WSR/system/prisc+x" system/prisc+x 2>/dev/null || true
    cp "$WSR/system/chtpm_parser_pal" system/chtpm_parser_pal 2>/dev/null || true
    cp "$WSR/system/chtpm_rgb_render" system/chtpm_rgb_render 2>/dev/null || true
    cp "$WSR/system/keyboard_input" system/keyboard_input 2>/dev/null || true
    cp "$WSR/system/renderer" system/renderer 2>/dev/null || true
    cp "$WSR/system/gl_mirror" system/gl_mirror 2>/dev/null || true
    cp "$WSR/default_op.txt" system/default_op.txt 2>/dev/null || true
    chmod +x system/*
    echo "copied from wsr-pal"

    # Generic on-demand emoji asset generator (PITFALL 57/!.pal-2do.txt
    # 2DO 1 real fix, 2026-07-30) — see 102.editor-📄️00.00/scripts/
    # build.sh's own identical addition for the full rationale.
    mkdir -p ops/+x
    cp "$WSR/ops/+x/emoji_gen_atlas.+x" ops/+x/emoji_gen_atlas.+x 2>/dev/null || true
    cp "$WSR/ops/+x/emoji_xtract.+x" ops/+x/emoji_xtract.+x 2>/dev/null || true
    chmod +x ops/+x/emoji_gen_atlas.+x ops/+x/emoji_xtract.+x 2>/dev/null || true
    echo "copied emoji_gen_atlas.+x + emoji_xtract.+x from wsr-pal"

    # Copy font glyph registry (local to this widget)
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
    echo "WARN: wsr-pal not found, system binaries not linked"
fi

echo "build ok"
