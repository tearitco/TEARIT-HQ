#!/usr/bin/env python3
"""run_suite.py - run the regression scenarios one at a time (no video, no TTS) and print OK/FAIL per scenario.
  python3 run_suite.py [name ...]     default: place_and_move lamps_at_dusk mouse_align
Each scenario runs inside present_harness.py (cpu_gate, desk saved and restored). One at a time; stops nothing on FAIL, exit 1 if any failed.
Drives the real editor window: tell the owner before running."""
import os, sys, subprocess
H = os.path.dirname(os.path.abspath(__file__))
names = sys.argv[1:] or ["place_and_move", "lamps_at_dusk", "mouse_align", "dropdown_thumb"]
bad = 0
for n in names:
    sc = os.path.join(H, "scenarios", n + ".txt")
    if not os.path.exists(sc): print("MISSING", n); bad += 1; continue
    out = "/tmp/claude-1000/suite_" + n
    r = subprocess.run(["nice", "-n", "15", sys.executable, os.path.join(H, "present_harness.py"), sc, "--out", out, "--no-video", "--no-tts"],
                       capture_output=True, text=True)
    ok = r.returncode == 0
    print(("OK   " if ok else "FAIL ") + n)
    if not ok: bad += 1; print((r.stdout + r.stderr)[-400:])
if not sys.argv[1:]:      # verb-only check, no window: save/load restores the clock on every skin
    r = subprocess.run([sys.executable, os.path.join(H, "clock_roundtrip.py")], capture_output=True, text=True)
    print(("OK   " if r.returncode == 0 else "FAIL ") + "clock_roundtrip"); print(r.stdout.strip())
    bad += r.returncode != 0
sys.exit(1 if bad else 0)
