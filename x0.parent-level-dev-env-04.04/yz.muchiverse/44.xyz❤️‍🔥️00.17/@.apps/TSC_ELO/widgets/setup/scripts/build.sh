#!/bin/bash
# scripts/build.sh - build the Match Setup WIDGIT's ops + copy system
# binaries as LOCAL COPIES (real code over docs - same principle as
# &.widgits/board-viewer/scripts/build.sh, which itself follows
# &.widgits/file-menu/scripts/build.sh's own proven pattern).
set -e
SCRIPT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$SCRIPT_DIR"

mkdir -p ops/+x

CFLAGS="-Wall -Wextra -O2"

echo "--- Building setup widget ops ---"
gcc $CFLAGS -o "ops/+x/setup_set_focus.+x" "ops/setup_set_focus.c"
gcc $CFLAGS -o "ops/+x/setup_enqueue_cmd.+x" "ops/setup_enqueue_cmd.c"
gcc $CFLAGS -o "ops/+x/setup_menu_input.+x" "ops/setup_menu_input.c"
gcc $CFLAGS -o "ops/+x/setup_compose_frame.+x" "ops/setup_compose_frame.c"
gcc $CFLAGS -o "ops/+x/ledger_append.+x" "ops/ledger_append.c"

echo "--- Copying system binaries (local copies) ---"
HOUSE="$(cd "$SCRIPT_DIR/../../../.." && pwd)"
WSR="$HOUSE/014.wsr-pal💸️📌️+2"
MUT="$HOUSE/101.mutaclsym🧟‍♂️️+18.01"
if [ -d "$WSR" ]; then
    mkdir -p system
    cp "$WSR/system/prisc+x" system/prisc+x 2>/dev/null || true
    cp "$WSR/system/chtpm_parser_pal" system/chtpm_parser_pal 2>/dev/null || true
    # chtpm_rgb_render: mutaclysm's own fork when available (has the
    # overlay/MAP3D machinery board-viewer needs; for this text-only
    # widget the generic wsr-pal copy would also do, so fall back).
    if [ -d "$MUT" ]; then
        cp "$MUT/system/chtpm_rgb_render" system/chtpm_rgb_render 2>/dev/null || true
    else
        cp "$WSR/system/chtpm_rgb_render" system/chtpm_rgb_render 2>/dev/null || true
    fi
    cp "$WSR/system/keyboard_input" system/keyboard_input 2>/dev/null || true
    cp "$WSR/system/renderer" system/renderer 2>/dev/null || true
    # gl_mirror MUST be the 014.wsr-pal version (real interact_relay
    # forwarding) - NEVER mutaclysm's own mirror-only copy. Compiled from
    # wsr-pal's SOURCE here (not copied) so it also carries the optional
    # GL_MIRROR_X/GL_MIRROR_Y window-placement support that lets a
    # WIDGIT's own GL window open BESIDE the host's instead of on top of
    # it (live-observed overlap in TSC_ELO: two glut windows, same spot).
    if [ -f "$WSR/system/gl_mirror.c" ]; then
        if gcc -Wall -Wextra -O2 -o "system/gl_mirror" "$WSR/system/gl_mirror.c" \
            -lglut -lGL -lGLU -lX11 2>/tmp/tsc_widget_gl_mirror_build.log; then
            echo "    gl_mirror: built ok (wsr-pal source, GL_MIRROR_X/Y support)"
        else
            echo "    gl_mirror: source build failed, copying binary instead"
            cat /tmp/tsc_widget_gl_mirror_build.log >&2
            cp "$WSR/system/gl_mirror" system/gl_mirror 2>/dev/null || true
            rm -f /tmp/tsc_widget_gl_mirror_build.log
        fi
    else
        cp "$WSR/system/gl_mirror" system/gl_mirror 2>/dev/null || true
    fi
    chmod +x system/* 2>/dev/null || true

    echo "--- Copying font glyph registry (real local copy) ---"
    mkdir -p "$SCRIPT_DIR/pieces/registry/fonts/ascii"
    for dir in "$WSR/pieces/registry/fonts/ascii/"*/; do
        [ -d "$dir" ] || continue
        code="$(basename "$dir")"
        mkdir -p "$SCRIPT_DIR/pieces/registry/fonts/ascii/$code"
        cp "$dir/glyph.txt" "$SCRIPT_DIR/pieces/registry/fonts/ascii/$code/glyph.txt"
    done
    echo "glyphs: $(ls "$SCRIPT_DIR/pieces/registry/fonts/ascii/" | wc -l)"
else
    echo "WARN: wsr-pal not found at $WSR, system binaries not copied"
fi

echo "build ok"
