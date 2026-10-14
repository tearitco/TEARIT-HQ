#!/bin/bash
# nb_timer_test.sh - G1.1 acceptance harness: does a live timer keep running?
#
# THE FIRST STEP OF G1. The plan says a 2-3 sprint worker rewrite (tick
# loop + push protocol) must not start without a test that states what
# "fixed" means, so this is that test. It was written RED on purpose - the
# failure was the specification - and it went GREEN on 2026-10-13 when the
# tick loop landed.
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
#
# WHERE IT STANDS (2026-10-13), measured, not guessed:
#
#   GREEN: 20 distinct self-updates in 10.1 s, monotonic 3 -> 22.
#
#   Negative-tested the same run with the TICK frame withheld, so the
#   assertion is known to be able to fail: 0 distinct tick values. A
#   green test nobody has seen go red is decoration.
#
#   HOW THE FIX LANDED, because the wrong turns are the useful part.
#   The tick loop was built first and WORKED - timers firing every 500 ms,
#   invocations climbing 1 -> 12, setInterval re-arming. It still pushed
#   exactly one frame, because the rows never changed: the page's
#   document.getElementById("t") was returning NULL.
#
#   The cause was the FIXTURE, not the loop, and it is a trap worth
#   knowing: in the nb_serialize wire format the id FIELD is not the id
#   the runtime reads. nb_attr_get() parses the raw ATTRS BLOB. The
#   serializer writes  N|p|t||6|tick 0|8|p id="t"  - the blob repeats the
#   tag name and carries the attributes. My hand-written fixture put the
#   id in the field and left the blob empty, so the element was invisible
#   to getElementById while loading and dumping perfectly. Only
#   nb_serialize gets the blob right, so fixtures are now GENERATED:
#   tests/g1_make_dom.c turns timer.html into timer.dom.
#
#   Two wrong theories got there first, both recorded because the method
#   is the lesson:
#     - "the first callback destroys the DOM's text" came from reading a
#       multi-line fprintf with grep, which shows only the FIRST physical
#       line. The row set was never truncated; the theory was fiction.
#     - "the textContent setter is at fault" came from reasoning about
#       the fixture instead of diffing it against a generated one. The
#       setter was innocent and the diagnosis took one experiment.
#
#   The standing rule this bought: do not hand-write this wire format,
#   and never read a multi-line diagnostic as if grep showed all of it.
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
