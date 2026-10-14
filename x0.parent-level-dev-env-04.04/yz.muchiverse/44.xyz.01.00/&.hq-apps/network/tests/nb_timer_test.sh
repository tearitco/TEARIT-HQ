#!/bin/bash
# nb_timer_test.sh - G1.1 acceptance harness: does a live timer keep running?
#
# THE FIRST STEP OF G1, and deliberately RED on the code as it stands.
# The plan says a 2-3 sprint worker rewrite (tick loop + push protocol)
# must not start without a test that states what "fixed" means, so this is
# that test. Today it fails, and that is the point: the failure is the
# specification.
#
# WHAT IT MEASURES (gap plan G1.1): a page with setInterval(f, 500)
# updating its own row must produce >= 6 DISTINCT self-updates in 10 s
# with no user input, read from the worker's own frames, tick monotonic.
#
# WHAT IT IS NOT: a test of the manager, the renderer or the projection.
# It speaks the worker's line-RPC directly - which is why it is hermetic
# and needs no browser, no X display and no other agent's window.
#
# HARNESS SANITY comes first, and this is the part that matters when it
# goes green: a red run must be distinguishable from a broken harness. So
# the test asserts the FIRST render carries the page's own row before it
# asserts anything about updates - if that fails, the problem is the
# fixture or the framing, not the timer.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
PKG="$(cd "$HERE/.." && pwd)"
WORKER="$PKG/ops/+x/nb_js_worker.+x"
FAIL=0
fail() { echo "FAIL: $*"; FAIL=1; }
note() { printf '%s\n' "$*"; }

[ -x "$WORKER" ] || { echo "FAIL: worker not built ($WORKER)"; exit 1; }
command -v python3 >/dev/null || { echo "FAIL: python3 missing"; exit 1; }

# timer.DOM, not timer.html: the worker's LOAD handler hands the second
# field to nb_dom_load(), which reads the nb_serialize wire format and
# returns NULL for HTML. For four commits this suite pointed at the .html
# and the worker said nothing about it. Now it says:
#   ERROR|DOM: fetch.dom did not deserialize ... not HTML
# which is what finally named the mistake. The .html stays in the tree as
# the readable source the .dom was generated from.
python3 "$HERE/g1_timer_probe.py" \
    "$WORKER" "$HERE/fixtures/g1/timer.js" "$HERE/fixtures/g1/timer.dom"
rc=$?
[ "$rc" -eq 0 ] || FAIL=1
[ "$FAIL" -eq 0 ] && echo "nb_timer_test.sh PASS"
exit $FAIL
