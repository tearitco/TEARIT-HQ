#!/bin/sh
# build_install_eden.sh - builds the Eden button installer (../install_eden.c) into ../+x/install_eden.+x (picked up by $.crypts/compile-runner.sh as ops/build_*.sh).
# Verify: the pal harness _shared-lib/harness/eden_install.pal.
set -e
D="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$D/../+x"
${CC:-gcc} -std=gnu11 -Wall -Wextra -Wno-format-truncation -Wno-unused-result -O2 -o "$D/../+x/install_eden.+x" "$D/../install_eden.c"
echo "OK eden/+x/install_eden.+x"
