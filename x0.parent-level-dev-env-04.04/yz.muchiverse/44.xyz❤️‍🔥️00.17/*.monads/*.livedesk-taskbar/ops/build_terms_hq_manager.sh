#!/bin/sh
# build_terms_hq_manager.sh — build db-hq's Terms tab MANAGER binary
# (2026-08-28, TPMOS-compliant, matches khtpm_hq_manager.c's pattern).
# Reads Terms data from RPG Maker MV's System.json and publishes to state file.
set -e
cd "$(dirname "$0")"
mkdir -p +x
CC=${CC:-gcc}
CFLAGS="-std=c11 -Wall -O2"

echo "-- db-hq terms manager -> +x/terms_hq_manager.+x"
$CC $CFLAGS -o +x/terms_hq_manager.+x terms_hq_manager.c

echo "OK +x/terms_hq_manager.+x"
