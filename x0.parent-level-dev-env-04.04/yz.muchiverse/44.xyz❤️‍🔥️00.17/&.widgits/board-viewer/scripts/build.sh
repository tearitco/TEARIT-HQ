#!/bin/bash
# scripts/build.sh - build board-viewer widget ops. Modeled directly on
# &.widgits/file-menu/scripts/build.sh (the house's own real, proven
# widget build script) - real code COPIES binaries from wsr-pal, not
# symlinks (WIDGIT_BIBLE.md's own "Symlink Sources" table describes
# symlinking, but the actual file-menu build.sh copies - trusting the
# real code over the doc, same principle applied throughout this
# session's own research).
set -e
SCRIPT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$SCRIPT_DIR"

mkdir -p ops/+x

CFLAGS="-Wall -Wextra -O2"

echo "--- Building board-viewer ops ---"
gcc $CFLAGS -o "ops/+x/bv_compose_frame.+x" "ops/bv_compose_frame.c"
gcc $CFLAGS -o "ops/+x/bv_menu_input.+x" "ops/bv_menu_input.c" -lm
gcc $CFLAGS -fopenmp -o "ops/+x/bv_render_3d.+x" "ops/bv_render_3d.c" -lm
# Real house-wide widget discovery/registration (Phase 0 follow-up,
# 2026-08-03 - see ops/ledger_append.c's own header comment: ported
# from file-menu's own proven mechanism, "default" xyzfs fallback so
# it works with no active login).
gcc $CFLAGS -o "ops/+x/ledger_append.+x" "ops/ledger_append.c"
gcc $CFLAGS -o "ops/+x/ledger_peers.+x" "ops/ledger_peers.c"
# REAL, NEW 2026-08-04, direct instruction (tile-picker's own "^" drag-
# anywhere mode, see &.widgits/tile-picker/TILE_PICKER_DESIGN.md §4):
# GLUT doesn't set _NET_WM_PID and this house's WM doesn't synthesize
# one (confirmed via direct xprop check on a live gl_mirror window) -
# tag this widget's own window with its real PID right after spawn (see
# button.sh's own call site) so tile-picker's ledger_peers+PID lookup
# can find THIS specific board-viewer instance's window unambiguously,
# even with several identically-titled "wsr-pal RGB mirror" windows open
# at once. Ported from tile-picker's own ops/tp_set_wm_pid.c.
gcc $CFLAGS -o "ops/+x/bv_set_wm_pid.+x" "ops/bv_set_wm_pid.c" -lX11

echo "--- Copying system binaries (local copies, per file-menu's own real pattern) ---"
WSR="$(cd "$SCRIPT_DIR/../.." && pwd)/014.wsr-pal💸️📌️+2"
MUT="$(cd "$SCRIPT_DIR/../.." && pwd)/101.mutaclsym🧟‍♂️️+18.01"
if [ -d "$WSR" ]; then
    mkdir -p system
    cp "$WSR/system/prisc+x" system/prisc+x 2>/dev/null || true
    cp "$WSR/system/chtpm_parser_pal" system/chtpm_parser_pal 2>/dev/null || true
    # REVERTED 2026-08-02 (real bug: swapping to mutaclysm's own fork
    # for MAP3D_MARKER support silently broke 2D emoji-mode rendering -
    # mutaclysm's copy has ZERO on-demand emoji generation, it only
    # recognizes emoji hand-curated from ITS OWN registry files
    # (pieces/registry/terrain/terrain_types.txt etc), which don't
    # exist in this project at all, so every UTF-8 emoji byte this
    # project's own bv_compose_frame.c emits rendered as nothing).
    # CORRECTED FINDING: wsr-pal's OWN copy already has BOTH features -
    # the earlier research claiming "neither copy has MAP3D_MARKER" was
    # simply wrong (confirmed live: grep for blit_overlay/MAP3D in
    # wsr-pal's own chtpm_rgb_render.c finds it, fully present).
    cp "$WSR/system/chtpm_rgb_render" system/chtpm_rgb_render 2>/dev/null || true
    cp "$WSR/system/keyboard_input" system/keyboard_input 2>/dev/null || true
    cp "$WSR/system/renderer" system/renderer 2>/dev/null || true
    # gl_mirror MUST be the 014.wsr-pal version (has real interact_relay
    # forwarding) - NEVER mutaclysm's own copy (mirror-only, no key
    # forward) - see @.apps/BOARD_WIDGET_ARCHITECTURE.md §1a.
    cp "$WSR/system/gl_mirror" system/gl_mirror 2>/dev/null || true
    chmod +x system/* 2>/dev/null || true
    echo "copied from wsr-pal"

    echo "--- Copying real generic emoji-generation ops (real FreeType +" \
         "NotoColorEmoji.ttf, on-demand voxel asset gen - see" \
         "system/chtpm_rgb_render.c's own 'GENERIC ON-DEMAND EMOJI" \
         "GENERATION' comment) - required for emoji_mode default-on ---"
    mkdir -p ops/+x
    cp "$WSR/ops/+x/emoji_gen_atlas.+x" ops/+x/emoji_gen_atlas.+x 2>/dev/null || true
    cp "$WSR/ops/+x/emoji_xtract.+x" ops/+x/emoji_xtract.+x 2>/dev/null || true
    chmod +x ops/+x/emoji_gen_atlas.+x ops/+x/emoji_xtract.+x 2>/dev/null || true
    mkdir -p pieces/registry/emoji_assets

    echo "--- Copying font glyph registry (real local copy, never symlinked" \
         "- WIDGIT_BIBLE.md §2.5) ---"
    mkdir -p "$SCRIPT_DIR/pieces/registry/fonts/ascii"
    for dir in "$WSR/pieces/registry/fonts/ascii/"*/; do
        [ -d "$dir" ] || continue
        code="$(basename "$dir")"
        mkdir -p "$SCRIPT_DIR/pieces/registry/fonts/ascii/$code"
        cp "$dir/glyph.txt" "$SCRIPT_DIR/pieces/registry/fonts/ascii/$code/glyph.txt"
    done
    echo "glyphs: $(ls "$SCRIPT_DIR/pieces/registry/fonts/ascii/" | wc -l)"
else
    echo "WARN: wsr-pal not found at $WSR, system binaries not linked"
fi

echo "build ok"
