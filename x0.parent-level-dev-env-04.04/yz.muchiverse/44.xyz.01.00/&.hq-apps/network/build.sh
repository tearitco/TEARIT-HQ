#!/bin/bash
# build.sh - builds the network cell's real apps.
#   network_browser_manager.+x  - real manager (fetch + simple HTML
#     extraction), publishes #.desktop/network_browser_*.state.txt.
#     Shared by BOTH the real, current window (button.sh -> shared
#     khtpm_core_render.+x's generic default path, <module>-launches
#     this same binary) and network_browser_render_ascii.c's own CLI
#     mirror (khtpm-hq-app-cli.sh) - this file has zero Elem/X11
#     dependency, doesn't need any of the shared khtpm_*.c copies.
#
# REAL FIX 2026-09-01 - cli_io_window.c, network_browser_render.c, and
# network_browser_render_ascii.c's own X11 sibling were the OLD, now-
# retired standalone renderer stack (direct live report: "so ur telling
# me we havent wired up the real browser to toolbar yet? ... clean up
# the old one. delete it. its in a git branch anyways") - deleted, not
# built here anymore. See git history (branch consolidate-taskbar-
# strip and earlier) if any of them are ever needed again.
# Binaries live in +x/ (git-ignored).
set -u
SDIR="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$SDIR/+x" "$SDIR/ops/+x"
CC=${CC:-gcc}
JSDIR="$SDIR/../js"

echo "-- network_browser_manager -> +x/network_browser_manager.+x"
$CC -std=c11 -Wall -O2 -o "$SDIR/+x/network_browser_manager.+x" "$SDIR/network_browser_manager.c" && echo "OK network_browser_manager" || exit 1

# 2026-09-02 (merge-test pass): the two new ops the manager shells out
# to for JS eval and media->sprite conversion. Duktape is a real,
# vendored (not git-submoduled) third-party single-file amalgamation,
# same convention as stb_image.h already used elsewhere in the house -
# both live in the shared &.hq-apps/js/ dir, not copied per-op.
echo "-- nb_js_eval -> ops/+x/nb_js_eval.+x"
$CC -std=c11 -Wall -O2 -I"$JSDIR" -o "$SDIR/ops/+x/nb_js_eval.+x" "$SDIR/ops/nb_js_eval.c" "$JSDIR/duktape.c" -lm && echo "OK nb_js_eval" || exit 1

echo "-- nb_media_to_sprite -> ops/+x/nb_media_to_sprite.+x"
$CC -std=c11 -Wall -O2 -I"$JSDIR" -o "$SDIR/ops/+x/nb_media_to_sprite.+x" "$SDIR/ops/nb_media_to_sprite.c" -lm && echo "OK nb_media_to_sprite" || exit 1
