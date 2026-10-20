#!/bin/bash
# nb_canvas_test.sh - G1.6: a canvas that mutates per tick must publish
# >= 2 DISTINCT PNGs in 5 s.
#
# The gap plan recorded the old state as "0 after the first render": the
# canvas publish path existed and was dirty-flagged, but nothing re-rendered
# between page loads, so a mutating canvas published exactly once and then
# sat still. The plan also predicted this would be "cheap once frames flow" -
# i.e. it is a test of the G1.1/G1.3 tick loop rather than new machinery.
# This suite is that test, so it is the honest check of the prediction.
#
# DISTINCT, not just present: the worker re-uses ONE path per canvas
# (/tmp/nb_canvas_<i>.png), so a count of files would be 1 forever. The
# assertion is on the PNG BYTES, hashed - a republish of unchanged pixels
# must not count as liveness.
#
# Hermetic: worker RPC directly, no browser, no X, no other agent's window.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
PKG="$(cd "$HERE/.." && pwd)"
WORKER="$PKG/ops/+x/nb_js_worker.+x"
FAIL=0
fail() { echo "FAIL: $*"; FAIL=1; }

[ -x "$WORKER" ] || { echo "FAIL: worker not built ($WORKER)"; exit 1; }
command -v python3 >/dev/null || { echo "FAIL: python3 missing"; exit 1; }

WORK="$(mktemp -d /tmp/nb_canvas.XXXXXX)"
cleanup() { rm -rf "$WORK"; }
trap cleanup EXIT INT TERM

GEN="$WORK/mkdom"
cc -std=gnu11 -I"$PKG" -o "$GEN" "$HERE/g1_make_dom.c" "$PKG/nb_dom.c" 2>/dev/null \
    || { echo "FAIL: could not build the fixture generator"; exit 1; }
cat > "$WORK/page.html" <<'HTML'
<html><head><title>Canvas</title></head><body>
<p id="note">canvas below</p>
<p>Canvas: this paragraph must never change.</p>
</body></html>
HTML
"$GEN" "$WORK/page.html" "$WORK/page.dom" >/dev/null \
    || { echo "FAIL: fixture generator failed"; exit 1; }

cat > "$WORK/page.js" <<'JS'
// Mutate the canvas every tick so each publish must carry DIFFERENT
// pixels. fillStyle is rotated through distinct colours, so a republish
// that ignored the new state would produce identical bytes.
var cv = document.createElement("canvas");
cv.width = 32; cv.height = 32;
document.body.appendChild(cv);
var ctx = cv.getContext("2d");
var n = 0;
var COLOURS = ["#ff0000", "#00ff00", "#0000ff", "#ffff00", "#ff00ff"];
function draw() {
    n = n + 1;
    ctx.fillStyle = COLOURS[n % COLOURS.length];
    ctx.fillRect(0, 0, 32, 32);
    var note = document.getElementById("note");
    if (note) { note.textContent = "frame " + n; }
}
setInterval(draw, 400);
draw();
JS

python3 - "$WORKER" "$WORK/page.js" "$WORK/page.dom" <<'PY'
import glob, hashlib, os, re, select, subprocess, sys, time

worker, js, dom = sys.argv[1], sys.argv[2], sys.argv[3]
WINDOW = 5.0

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
    p.stdout.read(1)
    return body


def command(payload, timeout=25):
    send(payload)
    frames = []
    deadline = time.time() + timeout
    while time.time() < deadline:
        fr = read_frame(deadline)
        if fr is None:
            break
        frames.append(fr)
        if fr.startswith(b"STATUS"):
            break
    return frames


def png_states():
    """Hash every canvas PNG on disk -> the set of distinct published images."""
    seen = set()
    for path in glob.glob("/tmp/nb_canvas_*.png"):
        try:
            with open(path, "rb") as f:
                seen.add(hashlib.sha256(f.read()).hexdigest())
        except OSError:
            pass
    return seen


def imgs(frames):
    out = []
    for f in frames:
        if f.startswith(b"RENDER\n"):
            out += re.findall(rb"IMG\|([^\n]*)", f)
    return out


rc = 0
# Clear any PNGs left by an earlier run so we measure only this one.
for old in glob.glob("/tmp/nb_canvas_*.png"):
    try:
        os.unlink(old)
    except OSError:
        pass

load = command("LOAD\n%s\n%s\nhttp://localhost/canvas\nCanvas\n\n" % (js, dom))
print("  LOAD: %d IMG rows" % len(imgs(load)))
if not imgs(load):
    print("FAIL: the canvas published nothing at all during LOAD - the "
          "publish path is broken before liveness even matters")
    rc = 1

send("TICK\n50\n")
deadline = time.time() + 10
while time.time() < deadline:
    fr = read_frame(deadline)
    if fr is not None and fr.startswith(b"STATUS"):
        break

start = time.time()
pushed = 0
states = set()
while time.time() - start < WINDOW:
    deadline = start + WINDOW
    fr = read_frame(deadline)
    if fr is None:
        continue
    if fr.startswith(b"RENDER\n"):
        pushed += 1
    states |= png_states()
    # Hash every poll, not just on frames, so a publish that arrives
    # without a frame still counts.
    time.sleep(0.05)

print("  over %.0fs: %d pushed frames, %d DISTINCT png states"
      % (WINDOW, pushed, len(states)))
if len(states) >= 2:
    print("PASS: a canvas that mutates per tick published >= 2 distinct PNGs "
          "in %.0fs (%d distinct)" % (WINDOW, len(states)))
else:
    print("FAIL: G1.6 wants >= 2 distinct PNGs in %.0fs, saw %d - the canvas "
          "is published once and never again" % (WINDOW, len(states)))
    rc = 1

# ---- THE NEGATIVE TEST, and the one that proves the pass above means
# something. Same harness, same page, TICK withheld - which is exactly the
# state the gap plan recorded as "0 after the first render". If the suite
# cannot reproduce THAT, then "5 distinct" above is measuring something
# other than the tick loop, and the whole KPI is void.
for old in glob.glob("/tmp/nb_canvas_*.png"):
    try:
        os.unlink(old)
    except OSError:
        pass
p.kill()

p = subprocess.Popen([worker], stdin=subprocess.PIPE,
                     stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
                     bufsize=0)
command("LOAD\n%s\n%s\nhttp://localhost/canvas\nCanvas\n\n" % (js, dom))
# deliberately NO TICK frame this time
states_notick = set()
start = time.time()
while time.time() - start < WINDOW:
    deadline = start + WINDOW
    fr = read_frame(deadline)
    if fr is None:
        time.sleep(0.05)
        continue
    states_notick |= png_states()
    time.sleep(0.05)
states_notick |= png_states()
print("  same page WITHOUT the tick loop: %d distinct png states"
      % len(states_notick))
if len(states_notick) <= 1:
    print("PASS: without the tick loop the canvas publishes once and stops - "
          "so the distinct states above are the tick loop's doing")
else:
    print("FAIL: the canvas published %d distinct states even with no tick "
          "loop, so this suite is not measuring what it claims"
          % len(states_notick))
    rc = 1

p.kill()
sys.exit(rc)
PY
rc=$?
[ "$rc" -ne 0 ] && FAIL=1
[ "$FAIL" -eq 0 ] && echo "nb_canvas_test.sh PASS"
exit $FAIL