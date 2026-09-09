#!/bin/sh
# build_test_proc_registry.sh — build + run all standalone, desktop-safe
# tests for kh_proc_registry.h / kh_spawn.h. They spawn their own
# detached /tmp children; no house process is touched. See:
#   #.#.calendar-dox/!.HQ-IQ-BOOK/08-roadmap/design-docs/
#   PROC-LIFECYCLE-ORCHESTRATOR-TEARDOWN.md
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
T="${TMPDIR:-/tmp}"
for t in test_proc_registry test_proc_registry_tb test_proc_registry_master; do
    cc -std=c11 -Wall -Wextra -O2 "$HERE/$t.c" -o "$T/$t.+x"
done
echo "== unit: kh_proc_registry =="            ; "$T/test_proc_registry.+x"
echo; echo "== integration: taskbar wiring ==" ; "$T/test_proc_registry_tb.+x"
echo; echo "== master-ledger: owned/subtree/self ==" ; "$T/test_proc_registry_master.+x"
