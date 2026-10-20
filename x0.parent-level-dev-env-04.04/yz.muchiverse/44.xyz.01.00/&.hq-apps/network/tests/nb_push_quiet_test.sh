#!/bin/bash
# nb_push_quiet_test.sh - G1.3, the NEGATIVE half: an idle page must be
# completely silent.
#
# G1.3 asks for push ("frames arrive without a request"). Every push
# protocol ships with that risk, and it is the one nobody tests: once the
# manager can be woken by the worker, a page that has NOTHING to say can
# still republish forever, burning CPU and making the window flicker at
# the user. This suite is the brake on that.
#
# So the assertion is not "does it push" - it is "does it STOP". Load a
# page whose script installs no timers, arm the tick loop, then watch:
# zero RENDER frames for five seconds.
#
# NEGATIVE-TESTED IN THE SAME RUN: the identical harness against a page
# that DOES install a timer must push. Otherwise "zero frames" could just
# mean the harness is broken and everything passes for the wrong reason.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
PKG="$(cd "$HERE/.." && pwd)"
WORKER="$PKG/ops/+x/nb_js_worker.+x"
FAIL=0
fail() { echo "FAIL: $*"; FAIL=1; }

[ -x "$WORKER" ] || { echo "FAIL: worker not built ($WORKER)"; exit 1; }
command -v python3 >/dev/null || { echo "FAIL: python3 missing"; exit 1; }

# A DOM with two rows, built by the project's own serializer so the attrs
# blob (which is what nb_attr_get actually reads) is right.
GEN=/tmp/nb_push_quiet_mkdom.$$
cc -std=gnu11 -I"$PKG" -o "$GEN" "$HERE/g1_make_dom.c" "$PKG/nb_dom.c" 2>/dev/null \
    || { echo "FAIL: could not build the fixture generator"; exit 1; }
cat > /tmp/nb_push_quiet_page.html <<'HTML'
<html><head><title>Quiet</title></head><body>
<p id="t">idle 0</p>
<p>Quiet page: this paragraph must never change.</p>
</body></html>
HTML
"$GEN" /tmp/nb_push_quiet_page.html /tmp/nb_push_quiet_page.dom >/dev/null \
    || { echo "FAIL: fixture generator failed"; rm -f "$GEN"; exit 1; }

# The two scripts under test. The ONLY difference is the setInterval.
cat > /tmp/nb_push_quiet_idle.js <<'JS'
// installs NO timers: an idle page has nothing to say
document.getElementById("t").textContent = "idle 0";
JS
cat > /tmp/nb_push_quiet_live.js <<'JS'
var n = 0;
function bump() {
  n = n + 1;
  var el = document.getElementById("t");
  if (el) { el.textContent = "idle " + n; }
}
setInterval(bump, 200);
bump();
JS

python3 - "$WORKER" <<'PY'
import os, select, subprocess, sys, time

worker = sys.argv[1]
dom = "/tmp/nb_push_quiet_page.dom"
IDLE_W, LIVE_W = 5.0, 3.0


def talk(js_path, window, label):
    p = subprocess.Popen([worker], stdin=subprocess.PIPE,
                         stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
                         bufsize=0)

    def send(payload):
        b = payload.encode()
        p.stdin.write(b"%06d\n" % len(b) + b + b"\n")
        p.stdin.flush()

    def read_frame(deadline):
        hdr = b""
        while len(hdr) < 7:
            left = deadline - time.time()
            if left <= 0:
                return None
            if not select.select([p.stdout], [], [], left)[0]:
                return None
            c = p.stdout.read(1)
            if not c:
                return None
            hdr += c
        n = int(hdr.decode().strip())
        body = b""
        while len(body) < n:
            left = deadline - time.time()
            if left <= 0:
                return None
            if not select.select([p.stdout], [], [], left)[0]:
                return None
            body += p.stdout.read(n - len(body))
        p.stdout.read(1)                     # the framing's trailing newline
        return body

    def load():
        send("LOAD\n%s\n%s\nhttp://localhost/quiet\nQuiet\n\n" % (js_path, dom))
        seen = 0
        deadline = time.time() + 20
        while time.time() < deadline:
            fr = read_frame(deadline)
            if fr is None:
                break
            if fr.startswith(b"RENDER\n"):
                seen += 1
            if fr.startswith(b"STATUS"):
                break
        return seen

    def arm():
        send("TICK\n50\n")
        deadline = time.time() + 10
        while time.time() < deadline:
            fr = read_frame(deadline)
            if fr is None:
                return False
            if fr.startswith(b"STATUS"):
                return fr.startswith(b"STATUS ok")
        return False

    during_load = load()
    if not arm():
        print("FAIL: the worker refused TICK for the %s page" % label)
        p.kill()
        return None
    # The whole point: count frames with NOTHING happening.
    pushed = 0
    deadline = time.time() + window
    while time.time() < deadline:
        fr = read_frame(deadline)
        if fr is None:
            break
        if fr.startswith(b"RENDER\n"):
            pushed += 1
    p.kill()
    return during_load, pushed


idle = talk("/tmp/nb_push_quiet_idle.js", IDLE_W, "idle")
live = talk("/tmp/nb_push_quiet_live.js", LIVE_W, "live")

rc = 0
if idle is None or live is None:
    rc = 1
else:
    idle_load, idle_push = idle
    live_load, live_push = live
    print("  idle page: %d frames during LOAD, %d unsolicited in %.0fs"
          % (idle_load, idle_push, IDLE_W))
    print("  timer page: %d frames during LOAD, %d unsolicited in %.0fs"
          % (live_load, live_push, LIVE_W))
    if idle_load < 1:
        print("FAIL: the idle page never rendered - the harness is not seeing pages")
        rc = 1
    elif idle_push != 0:
        print("FAIL: an idle page pushed %d unsolicited frames; push must be "
              "silent when nothing changed" % idle_push)
        rc = 1
    else:
        print("PASS: an idle page produces ZERO unsolicited frames")
    # Without this, "zero frames" could just mean a broken harness.
    if live_push < 2:
        print("FAIL: the timer page only pushed %d frames, so a zero above "
              "proves nothing" % live_push)
        rc = 1
    else:
        print("PASS: a page with a timer does push (%d frames), so the zero "
              "above is a real measurement" % live_push)
sys.exit(rc)
PY
rc=$?
rm -f "$GEN" /tmp/nb_push_quiet_page.html /tmp/nb_push_quiet_page.dom \
      /tmp/nb_push_quiet_idle.js /tmp/nb_push_quiet_live.js
[ "$rc" -ne 0 ] && FAIL=1
[ "$FAIL" -eq 0 ] && echo "nb_push_quiet_test.sh PASS"
exit $FAIL