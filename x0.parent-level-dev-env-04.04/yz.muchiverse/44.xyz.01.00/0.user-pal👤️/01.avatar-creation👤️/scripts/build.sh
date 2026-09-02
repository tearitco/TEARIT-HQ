#!/bin/bash
# Build avatar-creation system + ops
set -e
SCRIPT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$SCRIPT_DIR"
mkdir -p ops/+x system

CFLAGS="-Wall -Wextra -O2"

echo "--- system ---"
gcc $CFLAGS "system/prisc+x.c" -o "system/prisc+x"
gcc $CFLAGS "system/keyboard_input.c" -o "system/keyboard_input"
gcc $CFLAGS "system/renderer.c" -o "system/renderer"
gcc $CFLAGS -Wno-unused-result -Wno-stringop-truncation "system/chtpm_parser_pal.c" -o "system/chtpm_parser_pal"

# emoji + desktop window (from muchi-pals sources)
FT_CFLAGS="$(pkg-config --cflags freetype2 2>/dev/null)"
FT_LIBS="$(pkg-config --libs freetype2 2>/dev/null)"
if [ -f system/emoji_gen_atlas.c ]; then
  gcc $CFLAGS -I system/lib $FT_CFLAGS system/emoji_gen_atlas.c -o system/emoji_gen_atlas $FT_LIBS -lm 2>/dev/null \
    && echo "OK   emoji_gen_atlas" || echo "SKIP emoji_gen_atlas"
fi
if [ -f system/emoji_xtract.c ]; then
  gcc $CFLAGS -I system/lib system/emoji_xtract.c -o system/emoji_xtract -lm 2>/dev/null \
    && echo "OK   emoji_xtract" || echo "SKIP emoji_xtract"
fi
if [ -f system/avatar_window.c ]; then
  gcc $CFLAGS -I system/lib system/avatar_window.c -o system/avatar_window \
    -lGL -lX11 -lXext -lm 2>/dev/null \
    && echo "OK   avatar_window" || echo "SKIP avatar_window (no X/GL?)"
fi
if [ -f system/character_preview.c ]; then
  gcc $CFLAGS system/character_preview.c -o system/character_preview \
    -lGL -lGLU -lglut -lm 2>/dev/null \
    && echo "OK   character_preview" || echo "SKIP character_preview (needs GLUT/GL)"
fi

echo "--- ops ---"
for op in generate_clone claim_tokens buy_clone cycle_dna apply_name_age \
          toggle_sleep open_avatar_window open_character_preview \
          ensure_user_identity hydrate_avatars make_avatar_sprite \
          avatar_menu_input avatar_compose_frame; do
  gcc $CFLAGS -o "ops/+x/${op}.+x" "ops/${op}.c" && echo "OK   $op" || echo "FAIL $op"
done
echo "build ok"
