#!/bin/sh
# build_sprite_phymoji_gen.sh — build the real, shared sprite-driven
# phymoji generator (2026-08-30, direct instruction: "u should make a
# script to do phymoji of all entities. save it locally in shared.
# and all new entities will use it as well"). ONE compiled binary,
# copied locally into any project's own ops/+x/ (same real "copied
# locally" convention x11_mirror.+x/emoji_gen_atlas.+x already use) -
# see sprite_phymoji_gen.c's own header comment for the full real
# reasoning (generates from an entity's own real sprite.csv, not a
# re-rasterized emoji glyph that can look completely different).
set -e
cd "$(dirname "$0")"
mkdir -p +x
CC=${CC:-gcc}
CFLAGS="-std=c11 -Wall -O2"

echo "-- sprite_phymoji_gen -> +x/sprite_phymoji_gen.+x"
$CC $CFLAGS -o +x/sprite_phymoji_gen.+x sprite_phymoji_gen.c -lm
echo "OK +x/sprite_phymoji_gen.+x"
