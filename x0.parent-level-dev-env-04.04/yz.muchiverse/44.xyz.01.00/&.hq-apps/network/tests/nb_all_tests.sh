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
echo "########## table computed widths (hermetic: scratch manager + own fixture window)"
sh "$HERE/nb_table_width_test.sh" || RC=1

echo
echo "########## table spans (hermetic: scratch fetch of span fixture)"
sh "$HERE/nb_table_span_test.sh" || RC=1

echo
echo "########## history bound (needs the browser)"
sh "$HERE/nb_history_test.sh" || RC=1

echo
echo "########## span hover highlight (hermetic: own fixture window)"
sh "$HERE/nb_hover_test.sh" || RC=1

echo
echo "########## span digit slots (hermetic: own fixture window)"
sh "$HERE/nb_span_digit_test.sh" || RC=1

echo
echo "########## span range selection (hermetic: own fixture window)"
sh "$HERE/nb_span_select_test.sh" || RC=1

echo
echo "########## span mouse drag-select (hermetic: own fixture window)"
sh "$HERE/nb_span_drag_test.sh" || RC=1

echo
echo "########## theme readability (static + hermetic fixture window)"
sh "$HERE/nb_theme_test.sh" || RC=1

echo
echo "########## form gate (hermetic)"
sh "$HERE/nb_form_test.sh" || RC=1

echo
echo "########## autofill (hermetic: sandbox house root + the real origin C)"
sh "$HERE/nb_autofill_test.sh" || RC=1

echo
echo "########## find in page (live browser: state + painted band)"
sh "$HERE/nb_find_test.sh" || RC=1

echo
echo "########## row-run selection + copy (hermetic: own fixture window)"
sh "$HERE/nb_selrun_test.sh" || RC=1

echo
echo "########## page zoom (hermetic: own fixture window)"
sh "$HERE/nb_zoom_test.sh" || RC=1

echo
echo "########## downloads (scratch house + a real fetch from a throwaway server)"
sh "$HERE/nb_download_test.sh" || RC=1

echo
echo "########## G1.1 live timer (worker tick loop; hermetic, no browser)"
sh "$HERE/nb_timer_test.sh" || RC=1

echo
echo "########## G1.1 live timer, MANAGER side (scratch house + real push)"
sh "$HERE/nb_tick_live_test.sh" || RC=1

echo
echo "########## G1.3 negative: an idle page must push nothing"
sh "$HERE/nb_push_quiet_test.sh" || RC=1

echo
echo "########## G1.4 focus safety (armed cli_io survives republish churn)"
sh "$HERE/nb_focus_safety_test.sh" || RC=1

echo
echo "########## G1.5 SPA click-through (re-render, never a reload)"
sh "$HERE/nb_spa_click_test.sh" || RC=1

echo
echo "########## G1.6 canvas liveness (>= 2 distinct PNGs in 5 s)"
sh "$HERE/nb_canvas_test.sh" || RC=1

echo
echo "########## G1.2 rAF loop (>= 10 fps for 3 s, no CPU runaway)"
sh "$HERE/nb_raf_test.sh" || RC=1

echo
echo "########## content-row reachability (live browser + own opt-in fixture)"
sh "$HERE/nb_reach_test.sh" || RC=1

echo
echo "########## video playhead bar (hermetic: own fixture window)"
sh "$HERE/nb_bar_test.sh" || RC=1

echo
[ "$RC" -eq 0 ] && echo "ALL TESTS PASS" || echo "SOME TESTS FAILED"
exit $RC