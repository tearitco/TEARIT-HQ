#!/bin/sh
# build_test_proc_registry.sh — build + run BOTH standalone, desktop-safe
# tests for kh_proc_registry.h (they spawn their own detached /tmp
# children; no house process is touched). See:
#   #.#.calendar-dox/!.HQ-IQ-BOOK/08-roadmap/design-docs/
#   PROC-LIFECYCLE-ORCHESTRATOR-TEARDOWN.md
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
T="${TMPDIR:-/tmp}"
cc -std=c11 -Wall -Wextra -O2 "$HERE/test_proc_registry.c"    -o "$T/test_proc_registry.+x"
cc -std=c11 -Wall -Wextra -O2 "$HERE/test_proc_registry_tb.c" -o "$T/test_proc_registry_tb.+x"
echo "== unit: kh_proc_registry ==";        "$T/test_proc_registry.+x"
echo; echo "== integration: taskbar wiring ==="; "$T/test_proc_registry_tb.+x"
