#!/bin/bash
# nb_download_test.sh - pins DOWNLOADS (G3.3).
#
# What a browser does, and this is the whole contract:
#   ATTACH   Content-Disposition: attachment is a DOWNLOAD, not a page -
#            the bytes are saved and the pane reports it. Never parsed as
#            HTML.
#   TYPE     a media type this window cannot render is a DOWNLOAD too
#            (explicit allowlist, not a blocklist: a browser that renders
#            "anything not known to be binary" shows you base64 as a page)
#   NAME     Content-Disposition filename first (RFC 5987 filename* beats
#            plain filename=), then the URL's last segment, then
#            "download". Never invented from the page title.
#   UNIQUE   an existing name becomes "name (1).ext" - never an overwrite
#   REPORT   the row carries name, size and path
#   NEGATIVE an unwritable destination reports an error row instead of
#            pretending the save happened
#   CONTROL  a plain text/html page is STILL rendered - the rules above
#            must not turn ordinary pages into downloads
#   UNSAFE   a server-suggested "../../pwned.txt" cannot escape the
#            downloads directory
#
# HERMETIC: a scratch house ($S) and a throwaway python http server on a
# free port. It fetches for real, so the bytes on disk are the bytes the
# server sent - nothing is stubbed.
#
# Usage: sh nb_download_test.sh
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
PKG="$(cd "$HERE/.." && pwd)"
MANAGER="$PKG/+x/network_browser_manager.+x"
WORK=/tmp/nb_download_test
FAIL=0
fail() { echo "FAIL: $*"; FAIL=1; }
note() { printf '%s\n' "$*"; }
command -v python3 >/dev/null || { echo "FAIL: python3 missing"; exit 1; }
[ -x "$MANAGER" ] || { echo "FAIL: manager not built ($MANAGER)"; exit 1; }

KILLPIDS=""
cleanup() { for p in $KILLPIDS; do kill "$p" 2>/dev/null; done; }
trap cleanup EXIT INT TERM

rm -rf "$WORK"; mkdir -p "$WORK"
S="$WORK/house"; mkdir -p "$S/#.desktop" "$S/&.hq-apps/network/tmp"
UI="$S/#.desktop/network-browser-hq_ui.txt"
REQ="$S/#.desktop/network_browser_request.txt"
DL="$S/&.hq-apps/network/downloads"

# ---- a throwaway server with the four shapes a browser must tell apart --
cat > "$WORK/server.py" <<'PY'
import http.server, socketserver, sys
BODY_ATTACH = b"ATTACHED-BODY-123\n"
BODY_OCTET  = b"OCTET-BODY-456\n"
class H(http.server.BaseHTTPRequestHandler):
    def log_message(self, *a): pass
    def do_GET(self):
        if self.path.startswith("/attach"):
            self.send_response(200)
            self.send_header("Content-Type", "application/octet-stream")
            self.send_header("Content-Disposition", 'attachment; filename="hello world.txt"')
            self.send_header("Content-Length", str(len(BODY_ATTACH)))
            self.end_headers(); self.wfile.write(BODY_ATTACH)
        elif self.path.startswith("/octet"):
            self.send_response(200)
            self.send_header("Content-Type", "application/octet-stream")
            self.send_header("Content-Length", str(len(BODY_OCTET)))
            self.end_headers(); self.wfile.write(BODY_OCTET)
        elif self.path.startswith("/trav"):
            self.send_response(200)
            self.send_header("Content-Type", "application/octet-stream")
            self.send_header("Content-Disposition", 'attachment; filename="../../pwned.txt"')
            self.send_header("Content-Length", "5")
            self.end_headers(); self.wfile.write(b"PWNED")
        elif self.path.startswith("/page"):
            body = b"<html><head><title>Ordinary page</title></head><body><p>An ordinary paragraph that must still render as a page, not become a download.</p></body></html>"
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers(); self.wfile.write(body)
        else:
            self.send_response(404); self.send_header("Content-Length", "0"); self.end_headers()
socketserver.TCPServer.allow_reuse_address = True
with socketserver.TCPServer(("127.0.0.1", 0), H) as httpd:
    print(httpd.server_address[1], flush=True)
    httpd.serve_forever()
PY
setsid python3 "$WORK/server.py" > "$WORK/port.txt" 2>"$WORK/server.err" &
SPID=$!
KILLPIDS="$KILLPIDS $SPID"
PORT=""
for _ in $(seq 1 40); do PORT="$(head -1 "$WORK/port.txt" 2>/dev/null)"; [ -n "$PORT" ] && break; sleep 0.25; done
[ -n "$PORT" ] || { echo "FAIL: test server never reported a port"; cat "$WORK/server.err"; exit 1; }
note "  server on 127.0.0.1:$PORT"

