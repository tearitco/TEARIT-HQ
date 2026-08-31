#!/bin/sh
# build_khtpm_strip.sh — build the two-process strip architecture
# (khtpm_taskbar_manager_main.c manager driver + khtpm_strip_parser.c
# outer parser), per khtpm-strip-parser-design.md.
#
# PRODUCTION BINARY NAMES (2026-08-11): legacy tp_taskbar.c has been
# retired (archived to
# *.monads/*.livedesk-taskbar/ops/LEGACY-ARCHIVE-20260811.zip, originals
# deleted — direct instruction: "id like to deprecate the old toolbar
# system now"). This khtpm pair is now the real, only taskbar — dropped
# the "_test" suffix these binaries carried through the whole build-out
# session, now that there's no live legacy binary left to avoid
# clobbering.
set -e
cd "$(dirname "$0")"
mkdir -p +x
CC=${CC:-gcc}
CFLAGS="-std=c11 -Wall -O2"

# macOS leg (2026-08-22): XQuartz owns X11/Xft under /opt/X11 — brew's
# pkg-config doesn't search there by default, and clang won't find
# Xlib headers/libs without explicit -I/-L. Prebuilt emoji-helper
# binaries are Linux ELF, so on Darwin they build from wsr-pal source
# instead of being copied. Guarded: on Linux every added var stays
# empty and behavior is unchanged.
OS_TYPE="$(uname -s)"
X11_FLAGS=""
FT_CFLAGS=""
FT_LIBS=""
if [ "$OS_TYPE" = "Darwin" ]; then
    PKG_CONFIG_PATH="/opt/X11/lib/pkgconfig:/usr/local/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
    export PKG_CONFIG_PATH
    X11_FLAGS="-I/opt/X11/include -L/opt/X11/lib"
    if command -v pkg-config >/dev/null 2>&1; then
        FT_CFLAGS=$(pkg-config --cflags freetype2)
        FT_LIBS=$(pkg-config --libs freetype2)
    fi
fi

# Sync shared files from the single canonical source (2026-08-12
# dedup pass - see &.widgits/_shared-lib/README.md for why this is a
# build-time copy, not a runtime shared include path).
SHARED="$(cd "$(dirname "$0")/../../../&.widgits/_shared-lib" && pwd)"
cp "$SHARED/khtpm_css_parser.c" khtpm_css_parser.c
cp "$SHARED/khtpm_css_parser.h" khtpm_css_parser.h

echo "-- khtpm manager driver (pure logic, no Xlib) -> +x/khtpm_taskbar_manager_main.+x"
$CC $CFLAGS -o +x/khtpm_taskbar_manager_main.+x \
  khtpm_taskbar_manager_main.c khtpm_taskbar_manager.c

echo "-- khtpm strip parser (Xlib + layout engine + hit-testing + manager fork/exec) -> +x/khtpm_strip_parser.+x"
# REAL FIX 2026-08-13: now links Xft (same flags as build_db_hq.sh) -
# khtpm_strip_parser.c grew real XftDrawStringUtf8-based CJK/UTF-8
# text rendering this session (see that file's own header comment on
# its Xft include), replacing plain XDrawString which could only
# render Latin-1 correctly.
$CC $CFLAGS $X11_FLAGS $(pkg-config --cflags xft) -o +x/khtpm_strip_parser.+x \
  khtpm_strip_parser.c khtpm_strip_layout.c khtpm_taskbar_manager.c \
  -lX11 $(pkg-config --libs xft) -lm

# 2026-08-14 consolidation: the livedesk entity renderer + its helper
# set moved OUT of &.widgits/tile-picker into this runtime folder (the
# entity window is a livedesk-taskbar concern, not a tile-picker one).
# Built here now so the whole runtime is one folder + one build script.
echo "-- entity renderer tp_desktop_window_rgb.c -> +x/tp_desktop_window_rgb.+x"
$CC $CFLAGS $X11_FLAGS -o +x/tp_desktop_window_rgb.+x tp_desktop_window_rgb.c -lX11 -lXext -lm

echo "-- emoji->sprite helper tp_asset_to_sprite.c -> +x/tp_asset_to_sprite.+x"
$CC $CFLAGS -o +x/tp_asset_to_sprite.+x tp_asset_to_sprite.c -lm

