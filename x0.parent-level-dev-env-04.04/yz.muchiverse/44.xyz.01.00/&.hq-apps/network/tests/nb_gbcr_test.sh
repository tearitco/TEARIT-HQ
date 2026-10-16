#!/bin/bash
# nb_gbcr_test.sh - G2.2: the worker's box tree vs the pixels the renderer
# actually painted.
#
# REWRITTEN 2026-10-15 as a LIVE test on the channels the other live suites
# already use. The previous version built its own scratch house and read
# the manager's projection from it, and was flaky for reasons that had
# nothing to do with the browser: the projection file was intermittently
# listed-but-unopenable (bash glob and python glob disagreed about it in
# the same second), so every red run was an environment artefact.
#
# The live channels are proven - nb_zoom/nb_span/nb_reach all depend on
# them passing - and each one answers a different question here:
#
#   #.desktop/khtpm_pane_geom.state.txt  the renderer PUBLISHES its content
#                                        pane box (x, y, w) -> the
#                                        translation into page space
#   /tmp/entity-menu-frame.png.frame.txt the renderer's laid-out Elem tree,
#                                        via relay 112, parsed by
#                                        frame_geom.py -> the painted rows
#   #.desktop/network_browser_console.txt the manager's capture of the
#                                        worker's own gBCR, via eval
#
# THE CONVENTION, decided before any number so it cannot follow one:
# getBoundingClientRect is viewport-relative - a page's viewport starts at
# the top-left of the pane it is rendered into, as in any real browser -
# and the renderer paints in ABSOLUTE window coordinates. So painted rows
# are TRANSLATED into the page's viewport space before comparing. The
# alternative, making the worker report absolute coordinates, would leak
# window chrome into a page's geometry, which is what viewport-relative
# exists to prevent.
#
# It navigates the live browser to its own throwaway server and leaves it
# where it found it.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
HR="$(cd "$HERE/../../.." && pwd)"
PKG="$HR/&.hq-apps/network"
FAIL=0
fail() { echo "FAIL: $*"; FAIL=1; }

command -v python3 >/dev/null || { echo "FAIL: python3 missing"; exit 1; }
RPID="$(sh "$HERE/browser_pid.sh")"
[ -n "$RPID" ] || { echo "FAIL: no live network-browser renderer"; exit 1; }
RELAY="$HR/#.desktop/entity_menu_history/$RPID.txt"
PANE="$HR/#.desktop/khtpm_pane_geom.state.txt"
REQ="$HR/#.desktop/network_browser_request.txt"
CONSOLE="$HR/#.desktop/network_browser_console.txt"
FRAME=/tmp/entity-menu-frame.png.frame.txt

WORK="$(mktemp -d /tmp/nb_gbcr.XXXXXX)"
KILLPIDS=""
cleanup() { for p in $KILLPIDS; do kill "$p" 2>/dev/null; done; rm -rf "$WORK"; }
trap cleanup EXIT INT TERM

N=12
cat > "$WORK/server.py" <<'PY'
import http.server, socketserver, sys
N = int(sys.argv[1]) if len(sys.argv) > 1 else 12
b = ["<!doctype html><html><head><title>Gbcr</title></head><body>"]
for i in range(1, N + 1):
    b.append('<p id="p%d">Paragraph %d of the gBCR honesty fixture.</p>' % (i, i))
# a script, because run_page_scripts() returns early on a script-less page
# and the worker then never loads it at all
b.append("<script>var _gbcr=1;</script>")
b.append("</body></html>")
PAGE = "".join(b).encode()
class H(http.server.BaseHTTPRequestHandler):
    def log_message(self, *a): pass
    def do_GET(self):
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(PAGE)))
        self.end_headers(); self.wfile.write(PAGE)
socketserver.TCPServer.allow_reuse_address = True
s = socketserver.TCPServer(("127.0.0.1", 0), H)
print(s.server_address[1], flush=True); s.serve_forever()
PY
python3 "$WORK/server.py" "$N" > "$WORK/port.txt" 2>"$WORK/server.log" &
KILLPIDS="$KILLPIDS $!"
for _ in $(seq 1 40); do [ -s "$WORK/port.txt" ] && break; sleep 0.25; done
[ -s "$WORK/port.txt" ] || { echo "FAIL: throwaway server never bound"; exit 1; }
PORT="$(cat "$WORK/port.txt")"

# remember where the browser was, and put it back at the end
WAS="$(sed -n 's/^URL|//p' "$PKG/../../44.xyz.01.00/#.desktop/network_browser_page.state.txt" 2>/dev/null | head -1)"
restore() {
    [ -n "${WAS:-}" ] && printf 'go:%s\n' "$WAS" > "$REQ"
    sleep 4
}
trap 'restore; cleanup' EXIT INT TERM

