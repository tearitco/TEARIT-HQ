#!/bin/sh
# build_hotbar_manager.sh - compile the hotbar manager (picked up by
# $.crypts/compile-runner.sh: any ops/build_*.sh). Text-includes the shared
# khtpm_inventory.c; no link step against anything.
set -e
OPS="$(cd "$(dirname "$0")" && pwd)"
SHARED="$(cd "$OPS/../../../&.widgits/_shared-lib" && pwd)"
cd "$OPS"
mkdir -p +x
gcc -O2 -Wall -I"$SHARED" -o +x/hotbar_manager.+x hotbar_manager.c
echo "-- built hotbar_manager.+x"
