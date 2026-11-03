#!/usr/bin/env python3
"""nb_probe.py - drive one page through the worker with no window and no GUI.

WHY THIS EXISTS
---------------
Every other live browser test (nb_gbcr/nb_flex/nb_grid/...) needs a live
renderer in a live taskbar, so a question about the PAGE cannot be asked
without a window on screen -- and the answer then depends on window geometry
the question did not care about. This harness answers page questions hermetically:

    nb_probe.py --href https://example.com/ --page page.js --dom fetch.dom \
                --probe probe.js

It is the framed RPC path the manager itself uses (length-prefixed frames,
NOT bare lines), so what is measured here is what the manager drives.

The one seam it does not implement is the FETCH/FETCHED RPC. It answers each
FETCH with a MISMATCHED id, which nb_js_worker.c treats as "the manager could
not do it" and falls back to its own direct-curl path. That is deliberate:
curl does TLS, this harness does not, so the page gets real HTTPS with no TLS
stack here to get wrong, and the worker's own network layer is what is under
test rather than a re-implementation of it.

USAGE / OUTPUT
--------------
Prints one JSON blob on stdout:
    { "status": "...", "frames": [...], "console": [...] }
Anything the harness itself could not do is in "errors"; a non-empty
"errors" means the run is not evidence.

EXIT
----
0 if the worker answered STATUS for the probe; 1 otherwise.
"""
import argparse
import json
import os
import subprocess
import sys
import time

MAX_FRAME = 1 << 21


def wsend(fd, payload: str):
    """Write one length-prefixed frame. The worker's recv_frame() is
    LENGTH-PREFIXED and six-zero-padded; a bare newline-delimited write
    desyncs it and the next frame it reads is garbage."""
    data = payload.encode("utf-8", "surrogateescape")
    fd.write(b"%06d\n" % len(data))
    fd.write(data)
    fd.write(b"\n")
    fd.flush()


import select


def read_frame(rfile, timeout):
    """Read one frame: six digits, newline, payload, optional newline.
    Returns payload text, or None on EOF/timeout."""
    deadline = time.time() + timeout
    hdr = b""
    while True:
        left = deadline - time.time()
        if left <= 0:
            return None
        r, _, _ = select.select([rfile], [], [], left)
        if not r:
            return None
        line = rfile.readline()
        if not line:
            return None
        hdr = line.strip()
        if hdr:
            break
    try:
        n = int(hdr)
    except ValueError:
        return None
    if n < 0 or n > MAX_FRAME:
        return None
    buf = b""
    while len(buf) < n:
        left = deadline - time.time()
        if left <= 0:
            return None
        r, _, _ = select.select([rfile], [], [], left)
        if not r:
            return None
        chunk = rfile.read(n - len(buf))
        if not chunk:
            return None
        buf += chunk
    # optional trailing newline
    r, _, _ = select.select([rfile], [], [], 0)
    if r:
        rfile.read(1)
    return buf.decode("utf-8", "replace")


def serve_fetch(fd, frame, fetch_log):
    """Answer a FETCH with a mismatched id so the worker falls back to its
    own direct-curl path (which speaks TLS; we do not)."""
    lines = frame.split("\n")
    try:
        rid = int(lines[1])
    except (IndexError, ValueError):
        rid = -1
    url = lines[3] if len(lines) > 3 else "?"
    fetch_log.append(url)
    wsend(fd, "FETCHED\n%d\n0\n0\n\n" % (rid + 1000000))


def pump(fd, rfile, fetch_log, timeout, watch_frames, until_status=True):
    """Read frames until STATUS (or timeout), servicing FETCH."""
    deadline = time.time() + timeout
    while time.time() < deadline:
        fr = read_frame(rfile, max(0.1, deadline - time.time()))
        if fr is None:
            return None
        if fr.startswith("FETCH\n"):
            serve_fetch(fd, fr, fetch_log)
            continue
        if fr.startswith("LIVE|"):
            continue
        watch_frames.append(fr)
        if until_status and fr.startswith("STATUS"):
            return fr
    return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--worker", required=True)
    ap.add_argument("--page", required=True, help="page.js path")
    ap.add_argument("--dom", required=True, help="fetch.dom path")
    ap.add_argument("--href", default="https://example.com/")
    ap.add_argument("--title", default="probe")
    ap.add_argument("--css", default="")
    ap.add_argument("--width", type=int, default=800)
    ap.add_argument("--height", type=int, default=600)
    ap.add_argument("--probe", required=True, help="JS file to EVAL after load (must be one line)")
    ap.add_argument("--budget", type=int, default=120, help="NB_EVAL_BUDGET seconds")
    ap.add_argument("--load-timeout", type=float, default=180.0)
    ap.add_argument("--eval-timeout", type=float, default=120.0)
    ap.add_argument("--console", required=True, help="console capture path to point NBW_CONSOLE at")
    args = ap.parse_args()

    errors = []
    if os.path.exists(args.console):
        os.unlink(args.console)

    env = dict(os.environ)
    env["NBW_CONSOLE"] = args.console
    env["NB_EVAL_BUDGET"] = str(args.budget)
    env["NB_STACK"] = "1"
    env.pop("NB_CRASH_BT", None)

    proc = subprocess.Popen([args.worker], stdin=subprocess.PIPE,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                            env=env, bufsize=0)
    fd = proc.stdin
    rfile = proc.stdout

    fetch_log = []
    frames = []

    payload = "LOAD\n%s\n%s\n%s\n%s\n%s\n%d\n%d" % (
        args.page, args.dom, args.href, args.title, args.css,
        args.width, args.height)
    wsend(fd, payload)
    st = pump(fd, rfile, fetch_log, args.load_timeout, frames)
    if st is None:
        errors.append("no STATUS from LOAD within %.0fs" % args.load_timeout)

    probe = open(args.probe, "r").read().replace("\n", " ")
    wsend(fd, "EVAL\n%s" % probe)
    st2 = pump(fd, rfile, fetch_log, args.eval_timeout, frames)
    if st2 is None:
        errors.append("no STATUS from EVAL within %.0fs" % args.eval_timeout)

    try:
        wsend(fd, "QUIT")
    except Exception:
        pass
    time.sleep(0.3)
    if proc.poll() is None:
        proc.terminate()
    try:
        _, err = proc.communicate(timeout=5)
    except subprocess.TimeoutExpired:
        proc.kill()
        _, err = proc.communicate()

    console = []
    if os.path.exists(args.console):
        with open(args.console, errors="replace") as f:
            console = [l.rstrip("\n") for l in f]

    out = {
        "status": st,
        "eval_status": st2,
        "frames": frames,
        "console": console,
        "fetches": fetch_log,
        "stderr": (err or b"").decode("utf-8", "replace")[-8000:],
        "errors": errors,
    }
    json.dump(out, sys.stdout, indent=2)
    print()
    return 0 if st2 is not None else 1


if __name__ == "__main__":
    sys.exit(main())