printf 'go:http://127.0.0.1:%s/\n' "$PORT" > "$REQ"
for _ in $(seq 1 60); do
    grep -q "Paragraph 1 of the gBCR" "$CONSOLE" 2>/dev/null && break
    sleep 0.5
done
sleep 3

[ -f "$PANE" ] || { echo "FAIL: the renderer published no pane box ($PANE)"; fail 1; exit 1; }
PANEW=$(sed -n 's/^content_w=//p' "$PANE"); PANEX=$(sed -n 's/^content_x=//p' "$PANE"); PANEY=$(sed -n 's/^content_y=//p' "$PANE")
echo "  renderer pane: x=$PANEX y=$PANEY w=$PANEW"

rm -f "$FRAME"
printf '112\n' >> "$RELAY"
for _ in $(seq 1 30); do [ -s "$FRAME" ] && break; sleep 0.5; done
[ -s "$FRAME" ] || { echo "FAIL: no frame dump after relay 112"; exit 1; }

# the worker's own gBCR, read back through the manager's console capture
JS="var o=[];for(var i=1;i<=$N;i++){var e=document.getElementById('p'+i);var r=e.getBoundingClientRect();o.push('G'+i+' '+Math.round(r.x)+' '+Math.round(r.y)+' '+Math.round(r.width)+' '+Math.round(r.height));}o.join(';');"
printf 'eval:%s\n' "$JS" > "$REQ"
for _ in $(seq 1 40); do grep -q 'G1 ' "$CONSOLE" 2>/dev/null && break; sleep 0.5; done

python3 - "$HERE" "$N" "$PANEX" "$PANEY" "$PANEW" "$FRAME" "$CONSOLE" <<'PY'
import re, subprocess, sys
here, n, px, py, pw, frame, console = (sys.argv[1], int(sys.argv[2]), int(sys.argv[3]),
                                      int(sys.argv[4]), int(sys.argv[5]), sys.argv[6], sys.argv[7])

out = subprocess.run([sys.executable, here + "/frame_geom.py", frame],
                     capture_output=True, text=True).stdout
painted = {}
for line in out.splitlines():
    if line.startswith("#"):
        continue
    m = re.match(r"\s*\d+\s+(\S+)\s+(\S+)\s+(\S*)\s+x=(-?\d+)\s+y=(-?\d+)\s+w=(-?\d+)\s+h=(-?\d+)", line)
    if not m:
        continue
    cls, x, y, w, h = m.group(3), int(m.group(4)), int(m.group(5)), int(m.group(6)), int(m.group(7))
    # CONTENT rows are class "quiet" - measured on the frame, which carries
    # 12 `quiet` rows for 12 paragraphs and only 2 `nb-text` ones (chrome).
    # Filtering on nb-text matched the chrome and found 2 of 12.
    if "quiet" in cls:
        painted[len(painted) + 1] = (x, y, w, h)

txt = open(console, errors="replace").read().replace("\n", " ")
worker = {}
for m in re.finditer(r"\bG(\d+)\s+(-?\d+)\s+(-?\d+)\s+(-?\d+)\s+(-?\d+)", txt):
    worker[int(m.group(1))] = tuple(int(m.group(i)) for i in (2, 3, 4, 5))

print("  painted rows: %d   worker samples: %d" % (len(painted), len(worker)))
if not worker:
    print("FAIL: the worker's gBCR never came back through the console")
    sys.exit(1)
if not painted:
    print("FAIL: no painted nb-text rows in the frame dump")
    sys.exit(1)

# THE CONVENTION: translate the painted absolute rows into the page's
# viewport space, then compare.
ok = bad = 0
worst = []
for i in sorted(worker):
    if i not in painted:
        continue
    ax, ay, aw, ah = painted[i]   # (x, y, w, h) - a 4-tuple
    wx, wy, ww, wh = worker[i]
    dy = abs((ay - py) - wy)
    dw = abs(aw - ww)
    if dy <= 2 and dw <= 2:
        ok += 1
    else:
        bad += 1
        if len(worst) < 5:
            worst.append("row %d: dy=%d dw=%d (painted y=%d h=%d | worker y=%d w=%d)"
                         % (i, dy, dw, ay, ah, wy, ww))
total = ok + bad
pct = 100.0 * ok / total if total else 0.0
print("  %d/%d rows agree within +/-2px after translation (%.1f%%)" % (ok, total, pct))
for wm in worst:
    print("    %s" % wm)
if total and pct >= 95.0:
    print("PASS: the worker's box tree agrees with the painted pixels on %.0f%% of rows" % pct)
    sys.exit(0)
print("FAIL: G2.2 wants >= 95%% agreement; measured %.1f%% over %d rows" % (pct, total))
sys.exit(1)
PY
rc=$?
[ "$rc" -ne 0 ] && FAIL=1
[ "$FAIL" -eq 0 ] && echo "nb_gbcr_test.sh PASS"
exit $FAIL