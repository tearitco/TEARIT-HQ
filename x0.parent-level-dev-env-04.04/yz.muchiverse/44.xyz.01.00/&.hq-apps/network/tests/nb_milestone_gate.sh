#!/bin/sh
# nb_milestone_gate.sh - the enforcement point for the plan's own rule
# (2026-10-11 gap plan, section 5.6): "Milestone claims require K2/K3
# evidence, not just hermetic PASS."
#
# WHY THIS EXISTS
#
# K3 is tests/nb_interact_test.sh - the only suite that drives the LIVE window
# through the relay (typing, submit, nav-jump clicks, copy/paste). It is
# deliberately NOT in nb_all_tests.sh: it needs a live window AND the network
# (it submits a form to Wikipedia), so adding it to the always-green runner
# would make K1 red for environmental reasons rather than code reasons.
#
# The rule said "run it manually before milestone claims" and nothing enforced
# that. This is the enforcement point. It does not replace K1/K2; it is what
# you run IN ADDITION, and whose output you paste into the milestone note.
#
# WHAT IT DOES NOT DO
#
# It does not run nb_all_tests.sh - that is K1/K2 and takes tens of minutes.
# Run it separately and record its verdict line ("ALL TESTS PASS" or
# "SOME TESTS FAILED"). Read THAT line, never a `grep PASS=` - that grep
# returns a sub-suite's fixture count, not a suite count (burndown 3.3).
#
# Usage: sh nb_milestone_gate.sh
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
HR="$(cd "$HERE/../../.." && pwd)"
PID="$(sh "$HERE/browser_pid.sh" 2>/dev/null)"
[ -n "$PID" ] || { echo "FAIL: no live network-browser renderer (K3 needs one)"; exit 1; }

echo "== milestone gate =="
echo "  K1/K2 (hermetic + live suite): run 'sh $HERE/nb_all_tests.sh' and record"
echo "     its verdict line. That is NOT this script's job."
echo "  K3 (live interaction, below) against browser pid $PID"
echo

sh "$HERE/nb_interact_test.sh"
rc=$?

echo
if [ "$rc" -eq 0 ]; then
    echo "K3 PASS - paste this output into the milestone note alongside the"
    echo "K1/K2 verdict line. A milestone claim also needs the KPI artifact"
    echo "named in the gap plan section 3 for the KPI being closed."
else
    echo "K3 FAIL (rc=$rc) - do not claim the milestone. Note: K3 reports SKIP"
    echo "rather than FAIL when it cannot discover its target, so a FAIL here"
    echo "is a real interaction regression, not a missing fixture."
fi
exit $rc