setsid "$MANAGER" "$S" "$PKG" ui >"$WORK/manager.log" 2>&1 &
MPID=$!
KILLPIDS="$KILLPIDS $MPID"
for _ in $(seq 1 60); do grep -q 'status=' "$UI" 2>/dev/null && break; sleep 0.5; done

go() {   # go <path> ; waits for a fresh ui.txt
    : > "$UI"
    printf 'go:http://127.0.0.1:%s%s\n' "$PORT" "$1" > "$REQ"
    for _ in $(seq 1 60); do
        grep -q 'content_count=' "$UI" 2>/dev/null && { sleep 0.5; return 0; }
        sleep 0.5
    done
    return 1
}

# ---- ATTACH --------------------------------------------------------------
go /attach || { echo "FAIL: /attach never published a projection"; tail -5 "$WORK/manager.log"; exit 1; }
printf 'ATTACHED-BODY-123\n' > "$WORK/want.txt"
if [ -f "$DL/hello world.txt" ] && cmp -s "$DL/hello world.txt" "$WORK/want.txt"; then
    echo "PASS: attachment saved byte-for-byte as 'hello world.txt'"
else
    fail "attachment not saved correctly; downloads dir: $(ls -A "$DL" 2>/dev/null | tr '\n' ' ')"
fi
if grep -q 'c_0_is_download=1' "$UI" && grep -q 'c_0_size=16' "$UI"; then
    echo "PASS: the row reports it is a download, with the real size ($(grep -m1 'c_0_size=' "$UI"))"
else
    fail "no download row with size: $(grep -m1 'c_0_' "$UI")"
fi
if grep -q 'ATTACHED-BODY' "$UI"; then
    fail "the attachment body leaked into the pane as page text"
else
    echo "PASS: the attachment was NOT parsed as HTML (no body text in the pane)"
fi

# ---- UNIQUE: same URL again ---------------------------------------------
go /attach || fail "second /attach never published"
if [ -f "$DL/hello world (1).txt" ]; then
    echo "PASS: a repeat download becomes 'hello world (1).txt' (no overwrite)"
else
    fail "no 'hello world (1).txt'; dir: $(ls -A "$DL" 2>/dev/null | tr '\n' ' ')"
fi
if [ -f "$DL/hello world.txt" ]; then
    echo "PASS: the first download was not overwritten"
else
    fail "the first download vanished"
fi

# ---- TYPE: no disposition, unrenderable type ---------------------------
go /octet || fail "/octet never published"
if [ -f "$DL/octet" ] && grep -q 'c_0_is_download=1' "$UI"; then
    echo "PASS: an unrenderable type downloads even with no disposition header"
else
    fail "octet-stream not downloaded; dir: $(ls -A "$DL" 2>/dev/null | tr '\n' ' ')"
fi

# ---- UNSAFE: a server cannot choose where files land --------------------
go /trav || fail "/trav never published"
if [ -f "$WORK/house/&.hq-apps/network/pwned.txt" ] || [ -f "$WORK/pwned.txt" ] || [ -f "$WORK/house/pwned.txt" ]; then
    fail "the server's ../.. escaped the downloads directory"
else
    echo "PASS: '../../pwned.txt' could not escape downloads/ (landed: $(ls -A "$DL" | grep -i pwned | tr '\n' ' '))"
fi

# ---- CONTROL: an ordinary page is still a page -------------------------
go /page || fail "/page never published"
if grep -q 'c_0_is_download=1' "$UI"; then
    fail "a plain text/html page was turned into a download"
else
    echo "PASS: an ordinary text/html page still renders as a page"
fi
if grep -q 'ordinary paragraph' "$UI"; then
    echo "PASS: the page's own text is in the projection"
else
    fail "the ordinary page lost its text"
fi

# ---- NEGATIVE: an unwritable destination must say so -------------------
chmod 500 "$DL" 2>/dev/null || true
go /octet || fail "/octet (unwritable) never published"
if grep -qi 'download failed' "$UI"; then
    echo "PASS: an unwritable destination reports an error row instead of faking success"
else
    fail "no error row when the save could not happen: $(grep -m1 'c_0_text=' "$UI")"
fi
chmod 700 "$DL" 2>/dev/null || true

[ "$FAIL" -eq 0 ] && echo "nb_download_test.sh PASS"
exit $FAIL