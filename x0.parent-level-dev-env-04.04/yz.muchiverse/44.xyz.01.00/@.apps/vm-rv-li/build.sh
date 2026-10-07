#!/bin/bash
# build.sh - build vm-rv-li's real manager (RVLV emulator <-> khtpm bridge).
# Mirrors co-lab-hai/build.sh: pure-C, no X11 dependency, output to +x/.
set -u
SDIR="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$SDIR/+x" "$SDIR/ops/+x"
CC=${CC:-gcc}

echo "-- vm_rv_li_manager -> +x/vm_rv_li_manager.+x"
$CC -std=c11 -Wall -O2 -o "$SDIR/+x/vm_rv_li_manager.+x" "$SDIR/vm_rv_li_manager.c" \
    && echo "OK vm_rv_li_manager" || { echo "FAIL vm_rv_li_manager"; exit 1; }
