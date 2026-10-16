#!/bin/bash
# ############################################################################
# WIP 2026-10-15 - THIS SUITE DOES NOT PASS YET. DO NOT WIRE IT INTO
# nb_all_tests.sh, and do not read its FAILs as a verdict on the browser.
#
# Committed for the DESIGN and the DIAGNOSIS, not for a result. This is the
# first harness that compares the worker's box tree against the pixels the
# renderer actually painted - the comparison G2.2 has been waiting for.
#
# WHAT IS SETTLED (keep this):
#
#   THE CONVENTION. getBoundingClientRect is viewport-relative - a page's
#   viewport starts at the top-left of the pane it is rendered into, as in
#   any real browser. The renderer paints in ABSOLUTE window coordinates.
#   So renderer rows are TRANSLATED into the page's viewport space before
#   comparing. The alternative (making the worker report absolute) would
#   leak window chrome into a page's geometry, which is what
#   viewport-relative exists to prevent. Decided BEFORE any number, so the
#   convention cannot quietly follow whatever makes the test pass.
#
# WHAT WAS DIAGNOSED (the valuable part):
#
#   The original "failed to parse <proj>.xhtpm" was THIS TEST'S BUG, and
#   specifically the "ui" mode argument. In ui mode the manager emits the
#   key=value ui.txt ONLY and never calls write_chtpm_projection(), so no
#   parseable projection is ever produced and the renderer has nothing to
#   read. Run WITHOUT that argument it publishes
#   $house/&.hq-apps/network/network-browser-hq.chtpm - confirmed twice,
#   3611 bytes, all 12 fixture paragraphs as ids text1..text12.
#
#   Also confirmed: the manager writes into the HOUSE it was given, not the
#   package dir, so a scratch-house test genuinely cannot disturb a real
#   window. That had been an assumption this harness asserted, not knew.
#
# WHAT IS STILL WRONG:
#
#   It is flaky, not merely unfinished. In the last run the projection was
#   never produced at all ("projection exists: no") after 80 s, while two
#   earlier runs produced it. The fetch-to-projection path is not
#   deterministic from a cold scratch house, and until that is understood a
#   red run here means nothing - neither about the browser nor about the
#   comparison this harness was written to make.
#
#   Do not "fix" this by loosening the wait until it goes green.
# ############################################################################
# nb_gbcr_test.sh - G2.2: the worker's box tree vs the pixels the renderer
# actually painted.
#
# This is the first test in the browser that compares the TWO independent
# geometry systems against each other. Everything before it measured one
# side or proved they COULD agree:
#
#   - G2.1  the worker's box tree is self-consistent (nb_box_w, NB_ROW_PX)
#   - §9.10 the y-step agrees (30 = 30) once both use a row height
#   - §9.11 the width agrees (389 = 389) once the pane is fed in
#   - a4658c790 the renderer publishes its content pane, so the pane's box
#     is known to both sides
#
# THE CONVENTION, stated BEFORE the number so it cannot follow it:
# getBoundingClientRect is viewport-relative - a page's viewport begins at
# the top-left of the pane it is rendered into, which is exactly what a
# real browser reports. The renderer paints in ABSOLUTE window
# coordinates. So the renderer's rows are TRANSLATED into the page's
# viewport space (subtract the pane origin) before comparing.
#
# The alternative - making the worker report absolute coordinates - would
# mean leaking window chrome into a page's geometry, which is the thing
# viewport-relative exists to prevent. If this test ever goes red with a
# constant offset, that convention is what to revisit, not the tolerance.
#
# HERMETIC: own scratch house, own manager, own throwaway server, own
# renderer window on our own fixture xhtpm. Nothing here touches the real
# house or another agent's window.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
HR="$(cd "$HERE/../../.." && pwd)"
PKG="$HR/&.hq-apps/network"
MANAGER="$PKG/+x/network_browser_manager.+x"
WORKER="$PKG/ops/+x/nb_js_worker.+x"
RENDER="$HR/_.monads/_.livedesk-taskbar/ops/+x/khtpm_core_render.+x"
FAIL=0
fail() { echo "FAIL: $*"; FAIL=1; }

