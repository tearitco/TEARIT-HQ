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

win() { xwininfo -root -children 2>/dev/null | awk '/960x640/ {print $1; exit}'; }

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
  "$DUMP" "$(win)" "$SNAP/$name" >/dev/null
  echo "  captured $name"
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

echo "done: six real captures in $SNAP"
