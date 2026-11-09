#!/bin/bash
# nb_css_props_test.sh - G2.6: the recognized-property list is complete and
# exact - every property the page projection consumes lands in NbCssStyle with
# the expected value, and the NOT-recognized set drops silently, never half-
# applies. Hermetic: links nb_css.c directly (it has zero JS/engine deps by
# design), no window, no network, no worker build.
#
# WHY THIS EXISTS (2026-11-04)
# ----------------------------
# G2.6's definition of done is "the recognized-property list covers the G2.1-
# G2.5 set; the list is documented in-source; no silent drops." Until this
# gate, the only evidence was "the layout suites happen to pass" - a dropped
# property would surface as weird geometry, not as a named failure. The gate
# asserts each recognized property's resolved value by name, links against the
# real nb_css.c (not a copy), and locks the drops-are-total contract of the
# not-recognized set.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
NETDIR="$(cd "$HERE/.." && pwd)"

command -v gcc >/dev/null || { echo "FAIL: gcc missing"; exit 1; }

WORK="$(mktemp -d /tmp/nb_css_props.XXXXXX)"
trap 'rm -rf "$WORK"' EXIT INT TERM

gcc -std=c11 -O2 -Wall -I"$NETDIR" \
    "$NETDIR/nb_css.c" "$NETDIR/nb_dom.c" "$HERE/nb_css_props_test.c" -o "$WORK/nb_css_props_test" \
    || { echo "FAIL: nb_css_props_test did not compile"; exit 1; }

"$WORK/nb_css_props_test"
ec=$?
if [ $ec -eq 0 ]; then
    echo "nb_css_props_test.sh PASS (G2.6: recognized-property list complete, drops total)"
else
    echo "nb_css_props_test.sh FAIL"
fi
exit $ec
