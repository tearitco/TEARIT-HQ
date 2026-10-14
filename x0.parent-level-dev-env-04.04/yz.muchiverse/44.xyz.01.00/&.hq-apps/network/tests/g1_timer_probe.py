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
    """One frame, in the worker's exact order (nb_js_worker.c: send is
    "%.6d\\n" + payload + "\\n"; worker_dom_test.c's wreply reads it back the
    same way).

    The subtlety that cost this probe an entire session: the length
    header is SIX DIGITS PLUS A NEWLINE, so reading a fixed 6 bytes leaves
    that newline in the stream and every subsequent read is off by one.
    The first frame still looked right, because a defensive lstrip() ate
    the stray newline - which is exactly how a harness hides its own bug.
    So: read up to the newline, then the body, then the trailing newline,
    and do NOT strip anything from the body.
    """
    digits = b""
    while len(digits) < 32:
        if time.time() > deadline:
            return None
        ch = f.read(1)
        if not ch:
            return None
        if ch == b"\n":
            break
        digits += ch
    else:
        return None
    try:
        ln = int(digits)
    except ValueError:
        return None
    body = read_exactly(f, ln, deadline)
    if body is None:
        return None
    read_exactly(f, 1, deadline)      # the framing's trailing newline
    return body


def send(p, payload):
    try:
        p.stdin.write(b"%06d\n" % len(payload) + payload + b"\n")
        p.stdin.flush()
    except (BrokenPipeError, OSError):
        pass


def dom_diagnostics(p):
    """A DOM that will not load must SAY SO (2026-10-13).

    Before this, a fetch.dom the worker could not open - or could not
    deserialize, since nb_dom_load reads the nb_serialize wire format and
    not HTML - left g_dom_root NULL and the page rendered as its title
    and nothing else, with STATUS ok. Four commits of the G1 harness were
    spent proving that an HTML fixture is not a DOM, and every run said
    "ok" while rendering nothing.

    So: LOAD a page whose fetch.dom does not exist, and require an ERROR
    frame naming the problem. This is the assertion that makes the silence
    a bug rather than a mystery.
    """
    bad = "/tmp/nb_definitely_not_here.dom"
    frame = ("LOAD\n%s\n%s\nhttp://localhost/x\nX\n\n"
             % (os.path.abspath(page_js), bad)).encode()
    send(p, frame)
    deadline = time.time() + 20
    while time.time() < deadline:
        fr = read_frame(p.stdout, deadline)
        if fr is None:
            break
        if fr.startswith(b"ERROR|"):
            return fr.decode("utf-8", "replace")
        if fr.startswith(b"STATUS"):
            break
    return None


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
    diag = dom_diagnostics(p)
    if diag:
        print("  DOM diagnostics: %s" % diag[:110])
        print("PASS: an unloadable DOM is reported, not swallowed")
    else:
        print("FAIL: a fetch.dom that cannot be opened produced no ERROR frame - "
              "the page would render as its title only, silently")
        return 1

    # Six fields, not five: the worker reads f[5] as the page's CSS
    # (nb_js_worker.c's LOAD handler), and the manager sends
    # js/dom/href/title/css. ABSOLUTE paths too - relative ones resolve
    # against whatever CWD the worker inherited, which is not a thing to
    # depend on and is exactly how a DOM can go missing while every status
    # line still says ok.
    frame = ("LOAD\n%s\n%s\nhttp://localhost/timer\nTimer fixture\n%s\n"
             % (os.path.abspath(page_js), os.path.abspath(dom), "")).encode()
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
            print("  [len ] %d" % len(fr))
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