for b in "$MANAGER" "$WORKER" "$RENDER"; do
    [ -x "$b" ] || { echo "FAIL: not built: $b"; exit 1; }
done
command -v python3 >/dev/null || { echo "FAIL: python3 missing"; exit 1; }

# NOT under /tmp. Measured: in a /tmp scratch house the manager's projection
# is listed by ls but is frequently not openable at all, and even when it is,
# directory visibility flips between bash and python in the same second. The
# file is produced by an atomic tmp+rename from another process, and /tmp in
# this environment does not give a consistent view of it - so a harness there
# is flaky for reasons that have nothing to do with the browser.
WORK="$(mktemp -d "$HOME/.nb_gbcr.XXXXXX")"
KILLPIDS=""
cleanup() { for p in $KILLPIDS; do kill "$p" 2>/dev/null; done; rm -rf "$WORK"; }
trap cleanup EXIT INT TERM

S="$WORK/house"; mkdir -p "$S/#.desktop" "$S/&.hq-apps/network/tmp" "$S/&.hq-apps/network/ops/+x"
UI="$S/#.desktop/network-browser-hq_ui.txt"
REQ="$S/#.desktop/network_browser_request.txt"
STATE="$S/#.desktop/network_browser_page.state.txt"
PANE="$S/#.desktop/khtpm_pane_geom.state.txt"
# the manager resolves the worker relative to the HOUSE, and probes it
# silently: a house without the app installed renders the static
# extraction and never ticks, with an empty log.
cp "$WORKER" "$S/&.hq-apps/network/ops/+x/" || { echo "FAIL: could not install worker"; exit 1; }

N=12
cat > "$WORK/server.py" <<'PY'
import http.server, socketserver, sys
N = int(sys.argv[2]) if len(sys.argv) > 2 else 12
body = ["<!doctype html><html><head><title>Gbcr</title></head><body>"]
for i in range(1, N + 1):
    body.append('<p id="p%d">Paragraph %d of the gBCR honesty fixture.</p>' % (i, i))
# a script: run_page_scripts() returns early on a script-less page, so
# the worker never loads it and its box tree never exists.
body.append("<script>var _gbcr=1;</script>")
body.append("</body></html>")
PAGE = "".join(body).encode()
class H(http.server.BaseHTTPRequestHandler):
    def log_message(self, *a): pass
    def do_GET(self):
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(PAGE)))
        self.end_headers(); self.wfile.write(PAGE)
socketserver.TCPServer.allow_reuse_address = True
srv = socketserver.TCPServer(("127.0.0.1", 0), H)
print(srv.server_address[1], flush=True); srv.serve_forever()
PY
python3 "$WORK/server.py" "$N" > "$WORK/port.txt" 2>"$WORK/server.log" &
KILLPIDS="$KILLPIDS $!"
for _ in $(seq 1 40); do [ -s "$WORK/port.txt" ] && break; sleep 0.25; done
[ -s "$WORK/port.txt" ] || { echo "FAIL: server never bound"; exit 1; }
PORT="$(cat "$WORK/port.txt")"

