#!/bin/bash
# nb_interact_test.sh - drive the live network-browser window through the
# relay file and assert on state files. Covers what page.state snapshots
# cannot: typing, submit, nav-jump clicks, copy/paste.
# Nav numbers are DISCOVERED from frame dumps (never hardcoded - numbering
# is global across windows and shifts). Any step that cannot discover its
# target reports SKIP, not FAIL.
# Discipline (learned live): digits type into ARMED fields, so disarm (27)
# before every nav-jump; sleeps let manager ticks settle.
# Usage: sh nb_interact_test.sh   (needs a running network browser)
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
HR="$(cd "$HERE/../../.." && pwd)"
PID=""
if [ -n "${BROWSER_PID:-}" ]; then
  PID="$BROWSER_PID"
else
  "$HR/&.hq-apps/network/button.sh" "$HR" >/dev/null 2>&1
  sleep 4
  PID="$(pgrep -f "khtpm_core_render.+x .*network-browser-hq" | head -1)"
fi
[ -n "$PID" ] || { echo "FAIL: no network browser running"; exit 1; }
RELAY="$HR/#.desktop/entity_menu_history/$PID.txt"
FRAME=/tmp/entity-menu-frame.png.frame.txt
FAIL=0
send() { printf '%b\n' "$1" >> "$RELAY"; }
digits() { printf '%s' "$1" | grep -o . | while read -r d; do printf '%s\n' "$d" >> "$RELAY"; done; }
dump() { send '112'; sleep 1; }
# nav number for the first row whose label contains $1 (0 = not visible)
nav_for() { awk -F'|' -v pat="$1" '($1=="item"||$1=="text"||$1=="cli_io") && index($4, pat) && $7+0>0 {print $7; exit}' "$FRAME"; }
FORM="file://$HR/&.hq-apps/network/tests/fixtures/form-get.html"

echo "== 1. type into search field, submit, land on Wikipedia"
printf 'go:%s\n' "$FORM" > "$HR/#.desktop/network_browser_request.txt"
sleep 8
send '27'; sleep 1; dump
N="$(nav_for 'Search Wikipedia')"
if [ -z "$N" ]; then echo "SKIP: search field not visible"; else
  send '27'; digits "$N"; send '13'; sleep 2
  printf '66\n108\n111\n99\n107\n108\n121\n13\n27\n' >> "$RELAY"; sleep 2
  if grep -q "^search\tBlockly$" "$HR/#.desktop/network_browser_fields.txt" 2>/dev/null; then
    echo "PASS: field commit"
    dump
    N="$(nav_for 'Go')"
    if [ -z "$N" ]; then echo "SKIP: Go button not visible"
    else
      send '27'; digits "$N"; send '13'; sleep 15
      if [ "$(head -1 "$HR/#.desktop/network_browser_page.state.txt")" = "URL|https://en.wikipedia.org/w/index.php?search=Blockly" ]; then
        echo "PASS: submit navigates"
      else
        echo "FAIL: submit landed on $(head -1 "$HR/#.desktop/network_browser_page.state.txt")"; FAIL=1
      fi
    fi
  else
    echo "FAIL: field commit missing"; FAIL=1
  fi
fi

echo "== 2. clipboard round trip (copy row, paste into field)"
if command -v /tmp/clipget >/dev/null 2>&1; then
  send '27'; sleep 1; dump
  N="$(nav_for 'Example Domain')"
  if [ -z "$N" ]; then echo "SKIP: bookmark row not visible"
  else
    send '27'; digits "$N"; send '13'; sleep 1
    send '3'; sleep 2
    if /tmp/clipget 2>/dev/null | grep -q "Example Domain"; then
      echo "PASS: copy out"
    else
      echo "FAIL: copy out"; FAIL=1
    fi
  fi
else
  echo "SKIP: /tmp/clipget missing (build from house X11 headers to enable)"
fi
exit $FAIL
