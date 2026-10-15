#!/bin/bash
# nb_raf_test.sh - G1.2: requestAnimationFrame must produce >= 10 fps for 3 s
# WITHOUT a worker CPU runaway.
#
# The last G1 item. The plan's KPI is two-sided on purpose: >= 10 fps AND
# < 60 % of one core. A frame loop that achieves fps by spinning flat out
# has not implemented animation, it has implemented a heater - and on this
# box the worker is a RESIDENT process shared by every browser window, so
# a runaway here is not one page being slow, it is the whole house.
#
# So both halves are asserted, and both are measured from the worker
# process itself:
#   - fps from DISTINCT frame counters observed in pushed RENDER rows, not
#     from counting frames: the loop writes "frame N", so a duplicate push
#     cannot inflate the number;
#   - CPU from /proc/<pid>/stat utime+stime across the window, which is the
#     only honest source here (the worker is a child of a manager that may
#     itself be busy).
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

WORK="$(mktemp -d /tmp/nb_raf.XXXXXX)"
cleanup() { rm -rf "$WORK"; }
trap cleanup EXIT INT TERM

GEN="$WORK/mkdom"
cc -std=gnu11 -I"$PKG" -o "$GEN" "$HERE/g1_make_dom.c" "$PKG/nb_dom.c" 2>/dev/null \
    || { echo "FAIL: could not build the fixture generator"; exit 1; }
cat > "$WORK/page.html" <<'HTML'
<html><head><title>RAF</title></head><body>
<p id="out">frame 0</p>
<p>RAF: this paragraph must never change.</p>
</body></html>
HTML
"$GEN" "$WORK/page.html" "$WORK/page.dom" >/dev/null \
    || { echo "FAIL: fixture generator failed"; exit 1; }

cat > "$WORK/page.js" <<'JS'
// A real animation loop: each frame bumps a counter and writes it into the
// DOM, so the row content is the frame number. Re-registering inside the
// callback is exactly how rAF chains in a browser.
var frames = 0;
function step() {
    frames = frames + 1;
    var out = document.getElementById("out");
    if (out) { out.textContent = "frame " + frames; }
    requestAnimationFrame(step);
}
requestAnimationFrame(step);
JS

python3 - "$WORKER" "$WORK/page.js" "$WORK/page.dom" <<'PY'
import os, re, select, subprocess, sys, time

worker, js, dom = sys.argv[1], sys.argv[2], sys.argv[3]
WINDOW = 3.0
MIN_FPS = 10.0
CPU_LIMIT = 60.0        # percent of one core

p = subprocess.Popen([worker], stdin=subprocess.PIPE,
                     stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
                     bufsize=0)
PID = p.pid


def cpu_seconds(pid):
    """utime + stime from /proc/<pid>/stat, in seconds. Page faults and
    children are deliberately excluded - we want this process's own CPU."""
    try:
        with open("/proc/%d/stat" % pid) as f:
            fields = f.read().rsplit(")", 1)[1].split()
        # after the comm field, utime is field 14 overall => index 11 here
        return (int(fields[11]) + int(fields[12])) / os.sysconf("SC_CLK_TCK")
    except (OSError, IndexError, ValueError):
        return None


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


command("LOAD\n%s\n%s\nhttp://localhost/raf\nRAF\n\n" % (js, dom))
# Cap the idle wait tight enough that a 16 ms rAF is not throttled to
# something coarser than the frame interval itself.
command("TICK\n16\n")

cpu0 = cpu_seconds(PID)
start = time.time()
seen = set()
pushed = 0
while time.time() - start < WINDOW:
    deadline = start + WINDOW
    fr = read_frame(deadline)
    if fr is None:
        time.sleep(0.005)
        continue
    if fr.startswith(b"RENDER\n"):
        pushed += 1
        for m in re.findall(rb"frame (\d+)", fr):
            seen.add(int(m))
elapsed = time.time() - start
cpu1 = cpu_seconds(PID)

fps = len(seen) / elapsed if elapsed > 0 else 0.0
cpu_pct = None
if cpu0 is not None and cpu1 is not None and elapsed > 0:
    cpu_pct = (cpu1 - cpu0) / elapsed * 100.0

print("  %.2fs: %d pushed frames, %d DISTINCT frame counters -> %.1f fps"
      % (elapsed, pushed, len(seen), fps))
print("  worker cpu: %s"
      % ("unavailable" if cpu_pct is None else "%.1f%% of one core" % cpu_pct))

rc = 0
if fps >= MIN_FPS:
    print("PASS: rAF sustained %.1f fps for %.1fs (KPI: >= %.0f)"
          % (fps, elapsed, MIN_FPS))
else:
    print("FAIL: G1.2 wants >= %.0f fps for %.0fs, saw %.1f"
          % (MIN_FPS, WINDOW, fps))
    rc = 1

if cpu_pct is None:
    print("FAIL: could not read the worker's CPU - the second half of the KPI "
          "(no runaway) is then unproven")
    rc = 1
elif cpu_pct < CPU_LIMIT:
    print("PASS: worker CPU %.1f%% of one core, under the %.0f%% ceiling"
          % (cpu_pct, CPU_LIMIT))
else:
    print("FAIL: worker used %.1f%% of a core - the frame loop is a heater, "
          "not an animation, and this worker is resident for EVERY window"
          % cpu_pct)
    rc = 1

# The invariant paragraph, because a runaway loop that eats the DOM would
# still satisfy both numbers above.
#
# Read the EVAL's OWN frames: command() already drains through STATUS, so
# reading again afterwards just finds nothing and the check silently fails
# for the wrong reason - which is how this first reported FAIL.
after = command("EVAL\n1+1\n")
blob = "".join(f.decode("utf-8", "replace") for f in after)
if "must never change" in blob:
    print("PASS: the page's own content survived the frame loop")
else:
    print("FAIL: the invariant paragraph is gone after the frame loop "
          "(rAF loop may have consumed the DOM)")
    rc = 1

p.kill()

# ---- THE CONTROL. Same page, tick loop withheld. If rAF still sustained
# 10 fps without the tick loop, then the fps above was measuring something
# ambient (the machine being idle, a stray republish) rather than the frame
# source, and the KPI would be decorative.
p = subprocess.Popen([worker], stdin=subprocess.PIPE,
                     stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
                     bufsize=0)
PID = p.pid
command("LOAD\n%s\n%s\nhttp://localhost/raf\nRAF\n\n" % (js, dom))
# deliberately NO TICK frame
seen_off = set()
start = time.time()
while time.time() - start < WINDOW:
    fr = read_frame(start + WINDOW)
    if fr is None:
        time.sleep(0.02)
        continue
    if fr.startswith(b"RENDER\n"):
        for m in re.findall(rb"frame (\d+)", fr):
            seen_off.add(int(m))
elapsed_off = time.time() - start
fps_off = len(seen_off) / elapsed_off if elapsed_off > 0 else 0.0
print("  without the tick loop: %.1f fps (%d distinct counters)"
      % (fps_off, len(seen_off)))
if fps_off < MIN_FPS:
    print("PASS: without the tick loop the frame source stops, so the %.1f fps "
          "above is the tick loop's doing" % fps)
else:
    print("FAIL: %.1f fps even with no tick loop - this suite is measuring "
          "something other than the frame source" % fps_off)
    rc = 1
p.kill()
sys.exit(rc)
PY
rc=$?
[ "$rc" -ne 0 ] && FAIL=1
[ "$FAIL" -eq 0 ] && echo "nb_raf_test.sh PASS"
exit $FAIL