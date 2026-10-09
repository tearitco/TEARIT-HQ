#!/bin/bash
# nb_all_tests.sh - run every browser test, in the order they depend on.
#
#   1. nb_projection_test.sh  must run FIRST and the browser must be
#      ALREADY RUNNING: it asserts projection invariants against the live
#      manager and dumps a real frame.
#   2. nb_layout_test.sh      drives the same running manager through each
#      fixture and diffs golden snapshots.
#   3. nb_form_test.sh        hermetic (sandbox house root), no browser.
#
# Note the shared dependency: 2 and 3 do not launch a browser, and 1 and 2
# will happily test a STALE manager left over from an older build. Relaunch
# after rebuilding, or you are diffing the previous binary's output.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
HR="$(cd "$HERE/../../.." && pwd)"
RC=0

echo "########## projection invariants (needs a running browser)"
bash "$HERE/nb_projection_test.sh" || RC=1

echo
echo "########## layout snapshots (needs the same running browser)"
bash "$HERE/nb_layout_test.sh" || RC=1

echo
echo "########## inline span grouping contract (needs the browser)"
sh "$HERE/nb_span_test.sh" || RC=1

echo
echo "########## table columns (needs the browser)"
sh "$HERE/nb_table_test.sh" || RC=1

echo
echo "########## form gate (hermetic)"
sh "$HERE/nb_form_test.sh" || RC=1

echo
[ "$RC" -eq 0 ] && echo "ALL TESTS PASS" || echo "SOME TESTS FAILED"
exit $RC