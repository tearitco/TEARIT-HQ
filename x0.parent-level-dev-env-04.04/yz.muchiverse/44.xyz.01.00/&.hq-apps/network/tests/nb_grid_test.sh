#!/bin/sh
# nb_grid_test.sh - G2.4: grid-template-columns honored
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
HR="$(cd "$HERE/../../.." && pwd)"
REQ="$HR/#.desktop/network_browser_request.txt"
CONSOLE="$HR/#.desktop/network_browser_console.txt"
WORK=/tmp/nb_grid_test
FAIL=0
note() { printf '%s\n' "$*"; }
fail() { echo "FAIL: $*"; FAIL=1; }

PID="$(sh "$HERE/browser_pid.sh")"
[ -n "$PID" ] || { echo "FAIL: no live network-browser renderer"; exit 1; }

WAS="$(sed -n 's/^URL|//p' "$HR/#.desktop/network_browser_page.state.txt" 2>/dev/null | head -1)"
restore() { [ -n "${WAS:-}" ] && printf 'go:%s\n' "$WAS" > "$REQ"; sleep 4; }
trap 'restore' EXIT INT TERM

rm -rf "$WORK"; mkdir -p "$WORK"

cat > "$WORK/server.py" <<'PY'
import http.server, socketserver
PAGE = (b'<!doctype html><html><head><title>Grid</title></head><body>'
        b'<div id="g" style="display:grid;grid-template-columns:repeat(3,1fr)">'
        b'<p id="c1">One</p><p id="c2">Two</p><p id="c3">Three</p>'
        b'</div>'
        b'<script>var _g=1;</script></body></html>')
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

TOK="grid$$_$(date +%s)"
printf 'go:http://127.0.0.1:%s/\n' "$PORT" > "$REQ"
n=0; while ! grep -q 'GridProbe-unused' "$CONSOLE" 2>/dev/null && [ $n -lt 3 ]; do sleep 1; n=$((n+1)); done
sleep 6

printf 'eval:console.log("%s "+JSON.stringify(["c1","c2","c3"].map(function(i){var e=document.getElementById(i);if(!e)return null;var r=e.getBoundingClientRect();return [Math.round(r.x),Math.round(r.y),Math.round(r.width)];})))\n' "$TOK" > "$REQ"
n=0
while ! grep -q "$TOK" "$CONSOLE" 2>/dev/null && [ $n -lt 60 ]; do sleep 0.5; n=$((n+1)); done
RAW="$(grep -o "$TOK .*" "$CONSOLE" 2>/dev/null | tail -1 | cut -d' ' -f2-)"
[ -n "$RAW" ] || { echo "FAIL: this run's gBCR sample never arrived (token $TOK)"; exit 1; }

echo "$RAW" > "$WORK/raw.json"
python3 - "$WORK/raw.json" <<'PY'
import json, sys
rows = json.load(open(sys.argv[1]))
if not rows or any(r is None for r in rows):
    print("FAIL: grid children not found (%r)" % (rows,))
    sys.exit(1)
print("  measured (x,y,w):", rows)
bad = 0
# all in same row
ys = [r[1] for r in rows]
if len(set(ys)) != 1:
    print("FAIL: not in one row"); sys.exit(1)
# share edges
gaps = [(a[0]+a[2])-b[0] for a,b in zip(rows,rows[1:])]
if any(abs(g)>1 for g in gaps):
    print("FAIL: edges not aligned", gaps); bad=1
else:
    print("OK: grid columns share edges within 1px", gaps)
sys.exit(1 if bad else 0)
PY
rc=$?
if [ "$rc" -eq 0 ]; then echo "PASS: G2.4 grid basic"; else echo "FAIL: G2.4 grid"; fi
exit $rc
