#!/bin/sh
# build_views_to_voxel.sh - builds views_to_voxel into +x/ (visual hull of three orthographic pictures -> engine model.txt; libpng).
set -e
cd "$(dirname "$0")"
mkdir -p +x
${CC:-gcc} -std=gnu11 -Wall -Wextra -O2 -o +x/views_to_voxel.+x views_to_voxel.c $(pkg-config --cflags --libs libpng 2>/dev/null || echo -lpng) -lm
echo "OK +x/views_to_voxel.+x"
