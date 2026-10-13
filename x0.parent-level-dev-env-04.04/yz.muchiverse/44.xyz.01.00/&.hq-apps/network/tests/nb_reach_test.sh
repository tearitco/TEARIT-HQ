#!/bin/bash
# nb_reach_test.sh - pins content-row KEYBOARD REACHABILITY.
#
# The defect (measured 2026-10-11, before the fix): in the browser's own
# article fixture, 9 of 10 content TEXT rows carried nav=0. Only rows with
# an action - link rows, form fields, buttons - earned a nav slot. But the
# keyboard reaches rows ONLY through g_nav[]:
#   - arrow nav steps g_nav[]            (dock_nav_step)
#   - Ctrl+C copies g_nav[focus]         (the row-label copy)
#   - digit slots index g_nav[]
# so ordinary body text was unreachable: you could not arrow onto a
# paragraph and you could not copy one. The 2026-10-07 ask "copy article
# text out of the browser" was unachievable for body text.
#
# The fix is OPT-IN: class="nav-text" on a <text> row inside a scroll
# region, the inverse of the long-standing class="no-nav". Opt-in because
# a nav slot also draws a badge (kh_elem_badge_label_x), and 44 house files
# use <scrolllist> with plain <text> rows - numbering them all would badge
# every paragraph in every window. The browser template opts its own
# content rows in; no other window changes.
#
# What is pinned here:
#   REACH   every article content row in the fixture has nav > 0
#   SCOPE   chrome text rows (status/sidebar/console) are NOT numbered -
#           the opt-in must not leak outside nb-content
#   COPY    Ctrl+C on a focused paragraph yields that paragraph's text
#           through a real X11 CLIPBOARD read
#   NEG     a plain <text> row WITHOUT class="nav-text" stays nav 0 - the
#           fix must not have become a blanket change (this is the
#           assertion that would catch a house-wide regression)
#
# Usage: sh nb_reach_test.sh          (needs a running network browser)
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
HR="$(cd "$HERE/../../.." && pwd)"
NB="$HR/&.hq-apps/network"
FRAME=/tmp/entity-menu-frame.png.frame.txt
RECEIPT=/tmp/entity-menu-frame.png.receipt.txt
FAIL=0
fail() { echo "FAIL: $*"; FAIL=1; }

[ -f "$HR/#.desktop/network-browser-hq_ui.txt" ] || { echo "FAIL: no browser running"; exit 1; }
# PID selection goes through browser_pid.sh, NOT pgrep -f: pgrep matches
# any command line CONTAINING the pattern, including the shell running
# the search. Measured 2026-10-13: pgrep reported 3 renderers with
# exactly one alive - the other two were this suite's own shell and the
# tool running it - and the relay writes to a PID nobody reads are lost
# silently, which reads exactly like a broken feature.
PID="${BROWSER_PID:-$(sh "$HERE/browser_pid.sh")}"
[ -n "$PID" ] || { echo "FAIL: no browser window"; exit 1; }
RELAY="$HR/#.desktop/entity_menu_history/$PID.txt"
[ -f "$RELAY" ] || { echo "FAIL: no relay file for pid $PID"; exit 1; }

