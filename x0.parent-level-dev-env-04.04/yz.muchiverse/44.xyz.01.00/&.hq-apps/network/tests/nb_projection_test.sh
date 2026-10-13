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
PID="$(sh "$HERE/browser_pid.sh")"
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

# Ask the renderer for a frame dump and WAIT for one that provably belongs to
# this window and is newer than the request.
#
# Why this is not "sleep 2 and read the file": that is exactly how I spent
# three turns drawing confident conclusions from nothing. The renderer writes
# ascii_frames/<pid>.frame.txt lazily; right after a launch the file is often
# not there yet, and `grep -c WARN missing-file` prints nothing, which reads
# as "0 warnings" and invents a clean result. With two clones on this box a
# naive "newest frame on disk" can also be SOME OTHER WINDOW'S.
#
# Sets FRAME_FILE and FRAME_WARN. Returns non-zero if no fresh frame arrives.
dump_frame() {
    FRAME_FILE="$HR/#.desktop/ascii_frames/$PID.frame.txt"
    local marker now
    marker="$(date +%s)"
    printf '112\n' >> "$HR/#.desktop/entity_menu_history/$PID.txt"
    for _ in $(seq 1 30); do
        if [ -f "$FRAME_FILE" ]; then
            now="$(stat -c %Y "$FRAME_FILE" 2>/dev/null || echo 0)"
            if [ "$now" -ge "$marker" ]; then
                FRAME_WARN="$(grep -c WARN "$FRAME_FILE" 2>/dev/null)"
                [ -n "$FRAME_WARN" ] || FRAME_WARN=0
                return 0
            fi
        fi
        sleep 1
    done
    echo "FAIL: no FRESH ascii frame for pid $PID (stale or missing) - refusing to guess"
    return 1
}

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
    ROWS="$(grep -cE '\|(nb-text|nb-list|nb-trow|nb-link|nb-title|nb-rich)' "$FRAME" 2>/dev/null || echo 0)"
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
    # 5. EVERY published row reached the tree, at full page size.
    #    This is the assertion that pins the whole ceiling chain, and each
    #    link of it was a real silent truncation found in the same week:
    #      - gated elements took a pool slot before being dropped
    #        (37 of 124 rows on a 124-paragraph page)
    #      - the repeat expansion buffer stopped emitting when full
    #        (169 of 682 rows here)
    #      - MAX_CHILDREN=320 capped one container's row list
    #        (319 of 683 here, 364 children dropped)
    #    Reads the 206 full-tree dump, NOT the ascii mirror: the mirror
    #    skips rows parked off screen, so it answers "6 of 682" for a
    #    perfectly good build.
    rm -f /tmp/entity-menu-frame.png.receipt.txt
    printf '206\n' >> "$HR/#.desktop/entity_menu_history/$PID.txt"
    got=0
    for _ in $(seq 1 40); do
        [ -f /tmp/entity-menu-frame.png.receipt.txt ] && { got=1; break; }
        sleep 1
    done
    if [ "$got" -ne 1 ]; then
        echo "FAIL: no full-tree frame dump for the big page"; FAIL=1
    else
        sleep 1
        grep -oE '^c_[0-9]+_is_text=1' "$UI" 2>/dev/null | sed 's/c_//;s/_is_text=1//' | sort -n > /tmp/nb_big_pub.txt
        awk -F'|' '$1=="text" && $2 ~ /^cx[0-9]+$/ {print substr($2,3)}' "$FRAME" | sort -n > /tmp/nb_big_got.txt
        PUB="$(wc -l < /tmp/nb_big_pub.txt)"
        MISSING="$(comm -23 /tmp/nb_big_pub.txt /tmp/nb_big_got.txt | wc -l)"
        echo "  published text rows=$PUB  missing from tree=$MISSING"
        if [ "$MISSING" -eq 0 ] && [ "$PUB" -gt 100 ]; then
            echo "PASS: every published row reached the tree at full size ($PUB rows)"
        else
            echo "FAIL: $MISSING of $PUB published rows never reached the tree"; FAIL=1
        fi
    fi
    # 6. and the renderer had nothing to complain about while doing it
    if dump_frame; then
        if [ "$FRAME_WARN" -eq 0 ]; then
            echo "PASS: renderer reports no truncation on a full-size page"
        else
            grep WARN "$FRAME_FILE" | sed 's/^/    /'
            echo "FAIL: renderer WARN line(s) on a page the manager published in full"; FAIL=1
        fi
    else
        FAIL=1
    fi
fi
rm -f "$BIG"

# ---- mid page: EVERY PUBLISHED ROW MUST REACH THE TREE ------------------
# The gate-costs-no-pool-slot regression (2026-10-12). The content
# <repeat> emits 24 elements per row, one per content KIND, of which
# exactly one is visible. Before the fix each of those 24 took a pool
# slot and 23 were dropped AFTER allocation, so a 124-paragraph article
# asked for 2976 elements, got 1024, and rendered 37 of its 124 rows
# with a single "[WARN] element pool overflow" to explain it. ui.txt
# kept saying content_count=124 the whole time.
#
# This asserts the exact invariant rather than a proxy: the set of
# text rows the manager published must EQUAL the set the renderer built.
# Measured: 122 of 122 after, 37 of 122 before.
MID="$(mktemp /tmp/nb_mid_XXXXXX.html)"
{
    echo '<!doctype html><html><head><title>Mid</title></head><body>'
    echo '<h1>Mid page</h1>'
    i=1
    while [ "$i" -le 120 ]; do
        echo "<p>Paragraph number $i of the mid projection page, long enough to wrap so the pane genuinely scrolls.</p>"
        i=$((i + 1))
    done
    echo '</body></html>'
} > "$MID"

