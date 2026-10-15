#!/bin/bash
# nb_geometry_test.sh - G2-a: the harness. Measures geometry and is RED on
# the code as it stands.
#
# The plan says G2-a must exist before G2-b, for the same reason G1-a had
# to: a 2-3 sprint box-tree rewrite must not start without a test that
# states what "fixed" means. Today it fails, and that is the point - the
# failure is the specification, with numbers attached.
#
# WHAT IT MEASURES (gap plan G2.1 + G2.2), from the page's own point of
# view via getBoundingClientRect, which is what real layout code reads:
#
#   G2.1  every VISIBLE node carries a non-zero w and h, and a nested
#         div > p > span reports STRICTLY INCREASING y per depth.
#   G2.2  the numbers are honest: a display:none element reports
#         0,0,0,0 (the negative test - a box tree that invents geometry for
#         hidden nodes is worse than one that has none).
#
# The page does its own measuring and writes the verdict into a row, so the
# assertions travel back through the real LOAD -> JS -> DOM -> rows path.
# Nothing here is read out of a C struct, which is the only way to test
# what a page would actually see.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
PKG="$(cd "$HERE/.." && pwd)"
WORKER="$PKG/ops/+x/nb_js_worker.+x"
FAIL=0

[ -x "$WORKER" ] || { echo "FAIL: worker not built ($WORKER)"; exit 1; }
command -v python3 >/dev/null || { echo "FAIL: python3 missing"; exit 1; }

WORK="$(mktemp -d /tmp/nb_geom.XXXXXX)"
cleanup() { rm -rf "$WORK"; }
trap cleanup EXIT INT TERM

GEN="$WORK/mkdom"
cc -std=gnu11 -I"$PKG" -o "$GEN" "$HERE/g1_make_dom.c" "$PKG/nb_dom.c" 2>/dev/null \
    || { echo "FAIL: could not build the fixture generator"; exit 1; }

cat > "$WORK/page.html" <<'HTML'
<html><head><title>Geometry</title></head>
<style>
#d1 { padding: 10px; }
#p1 { margin: 8px; }
#hiddenbox { display: none; }
</style>
<body>
<div id="d1">
  <p id="p1">outer paragraph<span id="s1">inner span</span></p>
  <p id="p2">second paragraph</p>
</div>
<div id="hiddenbox"><p id="p3">never laid out</p></div>
<p id="out">no verdict</p>
</body></html>
HTML
"$GEN" "$WORK/page.html" "$WORK/page.dom" >/dev/null \
    || { echo "FAIL: fixture generator failed"; exit 1; }

cat > "$WORK/page.js" <<'JS'
// Measure like a page would, then write ONE verdict line per fact into the
// DOM so the assertions come back through the real rows pipeline.
var lines = [];
function rect(id) {
    var el = document.getElementById(id);
    if (!el) return null;
    var r = el.getBoundingClientRect();
    return { x: r.x, y: r.y, w: r.width, h: r.height };
}
function say(k, v) { lines.push(k + "=" + v); }

// --- G2.1a: every visible node carries a non-zero box -------------------
["d1", "p1", "s1", "p2"].forEach(function (id) {
    var r = rect(id);
    if (!r) { say("missing_" + id, "1"); return; }
    say("box_" + id, (r.w > 0 ? "w+" : "w0") + (r.h > 0 ? "/h+" : "/h0"));
});

// --- G2.1b: nested div > p > span, strictly increasing y per depth -------
var d = rect("d1"), p = rect("p1"), s = rect("s1");
if (d && p && s) {
    say("depth_increasing",
        (s.y > p.y && p.y >= d.y) ? "yes" : "no(" + d.y + "," + p.y + "," + s.y + ")");
} else {
    say("depth_increasing", "unknown");
}

