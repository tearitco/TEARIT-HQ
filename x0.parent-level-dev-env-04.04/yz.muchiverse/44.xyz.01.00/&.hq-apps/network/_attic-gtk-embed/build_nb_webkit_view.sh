#!/bin/sh
# build_nb_webkit_view.sh - optional. Compiles the WebKitGTK escape-hatch
# window ONLY if the dev packages are present; a no-op (exit 0) otherwise
# so the main build.sh can call it unconditionally.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
if ! pkg-config --exists webkit2gtk-4.0 gtk+-3.0 x11 2>/dev/null; then
    echo "nb_webkit_view: webkit2gtk-4.0 / gtk+-3.0 dev not found - skipping (hybrid hook falls back to xdg-open/firefox)"
    exit 0
fi
mkdir -p "$HERE/+x"
gcc -std=c11 -Wall -Wextra -O2 -o "$HERE/+x/nb_webkit_view.+x" "$HERE/nb_webkit_view.c" \
    $(pkg-config --cflags --libs webkit2gtk-4.0 gtk+-3.0 x11)
echo "OK $HERE/+x/nb_webkit_view.+x"
