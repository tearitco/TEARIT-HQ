#!/bin/bash
# scripts/build.sh - compile HORN_CHAT, warning-free.
#
# Sources of the two big system files follow the current house convention
# (_shared-lib/README.md, "SHARED-SOURCE-COMPILE-IN-PLACE.md",
# 2026-09-09): compile the canonical file IN PLACE via -I, never copy it
# into this project. Earlier copies drifted, which is exactly why the
# convention was changed. Only the built binary lands here.
#
# system/keyboard_input.c and system/renderer.c are this project's own
# local copies - there is no canonical version of either in _shared-lib,
# so there is nothing to compile in place.
set -e

# Walk up to the house root that owns &.widgits/_shared-lib. Same
# discovery wsr-pal's own scripts/build.sh uses.
_pcd="$(cd "$(dirname "$0")/.." && pwd)"
while [ "$_pcd" != "/" ] && [ ! -d "$_pcd/&.widgits/_shared-lib" ]; do
    _pcd="$(dirname "$_pcd")"
done
SHARED="$_pcd/&.widgits/_shared-lib"

if [ ! -d "$SHARED/system" ]; then
    echo "build: cannot find &.widgits/_shared-lib above this project" >&2
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$SCRIPT_DIR"

mkdir -p ops/+x pieces/horn pieces/display pieces/keyboard pieces/system \
         pieces/os pieces/apps/player_app/manager chats/HORN_SESSIONS

CFLAGS="-Wall -Wextra -O2 -I$SHARED"

echo "--- system processes ---"
# prisc+x: the interpreter that runs pal/horn_main_loop.pal.
gcc $CFLAGS "$SHARED/system/prisc+x.c" -o system/prisc+x

gcc $CFLAGS system/keyboard_input.c -o system/keyboard_input
gcc $CFLAGS system/renderer.c -o system/renderer

# chtpm_parser_pal: the persistent layout process. -Wno-unused-result and
# -Wno-stringop-truncation are required for THIS file only and are not
# cosmetic - wsr-pal's build.sh carries the same note after getting them
# to zero warnings here.
gcc $CFLAGS -Wno-unused-result -Wno-stringop-truncation \
    "$SHARED/system/chtpm_parser_pal.c" -o system/chtpm_parser_pal

echo "--- ops ---"
# horn_turn execs these by absolute path at runtime, so they must exist
# before the first turn rather than being built on demand.
for src in ops/*.c; do
    name="$(basename "$src" .c)"
    echo "  $name"
    # halo_chat_describe makes ids with libuuid; without -luuid the link failed and, under set -e, stopped the build before the HORN
    # ops after it (alphabetical) were compiled (found 2026-10-06 landing HALO, Q001).
    extra=""
    case "$name" in halo_chat_describe|irl_bootstrap_fsm) extra="-luuid" ;; esac
    gcc $CFLAGS "$src" -o "ops/+x/$name.+x" $extra
done

# Drop binaries whose source is gone. A stale horn_chat_openrouter.+x sat
# in ops/+x/ after the transport was renamed to horn_chat_backend, and
# nothing referenced it - it just made ops/ lie about what this project
# actually runs.
for bin in ops/+x/*.+x; do
    [ -e "$bin" ] || continue
    name="$(basename "$bin" .+x)"
    if [ ! -f "ops/$name.c" ]; then
        echo "  removing stale $bin (no ops/$name.c)"
        rm -f "$bin"
    fi
done

echo "--- done ---"
ls -l system/prisc+x system/chtpm_parser_pal ops/+x/