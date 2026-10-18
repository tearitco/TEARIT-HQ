#!/bin/sh
# nb_flex_test.sh - G2.3: a page-side display:flex container lays its children
# out in a ROW with correct widths.
#
# RED ON PURPOSE (2026-10-18). The plan's own G2-a rule is "a test that
# measures geometry and is RED today". This is that test, written BEFORE the
# implementation, so the contract is pinned by something that cannot be
# re-interpreted after the fact to match whatever the code ended up doing.
#
# WHY THIS IS NOT JUST A WORKER CHANGE (measured, see the handoff §5a)
#
# For a flex div with three <p> children, today:
#
#   worker:    c1 x=0   y=30 w=389   c2 x=0   y=60 w=389   c3 x=0   y=90 w=389
#   renderer:      x=200 y=180 w=389        x=200 y=210 w=389     x=200 y=240 w=389
#
# Both STACK, so both agree - which is why this is red for a real reason and
# not because something regressed. The definition of done is the plan's own
# wording: children in a row, and SAMPLED COLUMNS SHARE EDGES WITHIN 1 PX.
#
# That last clause is the part worth keeping. "Three boxes side by side" is
# easy; "box n's right edge is box n+1's left edge, within a pixel" is the
# property that distinguishes a real row from three boxes that merely happen
# to have increasing x. Measuring the shared edge means a wrong-but-plausible
# layout cannot pass.
#
# It also deliberately does NOT check the painted pixels. G2.3's definition is
# about the box tree. Agreement with the renderer is G2.2's job and is checked
# by nb_gbcr_test.sh - which must stay at 100% throughout this work.
#
# Usage: sh nb_flex_test.sh
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
HR="$(cd "$HERE/../../.." && pwd)"
PKG="$HR/&.hq-apps/network"
REQ="$HR/#.desktop/network_browser_request.txt"
CONSOLE="$HR/#.desktop/network_browser_console.txt"
WORK=/tmp/nb_flex_test
FAIL=0
note() { printf '%s\n' "$*"; }
fail() { echo "FAIL: $*"; FAIL=1; }

PID="$(sh "$HERE/browser_pid.sh")"
[ -n "$PID" ] || { echo "FAIL: no live network-browser renderer"; exit 1; }

WAS="$(sed -n 's/^URL|//p' "$HR/#.desktop/network_browser_page.state.txt" 2>/dev/null | head -1)"
restore() { [ -n "${WAS:-}" ] && printf 'go:%s\n' "$WAS" > "$REQ"; sleep 4; }
trap 'restore' EXIT INT TERM

rm -rf "$WORK"; mkdir -p "$WORK"

# Three equal children and no CSS beyond display:flex, so the ONLY thing under
# test is the flex row itself. Each child carries enough text that a shrink-to-
# fit model would be visibly wrong, so a stub cannot pass by accident.
cat > "$WORK/server.py" <<'PY'
import http.server, socketserver
PAGE = (b'<!doctype html><html><head><title>Flex</title></head><body>'
        b'<div id="fx" style="display:flex">'
        b'<p id="c1">Column one has a fair amount of text in it</p>'
        b'<p id="c2">Column two has a fair amount of text in it</p>'
        b'<p id="c3">Column three has a fair amount of text in it</p>'
        b'</div>'
        # a script, because run_page_scripts() returns early on a script-less
        # page and the worker then never loads it at all
        b'<script>var _f=1;</script></body></html>')
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
python3 "$WORK/server.py" > "$WORK/port.txt" 2>"$WORK/server.log" &
SRV=$!
trap 'kill $SRV 2>/dev/null; restore' EXIT INT TERM
i=0; while [ ! -s "$WORK/port.txt" ] && [ $i -lt 40 ]; do sleep 0.25; i=$((i+1)); done
[ -s "$WORK/port.txt" ] || { echo "FAIL: throwaway server never bound"; exit 1; }
PORT="$(cat "$WORK/port.txt")"

