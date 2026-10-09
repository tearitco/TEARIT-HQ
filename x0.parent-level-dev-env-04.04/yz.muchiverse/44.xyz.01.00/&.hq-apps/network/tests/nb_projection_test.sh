#!/bin/bash
# nb_projection_test.sh - asserts the PROJECTION invariants on a page big
# enough to overflow the renderer's element pool.
#
# Why this is not a nb_layout_test.sh snapshot: windowing happens in the
# projection, not in page.state. page.state still holds all 1500+ rows, so
# a golden file would be enormous AND would assert nothing about the thing
# that broke. The regression that mattered was: content_count present and
# healthy while the rendered frame held ZERO content rows - a blank pane
# on a page that reported "ready". Only a frame assertion catches that.
#
# Checks:
#   1. big page   -> content_count capped, honest notice row present,
#                    frame actually contains content rows (the blank-pane
#                    regression), manager still alive (heap-corruption
#                    regression)
#   2. small page -> no notice, nothing dropped
#
# Usage: sh nb_projection_test.sh     (needs a running network browser)
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
HR="$(cd "$HERE/../../.." && pwd)"
REQ="$HR/#.desktop/network_browser_request.txt"
UI="$HR/#.desktop/network-browser-hq_ui.txt"
FRAME=/tmp/entity-menu-frame.png.frame.txt
FAIL=0

[ -f "$UI" ] || { echo "FAIL: no browser running ($UI missing)"; exit 1; }
PID="$(pgrep -f 'khtpm_core_render.+x .*network-browser-hq' | head -1)"
[ -n "$PID" ] || { echo "FAIL: no network browser window"; exit 1; }

load() {   # load <url> - wait until the manager reports ready
    printf 'go:%s\n' "$1" > "$REQ"
    for _ in $(seq 1 60); do
        if grep -q "^URL|$(printf '%s' "$1" | sed 's/[][\.*^$/]/\\&/g')\$" "$HR/#.desktop/network_browser_page.state.txt" 2>/dev/null \
           && grep -q "status=Status: ready" "$UI" 2>/dev/null; then
            sleep 1; return 0
        fi
        sleep 1
    done
    return 1
}
cc()  { grep '^content_count=' "$UI" 2>/dev/null | tail -1 | cut -d= -f2; }
note(){ grep -c 'did not fit this window' "$UI" 2>/dev/null; }

# ---- big page ---------------------------------------------------------
BIG="$(mktemp /tmp/nb_big_XXXXXX.html)"
{
    echo '<!doctype html><html><head><title>Big</title></head><body>'
    echo '<h1>Big page</h1>'
    echo '<p>Intro paragraph.</p>'
    i=1
    while [ "$i" -le 1500 ]; do
        echo "<p>Paragraph number $i of the big projection test page.</p>"
        i=$((i + 1))
    done
    echo '</body></html>'
} > "$BIG"

echo "== big page (1500 paragraphs)"
if ! load "file://$BIG"; then
    echo "FAIL: big page did not settle"; FAIL=1
else
    N="$(cc)"
    echo "  content_count=$N  notice=$(note)"
    # 1. capped, not unbounded
    if [ -z "$N" ] || [ "$N" -gt 901 ]; then
        echo "FAIL: content_count=$N is not capped (expected <= 901)"; FAIL=1
    else
        echo "PASS: projection is capped"
    fi
    # 2. the shortfall is stated, not hidden
    if [ "$(note)" -ge 1 ]; then
        echo "PASS: honest notice row present"
    else
        echo "FAIL: rows were dropped but nothing says so"; FAIL=1
    fi
    # 3. THE regression: content_count>0 must mean a NON-EMPTY pane
    printf '112\n' >> "$HR/#.desktop/entity_menu_history/$PID.txt"
    sleep 2
    ROWS="$(grep -cE '\|(nb-text|nb-list|nb-trow|nb-link|nb-title)' "$FRAME" 2>/dev/null || echo 0)"
    echo "  content rows in frame: $ROWS"
    if [ "$ROWS" -gt 0 ]; then
        echo "PASS: frame is not blank"
    else
        echo "FAIL: content_count>0 but the frame has ZERO content rows (blank pane)"; FAIL=1
    fi
    # 4. still alive: the heap-corruption regression killed the process
    if pgrep -f 'network_browser_manager.+x' >/dev/null 2>&1; then
        echo "PASS: manager alive after a large projection"
    else
        echo "FAIL: manager died (heap corruption?)"; FAIL=1
    fi
fi
rm -f "$BIG"

# ---- small page -------------------------------------------------------
echo "== small page (no false truncation)"
if ! load "file://$HERE/fixtures/mini-article.html"; then
    echo "FAIL: small page did not settle"; FAIL=1
else
    N="$(cc)"
    if [ "$(note)" -eq 0 ] && [ -n "$N" ] && [ "$N" -gt 0 ] && [ "$N" -le 901 ]; then
        echo "PASS: small page is untouched (content_count=$N, no notice)"
    else
        echo "FAIL: small page content_count=$N notice=$(note)"; FAIL=1
    fi
fi

[ "$FAIL" -eq 0 ] && echo "PROJECTION PASS" || echo "PROJECTION FAIL"
exit $FAIL