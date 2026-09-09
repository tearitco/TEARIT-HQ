#!/bin/sh
# vendor-into.sh <target_ops_dir> [more_dirs...]
#
# Copy the house-authored shared source (khtpm_css_parser.c/.h,
# khtpm_render_core.c, khtpm_draw_core.c) from this canonical directory
# into one or more consumer `ops/` dirs, so that a SHIPPED/PACKAGED tree
# is self-contained and can be built with no `-I &.widgits/_shared-lib`.
#
# This is the ONLY blessed place that copies these files. It is for the
# install / tree-assembly step (xyz-installer-dev,
# 04.harnecient-fresh-install-design.md §5.1) — NEVER call it from a
# build_*.sh. Dev builds compile the canonical in place via `-I`
# (SHARED-SOURCE-COMPILE-IN-PLACE.md); the vendored copies exist only in
# a packaged artifact, not in the git working tree (they are
# .gitignored: **/ops/khtpm_*).
#
# Idempotent. stb_image_write.h is intentionally NOT handled here — it
# is a frozen vendored third-party header each consumer already carries.
set -eu

HERE="$(cd "$(dirname "$0")" && pwd)"
FILES="khtpm_css_parser.c khtpm_css_parser.h khtpm_render_core.c khtpm_draw_core.c"

if [ "$#" -lt 1 ]; then
    echo "usage: vendor-into.sh <target_ops_dir> [more_dirs...]" >&2
    exit 2
fi

for dst in "$@"; do
    if [ ! -d "$dst" ]; then
        echo "vendor-into: not a directory: $dst" >&2
        exit 1
    fi
    for f in $FILES; do
        cp "$HERE/$f" "$dst/$f"
    done
    echo "vendor-into: $FILES -> $dst"
done