echo "-- emoji atlas helpers emoji_gen_atlas/emoji_xtract (copied from wsr-pal)"
# emoji_gen_atlas.+x + emoji_xtract.+x ship as prebuilt binaries from the
# 014.wsr-pal toolchain (same source every other widget copies them from)
# - the entity calls them via ops_dir at runtime, so they must sit in the
# same +x/ dir as the entity binary.
WSR="$(cd "$(dirname "$0")/../../../014.wsr-pal💸️📌️+2" 2>/dev/null && pwd)"
for t in emoji_gen_atlas emoji_xtract; do
    if [ ! -x "+x/$t.+x" ]; then
        if [ "$OS_TYPE" = "Darwin" ]; then
            # macOS: prebuilt copies are Linux ELF — compile from source.
            # emoji_gen_atlas needs freetype; emoji_xtract doesn't.
            if [ -n "$WSR" ] && [ -f "$WSR/ops/$t.c" ]; then
                if $CC $CFLAGS -I"$WSR/ops" $FT_CFLAGS -o "+x/$t.+x" "$WSR/ops/$t.c" $FT_LIBS -lm 2>/dev/null; then
                    echo "    $t.+x built from wsr-pal source"
                else
                    echo "WARN: $t.+x failed to build from source"
                fi
            else
                echo "WARN: +x/$t.+x missing (no wsr-pal source to build)"
            fi
        elif [ -n "$WSR" ] && [ -x "$WSR/ops/+x/$t.+x" ]; then
            cp "$WSR/ops/+x/$t.+x" "+x/$t.+x"
            chmod +x "+x/$t.+x"
            echo "    $t.+x copied from wsr-pal"
        else
            echo "WARN: +x/$t.+x missing (wsr-pal copy unavailable)"
        fi
    fi
done

echo "-- real, shared sprite-driven phymoji generator (build in shared, copy binary)"
# Direct instruction 2026-08-30 ("make a script to do phymoji of all
# entities. save it locally in shared. and all new entities will use
# it as well") - one real compiled binary, built from the shared
# source, copied locally same as x11_mirror.+x/emoji_gen_atlas.+x
# already are - tp_desktop_window_rgb.c's own real load_entity_phymoji()
# shells out to this copy at runtime (ops_dir-relative, same real
# lookup pattern apply_asset_override() already uses).
if [ -f "$SHARED/ops/build_sprite_phymoji_gen.sh" ]; then
    (cd "$SHARED/ops" && bash build_sprite_phymoji_gen.sh >/dev/null 2>&1) || echo "WARN: sprite_phymoji_gen.+x failed to build"
    if [ -x "$SHARED/ops/+x/sprite_phymoji_gen.+x" ]; then
        cp "$SHARED/ops/+x/sprite_phymoji_gen.+x" "+x/sprite_phymoji_gen.+x"
        chmod +x "+x/sprite_phymoji_gen.+x"
    fi
fi

echo "-- window-position/range-grid helper tp_range_grid.c -> +x/tp_range_grid.+x"
$CC $CFLAGS $X11_FLAGS -o +x/tp_range_grid.+x tp_range_grid.c -lX11 -lXext

# 2026-08-18: taskbar's terminal ASCII mirror (HQ menu "cli" row) - two
# binaries, matching TPMOS's real renderer.c/keyboard_input.c split
# (never combined - see khtpm_strip_render_ascii.c's own header comment
# for the real \r\n/staircase bug this split fixes).
echo "-- taskbar ASCII renderer (no termios) -> +x/khtpm_strip_render_ascii.+x"
$CC $CFLAGS -o +x/khtpm_strip_render_ascii.+x khtpm_strip_render_ascii.c

echo "-- taskbar ASCII keyboard input (raw termios only, never prints) -> +x/khtpm_strip_keyboard_ascii.+x"
$CC $CFLAGS -o +x/khtpm_strip_keyboard_ascii.+x khtpm_strip_keyboard_ascii.c

echo "OK +x/khtpm_taskbar_manager_main.+x and +x/khtpm_strip_parser.+x (plus entity renderer + helpers)"