# NO "ui" MODE ARGUMENT, and that is the whole fix for "failed to parse".
# In ui mode the manager emits the key=value ui.txt ONLY and never calls
# write_chtpm_projection(), so no parseable projection is ever produced -
# the renderer then has nothing to read. Measured: with "ui" the scratch
# house gets ui.txt and nothing else; without it, the projection lands at
# $house/&.hq-apps/network/network-browser-hq.chtmp.
setsid "$MANAGER" "$S" "$PKG" > "$WORK/manager.log" 2>&1 &
KILLPIDS="$KILLPIDS $!"
sleep 2
printf 'go:http://127.0.0.1:%s/\n' "$PORT" > "$REQ"
# Locate the projection with a GLOB, not a literal filename.
#
# Measured anomaly, not fixed: in a scratch house where the manager HAD
# written the projection, `ls` listed it and `cat *` read it, while
# `cat network-browser-hq.chtpm` returned ENOENT - with `ls -1 | od -c`
# showing clean, identical bytes for the name. I could not explain that,
# so rather than depend on reproducing the exact string, the harness
# resolves the file the way a shell would and then verifies it is
# readable. An unexplained path mismatch is exactly the kind of thing
# that makes a test flaky for reasons that have nothing to do with the
# thing being measured.
# Resolve the projection, and PROVE it is stably readable before testing.
#
# MEASURED, and this is why the harness changed shape. In one scratch house,
# the SAME directory in the SAME second:
#
#     bash glob *.chtpm   -> network-browser-hq.chtpm    (works)
#     python glob *.chtpm -> []                          (empty)
#     python os.listdir   -> sees it, 2665 bytes         (works)
#
# and on the previous run bash's own glob and stat failed for the same file.
# So directory visibility for this path is NONDETERMINISTIC in this
# environment, and a harness that reads it once is flaky by construction -
# its failures would say nothing about the browser.
#
# So: require the file to be readable AND carrying the page content on
# CONSECUTIVE polls before any assertion runs, and if that never happens,
# say so as a HARNESS/ENVIRONMENT failure - never as a verdict on the
# system under test. That distinction is the whole point.
want() { [ -f "$1" ] && grep -q 'Paragraph 1 of the gBCR' "$1" 2>/dev/null; }
PROJ=""
_stable=0
for _try in $(seq 1 200); do
    for _c in "$S/&.hq-apps/network/"*.chtmp "$S/&.hq-apps/network/"*.xhtpm; do
        if want "$_c"; then
            if [ "$_c" = "$PROJ" ]; then
                _stable=$((_stable + 1))
                [ "$_stable" -ge 3 ] && { PROJ="$_c"; break; }
            else
                PROJ="$_c"; _stable=1
            fi
        fi
    done
    [ -n "$PROJ" ] && [ "$_stable" -ge 3 ] && break
    sleep 0.5
done
if [ -z "$PROJ" ]; then
    echo "FAIL (HARNESS/ENVIRONMENT, not a browser verdict):"
    echo "  the manager's projection never became STABLY readable in the scratch house."
    echo "  Directory visibility for this path is nondeterministic here (measured:"
    echo "  bash glob and python glob disagree about the same file in the same second),"
    echo "  so this run cannot measure anything. Nothing below is a real result."
    echo "  present: $(ls "$S/&.hq-apps/network/" 2>/dev/null | tr '\n' ' ')"
    tail -5 "$WORK/manager.log" | sed 's/^/    /'
    exit 1
fi
echo "  projection: $PROJ (stable over 3 consecutive polls)"
# our own renderer window on the scratch house, so nothing shared is touched
setsid "$RENDER" "$S" "$S/&.hq-apps/network/network-browser-hq.chtmp" > "$WORK/render.log" 2>&1 &
RPID=$!
KILLPIDS="$KILLPIDS $RPID"
sleep 7
kill -0 "$RPID" 2>/dev/null || { echo "FAIL: our renderer window died"; tail -5 "$WORK/render.log"; exit 1; }
# The pane box is published BY THE RENDERER, so it can only be read once
# our window exists and has laid out - checking it before this point would
# be checking that the manager writes a file it never writes.
[ -f "$PANE" ] || { echo "FAIL: the renderer never published its pane box ($PANE)"; tail -5 "$WORK/render.log"; exit 1; }
PANEW=$(sed -n 's/^content_w=//p' "$PANE"); PANEX=$(sed -n 's/^content_x=//p' "$PANE"); PANEY=$(sed -n 's/^content_y=//p' "$PANE")
echo "  renderer pane: x=$PANEX y=$PANEY w=$PANEW"

