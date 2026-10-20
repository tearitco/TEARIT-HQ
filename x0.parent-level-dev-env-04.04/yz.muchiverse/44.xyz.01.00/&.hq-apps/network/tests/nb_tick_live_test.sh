#!/bin/bash
# nb_tick_live_test.sh - the MANAGER half of G1.1: does a ticking page
# actually repaint in a real browser session, with no input after load?
#
# nb_timer_test.sh proves the worker's tick loop and push, hermetically.
# This proves the other end: that the manager ARMS the loop, DRAINS the
# pushed frames, overlays them onto page.state.txt and re-publishes the
# projection. Without this, the worker can tick perfectly into a socketpair
# nobody reads and the window still sits frozen - which is exactly the
# state the code was in before the push protocol.
#
# HERMETIC: a scratch house ($S) and a throwaway python server on a
# high port. No browser window, no X display, no other agent's window, and
# nothing written under the real house.
#
# WHAT IT MEASURES: the served page sets its own row from a setInterval.
# After the load settles, the test samples page.state.txt TWICE with no
# request in between and requires the tick value to have moved on. A
# frozen page fails; a ticking page passes.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
PKG="$(cd "$HERE/.." && pwd)"
MANAGER="$PKG/+x/network_browser_manager.+x"
FAIL=0
fail() { echo "FAIL: $*"; FAIL=1; }

[ -x "$MANAGER" ] || { echo "FAIL: manager not built ($MANAGER)"; exit 1; }
command -v python3 >/dev/null || { echo "FAIL: python3 missing"; exit 1; }

WORK="$(mktemp -d /tmp/nb_tick_live.XXXXXX)"
KILLPIDS=""
cleanup() { for p in $KILLPIDS; do kill "$p" 2>/dev/null; done; rm -rf "$WORK"; }
trap cleanup EXIT INT TERM

S="$WORK/house"; mkdir -p "$S/#.desktop" "$S/&.hq-apps/network/tmp"
UI="$S/#.desktop/network-browser-hq_ui.txt"
REQ="$S/#.desktop/network_browser_request.txt"
STATE="$S/#.desktop/network_browser_page.state.txt"

# The manager resolves the worker RELATIVE TO THE HOUSE
# (g_package_dir = "$house/&.hq-apps/network"), and run_page_scripts()
# silently returns when that probe cannot be opened. A scratch house with
# no app installed therefore loads pages with NO worker at all - the page
# renders its static extraction and never ticks, with nothing in the log.
# Install the binary, exactly as a real house has it. It is self-contained
# (quickjs is linked in), so a copy is enough and nothing else is needed.
mkdir -p "$S/&.hq-apps/network/ops/+x"
cp "$PKG/ops/+x/nb_js_worker.+x" "$S/&.hq-apps/network/ops/+x/" \
    || { echo "FAIL: could not install the worker into the scratch house"; exit 1; }

# ---- the page under test: it updates its own row, nobody types anything --
cat > "$WORK/server.py" <<'PY'
import http.server, socketserver, sys
PAGE = b"""<!doctype html>
<html><head><title>Live tick</title></head>
<body>
<p id="t">tick 0</p>
<p>Live tick: this paragraph must never change.</p>
<script>
var n = 0;
function bump() {
  n = n + 1;
  var el = document.getElementById("t");
  if (el) { el.textContent = "tick " + n; }
}
setInterval(bump, 500);
bump();
</script>
</body></html>"""
class H(http.server.BaseHTTPRequestHandler):
    def log_message(self, *a): pass
    def do_GET(self):
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(PAGE)))
        self.end_headers()
        self.wfile.write(PAGE)
socketserver.TCPServer.allow_reuse_address = True
srv = socketserver.TCPServer(("127.0.0.1", 0), H)
print(srv.server_address[1], flush=True)
srv.serve_forever()
PY
python3 "$WORK/server.py" > "$WORK/port.txt" 2>"$WORK/server.log" &
KILLPIDS="$KILLPIDS $!"
for _ in $(seq 1 40); do [ -s "$WORK/port.txt" ] && break; sleep 0.25; done
[ -s "$WORK/port.txt" ] || { echo "FAIL: throwaway server never bound a port"; exit 1; }
PORT="$(cat "$WORK/port.txt")"

setsid "$MANAGER" "$S" "$PKG" ui >"$WORK/manager.log" 2>&1 &
KILLPIDS="$KILLPIDS $!"
for _ in $(seq 1 40); do [ -f "$UI" ] && break; sleep 0.25; done

# ---- load the page, then STOP TOUCHING IT ---------------------------------
: > "$UI"
printf 'go:http://127.0.0.1:%s/\n' "$PORT" > "$REQ"
for _ in $(seq 1 60); do
    grep -q 'content_count=' "$UI" 2>/dev/null && { sleep 1; break; }
    sleep 0.5
done

if ! grep -q 'tick ' "$STATE" 2>/dev/null; then
    echo "FAIL: the page never rendered its own tick row"
    echo "  manager log tail:"; tail -5 "$WORK/manager.log" | sed 's/^/    /'
    exit 1
fi

tick_of() { grep -o 'tick [0-9]\+' "$STATE" 2>/dev/null | head -1 | tr -d 'tick '; }
A="$(tick_of)"
sleep 3
B="$(tick_of)"

echo "  page row: '$A' -> '$B' over 3 s with no input"

if [ -z "$A" ] || [ -z "$B" ]; then
    fail "could not read a tick value out of page.state.txt"
elif [ "$A" = "$B" ]; then
    fail "the page did NOT advance ($A -> $B): the worker ticked but the "\
"manager never merged the pushed frame back into page.state.txt"
elif [ "$B" -le "$A" ] 2>/dev/null; then
    fail "tick went backwards: $A -> $B"
else
    echo "PASS: a page that updates itself repaints with no input ($A -> $B)"
fi

if grep -q 'must never change' "$STATE" 2>/dev/null; then
    echo "PASS: the invariant paragraph survived the updates"
else
    fail "the invariant paragraph vanished from page.state.txt"
fi

[ "$FAIL" -eq 0 ] && echo "nb_tick_live_test.sh PASS"
exit $FAIL