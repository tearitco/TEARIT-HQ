#!/bin/sh
# build_test_proc_registry.sh — build + run the standalone, desktop-safe
# test for kh_proc_registry.h. Touches no house processes (spawns its
# own detached dummy children in /tmp). See:
#   #.#.calendar-dox/!.HQ-IQ-BOOK/08-roadmap/design-docs/
#   PROC-LIFECYCLE-ORCHESTRATOR-TEARDOWN.md
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="${TMPDIR:-/tmp}/test_proc_registry.+x"
cc -std=c11 -Wall -Wextra -O2 "$HERE/test_proc_registry.c" -o "$OUT"
echo "built $OUT"
exec "$OUT"
