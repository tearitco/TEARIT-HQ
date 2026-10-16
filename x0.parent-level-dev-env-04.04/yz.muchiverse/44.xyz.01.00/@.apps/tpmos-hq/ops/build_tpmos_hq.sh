#!/bin/sh
# build_tpmos_hq.sh - compile ops/tpmos_hq.c to ops/+x/tpmos_hq.+x
cd "$(dirname "$0")" && mkdir -p +x && gcc -std=gnu11 -O2 -Wall -Wextra -Wno-unused-result -o +x/tpmos_hq.+x tpmos_hq.c && echo "OK $(pwd)/+x/tpmos_hq.+x"
