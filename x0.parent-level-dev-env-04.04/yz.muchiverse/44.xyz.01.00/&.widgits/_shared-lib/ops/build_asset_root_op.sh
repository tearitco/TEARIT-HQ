#!/bin/sh
# build_asset_root_op.sh - build ops/+x/asset_root_op.+x (asset folder resolver, book-stack reader phase R3a). 2026-11-02.
set -e
cd "$(dirname "$0")"
mkdir -p +x
echo "-- asset_root_op -> +x/asset_root_op.+x"
${CC:-gcc} -std=c11 -Wall -Wextra -Werror -O2 -o +x/asset_root_op.+x asset_root_op.c
echo "OK +x/asset_root_op.+x"