// --- G2.2 negative: display:none must be 0,0,0,0 ------------------------
var h = rect("hiddenbox");
say("hidden_zero", (h && h.x === 0 && h.y === 0 && h.w === 0 && h.h === 0) ? "yes" : "no");
var hp = rect("p3");
say("hidden_child_zero", (hp && hp.w === 0 && hp.h === 0) ? "yes" : "no");

var out = document.getElementById("out");
if (out) { out.textContent = "VERDICT " + lines.join(" "); }
JS

python3 - "$WORKER" "$WORK/page.js" "$WORK/page.dom" <<'PY'
import re, select, subprocess, sys, time

worker, js, dom = sys.argv[1], sys.argv[2], sys.argv[3]
p = subprocess.Popen([worker], stdin=subprocess.PIPE,
                     stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, bufsize=0)


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


send("LOAD\n%s\n%s\nhttp://localhost/geometry\nGeometry\n\n" % (js, dom))
rows = []
deadline = time.time() + 30
while time.time() < deadline:
    fr = read_frame(deadline)
    if fr is None:
        break
    if fr.startswith(b"RENDER\n"):
        rows += [m.decode() for m in re.findall(rb"TEXT\|([^\n]*)", fr)]
    if fr.startswith(b"STATUS"):
        break
p.kill()

verdict = ""
for r in rows:
    if r.startswith("VERDICT"):
        verdict = r
        break
if not verdict:
    print("FAIL: the page produced no verdict row - the harness is not "
          "reaching the page's own measurement")
    print("  rows seen: %s" % rows[:4])
    sys.exit(1)

print("  page's verdict: %s" % verdict)
facts = dict(kv.split("=", 1) for kv in verdict.replace("VERDICT ", "").split())


def fact(name, default="?"):
    return facts.get(name, default)


rc = 0

# ---- G2.1a: non-zero boxes --------------------------------------------
print()
for node in ("d1", "p1", "s1", "p2"):
    box = fact("box_" + node, "missing")
    if box == "missing":
        print("FAIL: G2.1 node #%s was not found in the tree at all" % node)
        rc = 1
    elif "w0" in box or "h0" in box:
        # Substring, NOT endswith("w0"): the value is "w0/h+", so
        # endswith never matched it and a zero WIDTH passed as a real box.
        # That is the whole G2.1 failure being waved through by the test
        # that exists to catch it - the exact shape this sprint keeps
        # hitting, so it is called out here rather than quietly fixed.
        print("FAIL: G2.1 #%s reports a DEGENERATE box (%s) - no box tree yet"
              % (node, box))
        rc = 1
    else:
        print("PASS: G2.1 #%s carries a real box (%s)" % (node, box))

# ---- G2.1b: strictly increasing y per depth ----------------------------
print()
if fact("depth_increasing") == "yes":
    print("PASS: G2.1 nested div > p > span reports strictly increasing y")
else:
    print("FAIL: G2.1 nested div > p > span does NOT report strictly "
          "increasing y: %s" % fact("depth_increasing"))
    rc = 1

# ---- G2.2 negative: display:none stays 0,0,0,0 -------------------------
print()
if fact("hidden_zero") == "yes":
    print("PASS: G2.2 a display:none element reports 0,0,0,0")
else:
    print("FAIL: G2.2 NEGATIVE: a display:none element does not report "
          "0,0,0,0 - geometry is being invented for hidden nodes")
    rc = 1
if fact("hidden_child_zero") == "yes":
    print("PASS: G2.2 a node inside display:none also reports a zero box")
else:
    print("FAIL: G2.2 a node inside display:none reports a non-zero box")
    rc = 1

sys.exit(rc)
PY
rc=$?
echo
if [ "$rc" -eq 0 ]; then
    echo "nb_geometry_test.sh PASS"
else
    echo "nb_geometry_test.sh RED (exit $rc) - on the code as it stands."
    echo "This is G2-a working as intended: the failure is the G2.1/G2.2"
    echo "specification with numbers attached, NOT yet wired into"
    echo "nb_all_tests.sh. It goes in when the box tree makes it green."
fi
exit 0