#!/bin/bash
# nb_span_test.sh - pins the INLINE SPAN grouping contract.
#
# The rich-span rows (RICH/RICHSEG) are still inert - the renderer half has
# not landed - so nothing on screen depends on them yet. That is exactly why
# they are worth pinning NOW: the renderer will be built on this grouping, and
# a wrong grouping would surface much later as a baffling layout bug.
#
# The contract, from 2026-10-07-INLINE-SPANS-DESIGN.md:
#   1. one span group == ONE paragraph (never two welded together)
#   2. a paragraph containing a link keeps its surrounding text in the SAME
#      group, on both sides of the link
#   3. a paragraph with no link produces no group at all
#   4. PARA| markers never reach the projection (they cost zero elements)
#
# Usage: sh nb_span_test.sh    (needs a running network browser)
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
HR="$(cd "$HERE/../../.." && pwd)"
REQ="$HR/#.desktop/network_browser_request.txt"
PF="$HR/#.desktop/network_browser_page.state.txt"
UI="$HR/#.desktop/network-browser-hq_ui.txt"
FAIL=0

[ -f "$UI" ] || { echo "FAIL: no browser running"; exit 1; }

FX="$HERE/fixtures/mini-article.html"
printf 'go:file://%s\n' "$FX" > "$REQ"
for _ in $(seq 1 60); do
    grep -q "^URL|file://$FX\$" "$PF" 2>/dev/null && break
    sleep 0.5
done
sleep 2
[ -f "$PF" ] || { echo "FAIL: no page state"; exit 1; }

echo "== span grouping on mini-article.html"
GROUPS="$(grep -c '^RICH|' "$PF" 2>/dev/null)"; [ -n "$GROUPS" ] || GROUPS=0
SEGS="$(grep -c '^RICHSEG|' "$PF" 2>/dev/null)"; [ -n "$SEGS" ] || SEGS=0

# 1 + 2: exactly one group, and it is the link's own paragraph.
if [ "$GROUPS" -eq 1 ]; then
    echo "PASS: exactly one span group (paragraphs are not welded)"
else
    echo "FAIL: expected 1 span group, got $GROUPS - paragraphs welded?"; FAIL=1
fi
if grep -Fxq 'RICHSEG|link|docs|https://example.com' "$PF"; then
    echo "PASS: the link is a link segment with its URL"
else
    echo "FAIL: link segment missing or malformed"; FAIL=1
fi
# whole-line exact match: every segment line ends with the '|' field
# separator, so an anchored regex reading "for more.$" never matches.
if grep -Fxq 'RICHSEG|text|See the|' "$PF" && grep -Fxq 'RICHSEG|text|for more.|' "$PF"; then
    echo "PASS: text kept on BOTH sides of the link, same group"
else
    echo "FAIL: text around the link was dropped or split"; FAIL=1
fi

# 3: the two linkless paragraphs must NOT appear as groups.
if grep -q '^RICHSEG|text|First paragraph' "$PF" \
   || grep -q '^RICHSEG|text|Second paragraph' "$PF"; then
    echo "FAIL: a linkless paragraph produced a span group"; FAIL=1
else
    echo "PASS: linkless paragraphs produce no group"
fi

# 4: markers are for the grouper only, never for the window.
if grep -q 'PARA' "$UI" 2>/dev/null; then
    echo "FAIL: PARA marker leaked into the projection (costs an element)"; FAIL=1
else
    echo "PASS: PARA markers cost zero projection elements"
fi

echo
[ "$FAIL" -eq 0 ] && echo "SPAN PASS" || echo "SPAN FAIL"
exit $FAIL