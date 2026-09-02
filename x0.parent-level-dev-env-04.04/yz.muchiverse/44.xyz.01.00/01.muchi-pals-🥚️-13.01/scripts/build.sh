#!/bin/sh
# Builds every binary independently - no shared object files, no shared
# headers, matching cdda-tpm-std-fast.txt sec. 1 (same convention as
# mutaclsym/scripts/build.sh). Each translation unit is compiled and
# linked on its own line.
set -e
cd "$(dirname "$0")/.."

CC=${CC:-gcc}
CFLAGS="-std=c11 -Wall -Wextra -O2"

# egg_window.c is X11/GLX on Linux/Mac -- present natively on Linux
# (headers/libs on the default search path) but only via XQuartz on Mac,
# which installs to /opt/X11 instead of anywhere the compiler/linker look
# by default. On Windows it's a from-scratch native Win32 + WGL backend
# (see the #ifdef _WIN32 half of egg_window.c) instead of X11 entirely, so
# it needs Windows' own GDI/OpenGL/User32 import libs, not X11's, and no
# freetype/X11 headers are assumed present. Autodetect instead of
# hardcoding one platform's paths.
case "$(uname -s)" in
    Darwin)
        X11_CFLAGS="-I/opt/X11/include"
        X11_LIBS="-L/opt/X11/lib -lX11 -lXext -lGL -lm"
        ;;
    MINGW*|MSYS*|CYGWIN*)
        X11_CFLAGS=""
        X11_LIBS="-lopengl32 -lgdi32 -luser32 -lm"
        # emoji_gen_atlas.c uses a wmain() entry point on Windows so it
        # receives the real UTF-16 command line instead of one already
        # mangled through the system's ANSI codepage (verified: an emoji
        # argument arrived as literal '?' bytes without this) - -municode
        # tells the linker to start from wmainCRTStartup instead of the
        # ANSI mainCRTStartup, which is the only thing that actually looks
        # for a wmain symbol.
        WIN_UNICODE_FLAGS="-municode"
        ;;
    *)
        X11_CFLAGS=""
        X11_LIBS="-lX11 -lXext -lGL -lm"
        WIN_UNICODE_FLAGS=""
        ;;
esac

echo "-- prisc+x (VM)"
$CC $CFLAGS -o system/prisc+x system/prisc+x.c

echo "-- emoji_gen_atlas (FreeType color-bitmap emoji -> PNG)"
$CC $CFLAGS $WIN_UNICODE_FLAGS -o system/emoji_gen_atlas system/emoji_gen_atlas.c $(pkg-config --cflags --libs freetype2) -lm

echo "-- emoji_xtract (PNG -> plain-text pixel CSV)"
$CC $CFLAGS -o system/emoji_xtract system/emoji_xtract.c -lm

echo "-- egg_window (shaped GL window, clipped to the pet's own sprite silhouette)"
$CC $CFLAGS $X11_CFLAGS -o system/egg_window system/egg_window.c $X11_LIBS

echo "-- keyboard_input (raw termios, no ncurses)"
$CC $CFLAGS -o system/keyboard_input system/keyboard_input.c

echo "-- renderer (plain stdout, no ncurses)"
$CC $CFLAGS -o system/renderer system/renderer.c

echo "-- chtpm_parser_pal (PERSISTENT process, not a one-shot op - local"
echo "   copy, not synced from shared-ops - see that file's own header"
echo "   comment. -Wno-unused-result -Wno-stringop-truncation are"
echo "   REQUIRED on this one file - confirmed via a real test build"
echo "   this gets to zero warnings.)"
$CC $CFLAGS -Wno-unused-result -Wno-stringop-truncation -o system/chtpm_parser_pal system/chtpm_parser_pal.c

echo "-- ops"
$CC $CFLAGS -o ops/+x/generate_egg.+x ops/generate_egg.c
$CC $CFLAGS -o ops/+x/claim_tokens.+x ops/claim_tokens.c
$CC $CFLAGS -o ops/+x/coin_flip.+x ops/coin_flip.c
$CC $CFLAGS -o ops/+x/buy_egg.+x ops/buy_egg.c
$CC $CFLAGS -o ops/+x/hatch_egg.+x ops/hatch_egg.c
$CC $CFLAGS -o ops/+x/menu_input.+x ops/menu_input.c
$CC $CFLAGS -o ops/+x/compose_menu.+x ops/compose_menu.c
$CC $CFLAGS -o ops/+x/tick_pets.+x ops/tick_pets.c
$CC $CFLAGS -o ops/+x/feed_pet.+x ops/feed_pet.c
$CC $CFLAGS -o ops/+x/clean_pet.+x ops/clean_pet.c
$CC $CFLAGS -o ops/+x/toggle_sleep.+x ops/toggle_sleep.c
$CC $CFLAGS -o ops/+x/train_pet.+x ops/train_pet.c
$CC $CFLAGS -o ops/+x/export_card.+x ops/export_card.c
$CC $CFLAGS -o ops/+x/destroy_card.+x ops/destroy_card.c
$CC $CFLAGS -o ops/+x/list_processes.+x ops/list_processes.c
$CC $CFLAGS -o ops/+x/muchi_menu_input.+x ops/muchi_menu_input.c
$CC $CFLAGS -o ops/+x/muchi_compose_frame.+x ops/muchi_compose_frame.c

echo "build ok"