# The run token goes in the eval AND gates the wait, so a previous run's numbers
# can never satisfy this one. (That discipline exists because nb_gbcr_test.sh
# once reported 100 % on a deliberately broken worker.)
TOK="flex$$_$(date +%s)"
printf 'go:http://127.0.0.1:%s/\n' "$PORT" > "$REQ"
n=0; while ! grep -q 'FlexProbe-unused' "$CONSOLE" 2>/dev/null && [ $n -lt 3 ]; do sleep 1; n=$((n+1)); done
sleep 6

printf 'eval:console.log("%s "+JSON.stringify(["c1","c2","c3"].map(function(i){var e=document.getElementById(i);if(!e)return null;var r=e.getBoundingClientRect();return [Math.round(r.x),Math.round(r.y),Math.round(r.width)];})))\n' \
    "$TOK" > "$REQ"
n=0
while ! grep -q "$TOK" "$CONSOLE" 2>/dev/null && [ $n -lt 60 ]; do sleep 0.5; n=$((n+1)); done
RAW="$(grep -o "$TOK .*" "$CONSOLE" 2>/dev/null | tail -1 | cut -d' ' -f2-)"
[ -n "$RAW" ] || { echo "FAIL: this run's gBCR sample never arrived (token $TOK)"; exit 1; }

echo "$RAW" > "$WORK/raw.json"
python3 - "$WORK/raw.json" <<'PY'
import json, sys
rows = json.load(open(sys.argv[1]))
if not rows or any(r is None for r in rows):
    print("FAIL: the flex children were not found in the DOM (%r)" % (rows,))
    sys.exit(1)
print("  measured (x, y, w) per child:")
for i, (x, y, w) in enumerate(rows, 1):
    print("    c%d  x=%-5d y=%-5d w=%d" % (i, x, y, w))

bad = 0
# 1. ONE ROW. Three stacked boxes have three different y values.
ys = [r[1] for r in rows]
if len(set(ys)) != 1:
    print("  -> children are NOT in one row: y=%s" % ys)
    bad += 1
else:
    print("  -> one row: every child shares y=%d" % ys[0])

# 2. COLUMNS SHARE EDGES WITHIN 1 PX. This is the definition's own clause and
#    the part a merely-plausible layout fails.
gaps = []
for a, b in zip(rows, rows[1:]):
    gaps.append((a[0] + a[2]) - b[0])
if any(abs(g) > 1 for g in gaps):
    print("  -> edges do NOT line up: %s" % gaps)
    bad += 1
else:
    print("  -> adjacent columns share an edge within 1px: %s" % gaps)

# 3. x MUST ADVANCE. Without this, three boxes stacked at the same x with the
#    same width would satisfy clause 1 being flat.
if rows[-1][0] <= rows[0][0]:
    print("  -> x does not advance across the row: %s" % [r[0] for r in rows])
    bad += 1
else:
    print("  -> x advances: %s" % [r[0] for r in rows])

# 4. THE COLUMNS MUST FIT THE CONTAINER. Three full-width boxes side by side
#    would overflow; the row has to share the available width.
total = sum(r[2] for r in rows)
span = (rows[-1][0] + rows[-1][2]) - rows[0][0]
if span > 0 and total > span * 1.05:
    print("  -> widths (%d) exceed the span they occupy (%d)" % (total, span))
    bad += 1

sys.exit(1 if bad else 0)
PY
rc=$?

if [ "$rc" -eq 0 ]; then
    echo "PASS: G2.3 - a display:flex container lays its children out in a row"
    echo "      with columns that share edges within 1 px"
else
    echo "FAIL: G2.3 wants one row with columns sharing edges within 1 px."
    echo "      This test is RED BY DESIGN until the worker models flex rows AND"
    echo "      the manager projects them so the renderer lays them out the same"
    echo "      way - doing only the first would re-open G2.2. See handoff 5a."
fi
exit $rc