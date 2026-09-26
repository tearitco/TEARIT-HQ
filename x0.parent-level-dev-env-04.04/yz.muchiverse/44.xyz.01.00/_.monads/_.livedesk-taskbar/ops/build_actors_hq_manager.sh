#!/bin/sh
# build_actors_hq_manager.sh — db-hq Actors tab manager (PDL, not JSON)
set -e
cd "$(dirname "$0")"
mkdir -p +x
CC=${CC:-gcc}
echo "-- db-hq actors manager -> +x/actors_hq_manager.+x"
$CC -std=c11 -Wall -O2 -o +x/actors_hq_manager.+x actors_hq_manager.c
echo "OK +x/actors_hq_manager.+x"