dump() {   # dump - fresh frame for THIS pid, never a stale one
    rm -f "$RECEIPT"
    printf '112\n' >> "$RELAY"
    for _ in $(seq 1 20); do [ -f "$RECEIPT" ] && break; sleep 0.5; done
    [ -f "$RECEIPT" ] || return 1
    sleep 1
}
# relay digits are ASCII codes, not the character: dispatch_relay_code()
# atoi()s the line, so "2" is control-code 2, while the digit key is 50.
digit() { printf '%d\n' "$(printf '%d' "'$1")" >> "$RELAY"; sleep 0.4; }

printf 'go:file://%s/tests/fixtures/mini-article.html\n' "$NB" \
    > "$HR/#.desktop/network_browser_request.txt"
sleep 7
printf '27\n' >> "$RELAY"; sleep 1      # disarm any armed field
dump || { echo "FAIL: no frame dump"; exit 1; }

# ---- 1. every article content row is reachable
unreached=$(awk -F'|' '($1=="text") && $2 ~ /^(ct|cx|cr|cd|tr|li|dl|ds)[0-9]+$/ && $7+0==0' "$FRAME")
if [ -n "$unreached" ]; then
    fail "article rows still unreachable:"; echo "$unreached" | head -5
else
    n=$(awk -F'|' '($1=="text") && $2 ~ /^(ct|cx|cr|cd|tr|li|dl|ds)[0-9]+$/ && $7+0>0' "$FRAME" | wc -l)
    echo "PASS: all $n article content rows keyboard-reachable"
fi

# ---- 2. SCOPE: chrome text rows must NOT have been numbered
leak=$(awk -F'|' '($1=="text") && ($2=="status" || $2=="quiet" || $2=="") && $9>=560 && $7+0>0' "$FRAME")
if [ -n "$leak" ]; then
    fail "opt-in leaked into chrome rows:"; echo "$leak" | head -3
else
    echo "PASS: chrome text rows untouched"
fi

# ---- 3. COPY a focused paragraph through the real clipboard
PNAV=$(awk -F'|' '($1=="text") && $2 ~ /^cx[0-9]+$/ && $7+0>0 {print $7; exit}' "$FRAME")
if [ -z "$PNAV" ]; then
    fail "no reachable paragraph row to copy"
else
    printf '27\n' >> "$RELAY"; sleep 1
    printf '%s' "$PNAV" | grep -o . | while read -r d; do digit "$d"; done
    sleep 1
    dump || fail "no frame after the nav jump"
    landed=$(grep -o 'nav=[0-9]*' "$RECEIPT" | head -1 | cut -d= -f2)
    if [ "$landed" != "$PNAV" ]; then
        fail "nav jump landed on $landed, wanted $PNAV"
    else
        want=$(awk -F'|' -v n="$PNAV" '$7==n {print $4; exit}' "$FRAME")
        printf '3\n' >> "$RELAY"; sleep 3      # Ctrl+C
        got=$(python3 "$HERE/xclipget.py" 2>/dev/null)
        if [ "$got" = "$want" ]; then
            echo "PASS: copied a full paragraph out of the article"
        else
            fail "copy mismatch"
            echo "  want: $want"
            echo "  got:  $got"
        fi
    fi
fi

# ---- 4. NEGATIVE: a plain <text> row with no nav-text class stays nav 0.
# A fixture page with one opted-in row and one plain row, so the fix is
# proven to be opt-in rather than "all text rows now get numbers".
D=/tmp/nb_reach_test; rm -rf "$D"; mkdir -p "$D"
cat > "$D/optin.xhtpm" <<'EOF'
<window label="ReachOptIn" class="reach-optin-fixture database-window">
  <page id="main">
    <scrolllist class="from-top nb-content">
      <text id="opted"   class="para nav-text" label="Row that opted in"/>
      <text id="plain"   class="para"         label="Row that did not"/>
    </scrolllist>
  </page>
</window>
EOF
cat > "$D/optin.css" <<'EOF'
window { width: 500px; height: 200px; }
EOF
setsid "$HR/_.monads/_.livedesk-taskbar/ops/+x/khtpm_core_render.+x" "$HR" \
    "$D/optin.xhtpm" "$D" >"$D/render.log" 2>&1 &
FPID=$!
sleep 6
if ! kill -0 $FPID 2>/dev/null; then
    fail "opt-in fixture renderer died"; head -3 "$D/render.log"
else
    printf '112\n' >> "$HR/#.desktop/entity_menu_history/$FPID.txt"; sleep 4
    cp /tmp/entity-menu-frame.png "$D/f.png" 2>/dev/null
    cp /tmp/entity-menu-frame.png.frame.txt "$D/f.txt" 2>/dev/null
    o=$(awk -F'|' '$2=="opted" {print $7+0; exit}' "$D/f.txt")
    p=$(awk -F'|' '$2=="plain" {print $7+0; exit}' "$D/f.txt")
    if [ "$o" -gt 0 ] 2>/dev/null && [ "$p" -eq 0 ] 2>/dev/null; then
        echo "PASS: nav-text opts in, a plain row stays nav 0 (opt-in, not blanket)"
    else
        fail "opt-in boundary wrong: opted=$o plain=$p (want opted>0, plain=0)"
    fi
    kill $FPID 2>/dev/null
fi

[ "$FAIL" -eq 0 ] && echo "nb_reach_test.sh PASS"
exit $FAIL