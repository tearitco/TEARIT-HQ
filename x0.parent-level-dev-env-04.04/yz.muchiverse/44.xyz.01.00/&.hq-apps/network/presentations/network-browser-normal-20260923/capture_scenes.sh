#!/bin/sh
# Capture the six presentation scenes from the real running browser.
#
# Why the renderer is relaunched per scene: the shared renderer reads
# network-browser-hq_ui.txt at startup and does not reliably pick up a
# later atomic replace of that file, so a scene that is navigated to after
# launch would be captured as the previous frame. Writing the scene first and
# relaunching the renderer second is what makes the capture match the state
# file. Every capture below is a real X11 window dump, never a mock.
set -e
H="${1:?house_root}"
HERE="$(cd "$(dirname "$0")" && pwd)"
DUMP="$H/&.widgits/_shared-lib/ops/+x/dump_frame_png_op.+x"
GO="$HERE/../../ops/nb_write_go.sh"
SNAP="$HERE/snapshots"
BASE="http://127.0.0.1:8126"

# Find the browser window by GEOMETRY, never by a hardcoded size.
#
# This used to grep for "960x640", which was the size in #.desktop/hq_ui.pdl
# at the time. That default has since changed to 784x504, so win() started
# returning the empty string, dump_frame_png_op was handed window 0x0, and
# every scene died on `BadWindow (invalid Window parameter)` — while the six
# PNGs from the previous run stayed on disk looking perfectly current. Nothing
# in the output distinguished "recaptured" from "left over".
#
# So: the expected geometry is read from the same config that sizes the
# window (#.desktop/hq_ui.pdl default_win_w/h), and a candidate must be an
# UNNAMED viewable override-redirect window of a plausible size.
#
# Unnamed is not arbitrary. The compositor owns named override-redirect
# windows: this box has a 1360x768 "mutter guard window" that beats any
# "largest window" heuristic and that dump_frame_png_op cannot dump — the
# first geometry-based attempt picked it and died with a dump failure. The
# browser's own window is "(has no name)". Config-sized wins; largest
# unnamed is the fallback if the config cannot be read.
win() {
  cfg_w=$(sed -n 's/^default_win_w=//p' "$H/#.desktop/hq_ui.pdl" 2>/dev/null | head -1 | tr -d ' \r')
  cfg_h=$(sed -n 's/^default_win_h=//p' "$H/#.desktop/hq_ui.pdl" 2>/dev/null | head -1 | tr -d ' \r')
  xwininfo -root -children 2>/dev/null | awk '/^ +0x/ {print $1}' | while read -r id; do
    info=$(xwininfo -id "$id" 2>/dev/null) || continue
    printf '%s' "$info" | grep -q "Map State: IsViewable" || continue
    printf '%s' "$info" | grep -q "Override Redirect State: yes" || continue
    printf '%s' "$info" | grep -q '(has no name)' || continue
    w=$(printf '%s' "$info" | awk '/Width:/ {print $2}')
    h=$(printf '%s' "$info" | awk '/Height:/ {print $2}')
    [ -n "$w" ] && [ -n "$h" ] || continue
    [ "$w" -ge 400 ] && [ "$h" -ge 300 ] || continue
    if [ -n "$cfg_w" ] && [ "$w" = "$cfg_w" ] && [ "$h" = "$cfg_h" ]; then
      echo "0 $w $h $id"
    else
      echo "1 $w $h $id"
    fi
  done | sort -k1,1n -k2,2nr -k3,3nr | head -1 | awk '{print $NF}'
}

kill_browser() {
  pkill -f network_browser_manager 2>/dev/null || true
  pkill -f khtpm_core_render 2>/dev/null || true
  sleep 2
}

shot() {
  name="$1"
  kill_browser
  (cd "$HERE/../.." && sh button.sh "$H" >/dev/null 2>&1)
  sleep 4
  wid=$(win)
  if [ -z "$wid" ]; then
    echo "  FATAL $name: no browser window found (win() empty)." >&2
    echo "  Is the renderer up on \$DISPLAY=${DISPLAY:-unset}?" >&2
    exit 1
  fi
  "$DUMP" "$wid" "$SNAP/$name" >/dev/null || {
    echo "  FATAL $name: dump failed for window $wid" >&2; exit 1; }
  echo "  captured $name (window $wid)"
}

# 01: real page over HTTP - text, links and six PNGs.
bash "$GO" go "$BASE/index.html" "$HERE/../.." "$H"; sleep 5
shot 01_load_fixture.png

# 02: inline script fetches /api.json through the manager RPC and writes the
#     response into the DOM.
bash "$GO" go "$BASE/scene2_fetch.html" "$HERE/../.." "$H"; sleep 5
shot 02_fetch_json.png

# 03: three 1x1 PNGs from data: URIs, decoded by stb_image.
bash "$GO" go "$BASE/scene3_image.html" "$HERE/../.." "$H"; sleep 5
shot 03_datauri_image.png

# 04: second document, so the history strip shows a real pushed entry.
bash "$GO" go "$BASE/index.html" "$HERE/../.." "$H"; sleep 4
bash "$GO" go "$BASE/page2.html" "$HERE/../.." "$H"; sleep 5
shot 04_history_nav.png

# 05: the six PNGs laid out by the generic sprite-flow grid (3 columns).
bash "$GO" go "$BASE/index.html" "$HERE/../.." "$H"; sleep 5
shot 05_media_grid.png

# 06: getBoundingClientRect read inside the page and printed back into it.
bash "$GO" go "$BASE/scene6_rect.html" "$HERE/../.." "$H"; sleep 5
shot 06_layout_rect.png

# 07: <select> read through .value/.options/.selectedIndex, with the page's own
#     change listener firing on every assignment. Proves the dropdown reports
#     the markup-selected option on load ("Upload date", index 1) and then
#     follows three script-driven selections, ending on index 0 with three
#     change events counted.
bash "$GO" go "$BASE/scene8_select.html" "$HERE/../.." "$H"; sleep 5
shot 07_select_change.png

echo "done: seven real captures in $SNAP"
