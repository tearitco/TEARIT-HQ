#!/usr/bin/env python3
"""G1.1 acceptance probe: does a live timer keep running after LOAD?

The first step of G1. Deliberately RED on the code as it stands - the
failure is the specification, not an accident. See nb_timer_test.sh for
why this exists and what "fixed" has to mean.

Speaks the worker's line-RPC directly (%06d length, payload, newline), so
it is hermetic: no browser, no X display, no other agent's window.

Two phases, and the split matters:
  1. LOAD, up to and including STATUS. Sanity is asserted HERE - if the
     first render does not carry the page's own row, the fixture or the
     framing is wrong and the run says so instead of blaming the timer.
  2. Ten seconds of watching with no input. That is the KPI.
"""
import os
import re
import subprocess
import sys
import time

WINDOW = 10.0        # G1.1's window
MIN_UPDATES = 6      # G1.1's bar
TICK_RE = re.compile(rb"tick\s+(\d+)")

worker, page_js, dom = sys.argv[1:4]


def read_exactly(f, n, deadline):
    """Read n bytes or None on EOF/timeout."""
    out = b""
    while len(out) < n:
        if time.time() > deadline:
            return None
        ch = f.read(1)
        if not ch:
            return None
        out += ch
    return out


def read_frame(f, deadline):
    head = read_exactly(f, 6, deadline)
    if head is None:
        return None
    try:
        ln = int(head)
    except ValueError:
        return None
    body = read_exactly(f, ln, deadline)
    if body is None:
        return None
    # The trailing newline is part of the framing and must be consumed.
    # worker_dom_test.c's wreply does exactly this, and skipping it is what
    # made this probe fail in a way that looked like a dead worker: the next
    # header read started on that leftover newline, int() failed on
    # "\n00012" and the probe reported "never answered STATUS" after a
    # perfectly healthy keepalive.
    read_exactly(f, 1, deadline)
    # The worker emits a leading newline before some frame bodies: a
    # LIVE|drain| keepalive arrives as newline + "LIVE|drain|", so every
    # startswith() dispatch silently missed it and the keepalive looked
    # like an unexpected reply. Strip CR/LF here, once.
    #
    # Written with bytes([13, 10]) rather than a b"\r\n" literal on
    # purpose: one round of shell heredoc escaping turned b"\\r\\n" into
    # the four characters backslash-r-backslash-n, and lstrip() then
    # quietly stripped the letters r and n off every frame instead - a
    # test that passed because it was deleting what it was looking for.
    return body.lstrip(bytes([13, 10]))


def send(p, payload):
    try:
        p.stdin.write(b"%06d\n" % len(payload) + payload + b"\n")
        p.stdin.flush()
    except (BrokenPipeError, OSError):
        pass


def main():
    p = subprocess.Popen([worker], stdin=subprocess.PIPE,
                         stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
                         bufsize=0)
    try:
        return run(p)
    finally:
        send(p, b"QUIT\n")
        try:
            p.wait(timeout=5)
        except subprocess.TimeoutExpired:
            p.kill()


def run(p):
    debug = bool(os.environ.get("NB_TIMER_DEBUG"))
    frame = ("LOAD\n%s\n%s\nhttp://localhost/timer\nTimer fixture\n"
             % (page_js, dom)).encode()
    send(p, frame)

    # ---- phase 1: the initial load -------------------------------------
    renders, status = [], None
    deadline = time.time() + 20
    while time.time() < deadline:
        fr = read_frame(p.stdout, deadline)
        if fr is None:
            break
        if debug:
            print("  [frame] %r" % fr[:100])
        if fr.startswith(b"LIVE|"):
            continue
        if fr.startswith(b"RENDER\n"):
            renders.append(fr)
            continue
        status = fr
        break

    if status is None:
        print("FAIL: worker never answered STATUS (worker or harness problem)")
        return 1
    if not status.startswith(b"STATUS ok"):
        print("FAIL: worker said %r" % status[:120])
        return 1
    if not renders:
        print("FAIL: no RENDER frame during LOAD - the harness is not seeing the page")
        return 1

    first = TICK_RE.findall(renders[-1])
    if not first:
        print("FAIL: the first render carries no 'tick N' row, so the fixture or the "
              "framing is wrong - this is NOT evidence about the timer")
        return 1
    print("  harness sanity: first render carries 'tick %s' (the page ran during LOAD)"
          % first[-1].decode())
    print("  frames during LOAD: %d" % len(renders))

    # ---- phase 2: the KPI - ten seconds, no input ----------------------
    start = time.time()
    after = []
    while time.time() - start < WINDOW:
        fr = read_frame(p.stdout, start + WINDOW)
        if fr is None:
            time.sleep(0.05)
            continue
        if fr.startswith(b"RENDER\n"):
            after.append(fr)
        elif fr.startswith(b"STATUS"):
            break

    values = []
    for fr in after:
        m = TICK_RE.findall(fr)
        if m:
            values.append(int(m[-1]))
    uniq = sorted(set(values))
    elapsed = time.time() - start
    print("  watched %.1fs with no input: %d frames, %d distinct tick values %s"
          % (elapsed, len(after), len(uniq), uniq[:12]))

    bad = 0
    if len(uniq) < MIN_UPDATES:
        print("FAIL: G1.1 wants >= %d distinct self-updates in %.0fs, saw %d"
              % (MIN_UPDATES, WINDOW, len(uniq)))
        bad = 1
    if values and any(values[i] > values[i + 1] for i in range(len(values) - 1)):
        print("FAIL: tick values went backwards: %s" % values[:12])
        bad = 1
    if not bad:
        print("PASS: G1.1 live timer - %d distinct self-updates in %.1fs"
              % (len(uniq), elapsed))
    return bad


if __name__ == "__main__":
    sys.exit(main())