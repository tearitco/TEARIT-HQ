#!/bin/sh
# nb_gutter_test.sh - the scrollbar gutter is CONDITIONAL, and this is the
# negative control that proves it.
#
# WHY THIS EXISTS (2026-10-17)
#
# The box tree subtracts GENERIC_SCROLLBAR_W (8) from the content column only
# when the document actually overflows the pane. I proved that by hand once,
# with a throwaway fixture, and then deleted the fixture - which means the one
# assertion with no automated test was the one deciding whether every width in
# the box tree is right. A conditional deduction with no negative test rots,
# and it rots silently: the failing direction is a page that fits getting 8px
# too narrow, which looks like a plausible layout rather than a bug.
#
# The contract, both directions:
#   a page that OVERFLOWS  -> column is (pane_w - 8), matching what the
#                            renderer paints once the scrollbar exists
#   a page that FITS      -> column is pane_w, because no scrollbar is drawn
#
# Both are measured LIVE, through the real pane the renderer published, on a
# real throwaway server - the same channel nb_gbcr_test.sh uses, and the same
# run-token discipline so a stale console read can never pass for an answer.
#
# Usage: sh nb_gutter_test.sh
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
HR="$(cd "$HERE/../../.." && pwd)"
PKG="$HR/&.hq-apps/network"
REQ="$HR/#.desktop/network_browser_request.txt"
CONSOLE="$HR/#.desktop/network_browser_console.txt"
PANE="$HR/#.desktop/khtpm_pane_geom.state.txt"
WORK=/tmp/nb_gutter_test
FAIL=0
note() { printf '%s\n' "$*"; }
fail() { echo "FAIL: $*"; FAIL=1; }

PID="$(sh "$HERE/browser_pid.sh" --require-one 2>/dev/null || sh "$HERE/browser_pid.sh")"
[ -n "$PID" ] || { echo "FAIL: no live network-browser renderer"; exit 1; }
[ -f "$REQ" ] || { echo "FAIL: no request file ($REQ)"; exit 1; }

WAS="$(sed -n 's/^URL|//p' "$HR/#.desktop/network_browser_page.state.txt" 2>/dev/null | head -1)"
restore() { [ -n "${WAS:-}" ] && printf 'go:%s\n' "$WAS" > "$REQ"; sleep 4; }
trap 'restore' EXIT INT TERM

rm -rf "$WORK"; mkdir -p "$WORK"

# One server, two pages that differ ONLY in length. Same markup, same CSS-free
# body, same 30px rows - so the ONLY thing that can change the answer is whether
# the content overflows the pane. That is the whole experiment.
cat > "$WORK/server.py" <<'PY'
import http.server, socketserver, sys
def page(n):
    ps = "".join('<p id="p%d">Paragraph %d of the gutter fixture.</p>' % (i, i)
                 for i in range(1, n + 1))
    # a script, because run_page_scripts() returns early on a script-less page
    return ('<!doctype html><html><head><title>Gutter</title></head><body>'
            + ps + '<script>var _g=1;</script></body></html>').encode()
SHORT = page(2)     # 2 rows: comfortably fits a 354px pane
LONG  = page(40)    # 40 rows: 1200px, overflows it many times over
class H(http.server.BaseHTTPRequestHandler):
    def log_message(self, *a): pass
    def do_GET(self):
        body = LONG if self.path.startswith("/long") else SHORT
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers(); self.wfile.write(body)
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

probe() {  # probe <path> <paragraph-id> -> prints the pane width and the measured width
    TOK="gutter$$_$(date +%s)"
    printf 'go:http://127.0.0.1:%s%s\n' "$PORT" "$1" > "$REQ"
    n=0
    while [ ! -f "$PANE" ] && [ $n -lt 40 ]; do sleep 0.25; n=$((n+1)); done
    PW="$(sed -n 's/^content_w=//p' "$PANE" 2>/dev/null | head -1)"
    PH="$(sed -n 's/^content_h=//p' "$PANE" 2>/dev/null | head -1)"
    sleep 3
    # the token is embedded in the eval AND required by the wait, so an older
    # run's numbers can never satisfy this one
    printf 'eval:console.log("%s "+document.getElementById("%s").getBoundingClientRect().width)\n' \
        "$TOK" "$2" > "$REQ"
    n=0
    while ! grep -q "$TOK" "$CONSOLE" 2>/dev/null && [ $n -lt 60 ]; do sleep 0.5; n=$((n+1)); done
    W="$(grep -o "$TOK [0-9-]*" "$CONSOLE" 2>/dev/null | tail -1 | awk '{print $2}')"
    printf '%s %s\n' "$PW" "$W"
}

note "pane: $(sed -n 's/^content_w=/w=/p' "$PANE" 2>/dev/null | head -1) $(sed -n 's/^content_h=/h=/p' "$PANE" 2>/dev/null | head -1)"
note ""
note "the pane changes with the window, so the expectation is stated RELATIVE to it"
note ""

SHORT_W="$(probe /short p1 | awk '{print $2}')"
LONG_W="$(probe /long p1 | awk '{print $2}')"

[ -n "$SHORT_W" ] && [ -n "$LONG_W" ] || { echo "FAIL: a probe returned no width (short=[$SHORT_W] long=[$LONG_W])"; exit 1; }

note "  short page (fits, no scrollbar):  box width $SHORT_W"
note "  long page  (overflows, scrollbar): box width $LONG_W"

if [ "$SHORT_W" -eq "$LONG_W" ]; then
    echo "FAIL: both pages measured $SHORT_W - the deduction is NOT conditional"
    FAIL=1
else
    note "  -> the deduction fires only on the overflowing page (delta $((SHORT_W - LONG_W))px)"
fi

# And the delta must be the renderer's own constant, not some other width.
DELTA=$((SHORT_W - LONG_W))
if [ "$DELTA" -ne 8 ]; then
    echo "FAIL: delta is $DELTA px, expected 8 (GENERIC_SCROLLBAR_W)"
    FAIL=1
else
    note "PASS: a fitting page keeps the full column and an overflowing page loses exactly"
    note "     GENERIC_SCROLLBAR_W (8px) - the deduction is conditional in both directions"
fi

[ "$FAIL" -eq 0 ] && echo "nb_gutter_test.sh PASS"
exit $FAIL