echo "== mid page (120 paragraphs: published == built)"
if ! load "file://$MID"; then
    echo "FAIL: mid page did not settle"; FAIL=1
else
    # 206 = unconditional full-tree dump. The ascii mirror cannot answer
    # this question: dock_ascii_walk() returns early on any row parked
    # off screen (y < -1000), so it shows ~6 of 120 rows by design.
    # Measured 2026-10-12 while writing this: using the mirror here made
    # a correct build look like it had lost 114 rows.
    rm -f /tmp/entity-menu-frame.png.receipt.txt
    printf '206\n' >> "$HR/#.desktop/entity_menu_history/$PID.txt"
    got=0
    for _ in $(seq 1 30); do
        [ -f /tmp/entity-menu-frame.png.receipt.txt ] && { got=1; break; }
        sleep 1
    done
    if [ "$got" -ne 1 ]; then
        echo "FAIL: no full-tree frame dump for the mid page"; FAIL=1
    else
        sleep 1
        grep -oE '^c_[0-9]+_is_text=1' "$UI" 2>/dev/null | sed 's/c_//;s/_is_text=1//' | sort -n > /tmp/nb_mid_pub.txt
        awk -F'|' '$1=="text" && $2 ~ /^cx[0-9]+$/ {print substr($2,3)}' "$FRAME" | sort -n > /tmp/nb_mid_got.txt
        PUB="$(wc -l < /tmp/nb_mid_pub.txt)"
        MISSING="$(comm -23 /tmp/nb_mid_pub.txt /tmp/nb_mid_got.txt | wc -l)"
        echo "  published text rows=$PUB  missing from tree=$MISSING"
        if [ "$MISSING" -eq 0 ] && [ "$PUB" -gt 100 ]; then
            echo "PASS: every published row reached the tree ($PUB rows)"
        else
            echo "FAIL: $MISSING of $PUB published rows never reached the tree"; FAIL=1
        fi
        if dump_frame; then
            POOL="$(grep -m1 '^\[POOL\]' "$FRAME_FILE" 2>/dev/null | sed -n 's/.*elements \([0-9]*\)\/\([0-9]*\).*/\1|\2/p')"
            echo "  pool=${POOL:-?}"
            if [ -n "${POOL:-}" ] && [ "${POOL%%|*}" -lt "${POOL##*|}" ]; then
                echo "PASS: pool not exhausted (${POOL%%|*} of ${POOL##*|} slots)"
            else
                echo "FAIL: element pool exhausted (${POOL:-unknown}) - gated elements are being allocated again"; FAIL=1
            fi
        else
            FAIL=1
        fi
    fi
fi
rm -f "$MID"

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
    # 2026-10-08: the renderer counts its own truncation and prints [WARN]
    # lines into the ascii frame dump. Those counters were cumulative for the
    # process lifetime, so a small page reported drops it never had - which
    # made me mis-diagnose the cause twice. A page that fits MUST be silent.
    # dump_frame refuses to return a stale or missing frame, so "0 warnings"
    # here is a real observation and not an absent file.
    if dump_frame; then
        if [ "$FRAME_WARN" -eq 0 ]; then
            echo "PASS: renderer reports no truncation on a page that fits"
        else
            echo "FAIL: small page drew $FRAME_WARN renderer WARN line(s) - stale/cumulative counter?"; FAIL=1
        fi
    else
        FAIL=1
    fi
fi

# ---- var-table overflow stability -------------------------------------
# 850 input rows overflow the 4096 var slots: the frame must report a
# nonzero drop count, and - critically - the IDENTICAL text seconds
# later. Drops are re-derivable per bulk load, and the counter resets
# per load next to the table clear (2026-10-09, same cumulative class
# as the 2026-10-08 truncation fix): pre-fix, every idle tick re-added
# a full load's drops, so the second frame read thousands. Both frames
# go through dump_frame, so neither can be stale or another window's.
echo "== var-table overflow (identical WARN across idle ticks)"
BIGI="$(mktemp /tmp/nb_biginputs_XXXXXX.html)"
{
    echo '<!doctype html><html><head><title>BigInputs</title></head><body><h1>Many inputs</h1><form action="/s">'
    i=1
    while [ "$i" -le 850 ]; do
        echo "<input name=\"n$i\" value=\"v$i\">"
        i=$((i + 1))
    done
    echo '</form></body></html>'
} > "$BIGI"
if ! load "file://$BIGI"; then
    echo "FAIL: inputs page did not settle"; FAIL=1
else
    if dump_frame; then
        W1="$(grep -a "WARN" "$FRAME_FILE" 2>/dev/null)"
    else
        W1=""; FAIL=1
    fi
    sleep 8
    if dump_frame; then
        W2="$(grep -a "WARN" "$FRAME_FILE" 2>/dev/null)"
    else
        W2=""; FAIL=1
    fi
    if [ -z "$W1" ]; then
        echo "FAIL: overflow page drew no WARN lines - fixture does not overflow (vacuous)"; FAIL=1
    elif [ "$W1" = "$W2" ]; then
        echo "PASS: WARN counts identical across idle ticks"
    else
        echo "FAIL: WARN counts drifted between frames (cumulative counter?)"; FAIL=1
    fi
fi
rm -f "$BIGI"

[ "$FAIL" -eq 0 ] && echo "PROJECTION PASS" || echo "PROJECTION FAIL"
exit $FAIL