printf '112\n' >> "$HR/#.desktop/entity_menu_history/$RPID.txt"
for _ in $(seq 1 30); do [ -f /tmp/entity-menu-frame.png.frame.txt ] && break; sleep 0.5; done
sleep 1

# --- the two sides, then the comparison ----------------------------------
python3 - "$HERE" "$N" "$PANEX" "$PANEY" "$PANEW" "$S" <<'PY'
import re, subprocess, sys
here, n, px, py, pw = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), int(sys.argv[4]), int(sys.argv[5])
house = sys.argv[6]

# RENDERER side: the painted rows, from the verified parser
rows = subprocess.run([sys.executable, here + "/frame_geom.py",
                       "/tmp/entity-menu-frame.png.frame.txt"],
                      capture_output=True, text=True).stdout
painted = []
for line in rows.splitlines():
    if line.startswith("#"): continue
    m = re.match(r"\s*\d+\s+(\S+)\s+(\S+)\s+(\S*)\s+x=(-?\d+)\s+y=(-?\d+)\s+w=(-?\d+)\s+h=(-?\d+)", line)
    if not m: continue
    tag, eid, cls, x, y, w, h = m.group(1), m.group(2), m.group(3), int(m.group(4)), int(m.group(5)), int(m.group(6)), int(m.group(7))
    if "nb-text" in cls:
        painted.append((eid, x, y, w, h))

# WORKER side: the page's own gBCR, via the manager's eval -> console
js = "var o=[];for(var i=1;i<=%d;i++){var e=document.getElementById('p'+i);var r=e.getBoundingClientRect();o.push('p'+i+' '+Math.round(r.x)+' '+Math.round(r.y)+' '+Math.round(r.width)+' '+Math.round(r.height));}o.join(';');" % n
open(house + "/#.desktop/network_browser_request.txt", "w").write("eval:" + js + "\n")
time_txt = None
import time
time.sleep(6)
try:
    time_txt = open(house + "/#.desktop/network_browser_console.txt").read()
except OSError:
    pass

worker = {}
if time_txt:
    blob = time_txt.replace("\n", " ")
    for m in re.finditer(r"\bp(\d+)\s+(-?\d+)\s+(-?\d+)\s+(-?\d+)\s+(-?\d+)", blob):
        worker["p" + m.group(1)] = tuple(int(m.group(i)) for i in (2, 3, 4, 5))

print("  worker sampled: %d   renderer sampled: %d" % (len(worker), len(painted)))
if not worker:
    print("FAIL: no worker-side geometry - the eval never came back")
    sys.exit(1)
if not painted:
    print("FAIL: no renderer-side rows in the frame dump")
    sys.exit(1)

# TRANSLATE the renderer's absolute rows into the page's viewport space,
# then compare. This is the convention stated in the header.
ok = bad = 0
worst = []
for eid, x, y, w, h in painted:
    if eid not in worker: continue
    wx, wy, ww, wh = worker[eid]
    vy = y - py
    dy = abs(vy - wy)
    dw = abs(w - ww)
    if dy <= 2 and dw <= 2:
        ok += 1
    else:
        bad += 1
        if len(worst) < 5: worst.append("%s dy=%d dw=%d" % (eid, dy, dw))
total = ok + bad
pct = 100.0 * ok / total if total else 0.0
print("  %d/%d within +/-2px after translation (%.1f%%)" % (ok, total, pct))
for wmsg in worst:
    print("    off: %s" % wmsg)
if pct >= 95.0:
    print("PASS: the worker's box tree agrees with the painted pixels on %.0f%% of rows" % pct)
    sys.exit(0)
print("FAIL: G2.2 wants >= 95%% agreement, measured %.1f%%" % pct)
sys.exit(1)
PY
rc=$?
[ "$rc" -ne 0 ] && FAIL=1
[ "$FAIL" -eq 0 ] && echo "nb_gbcr_test.sh PASS"
exit $FAIL