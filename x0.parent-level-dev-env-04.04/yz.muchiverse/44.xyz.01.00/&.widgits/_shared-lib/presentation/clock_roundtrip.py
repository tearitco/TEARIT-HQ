#!/usr/bin/env python3
"""clock_roundtrip.py - save/load must restore the game clock on every petslike skin (fix 2026-11-04: load used to leave the clock ahead).
  python3 clock_roundtrip.py [skin ...]      default: every @.apps skin built from petslike_core
Per skin (verbs only: no window, no engine, one fresh process per call, niced): save slot 9, advance the clock 3 h, load slot 9, assert
game_time_epoch_ms in state/clock_root/#.desktop/clocks/rpg.pdl is back to the saved value (NOT clock_ui.txt: that label is only rewritten by a running clock
daemon, so it can look stale). A control step asserts the advance really moved the clock, so a no-op advance cannot make the test pass.
Refuses a skin whose slot 9 already exists (never overwrites a player's slot); removes the slot it made. Exit 1 on any FAIL."""
import os, sys, subprocess, shutil, time
HERE = os.path.dirname(os.path.abspath(__file__)); HOUSE = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
SKINS = ["cdda-hq", "civ-rpgce", "df-hq", "dsr-rpg", "gta-hq", "mcl-hq", "rpg-dust", "rpg-pet", "rpg-pvp-hq", "rpg-space", "tcg-rpgce", "rpg-edit-hq"]
def ms(app):
    for l in open(os.path.join(app, "state", "clock_root", "#.desktop", "clocks", "rpg.pdl")):
        if l.startswith("game_time_epoch_ms="): return int(l.split("=", 1)[1])
    raise RuntimeError("no game_time_epoch_ms")
def verb(app, binn, *a):
    r = subprocess.run(["nice", "-n", "15", binn] + list(a), cwd=app, capture_output=True, text=True, timeout=60); return (r.stdout + r.stderr).strip()
bad = 0
for s in (sys.argv[1:] or SKINS):
    app = os.path.join(HOUSE, "@.apps", s); binn = os.path.join(app, "ops", "+x", s.replace("-", "_") + ".+x"); slot = os.path.join(app, "state", "slots", "9")
    try:
        if not os.path.exists(binn): raise RuntimeError("no binary " + binn)
        if os.path.exists(slot): raise RuntimeError("slot 9 already exists: not overwriting")
        verb(app, binn, "save", "9")      # no advance before the save: the test must leave the clock where it found it
        t0 = int(open(os.path.join(slot, "clock_ms.txt")).read().strip() or 0)
        if t0 <= 0:      # the clock was never started in this game (no clock_root yet): there is nothing to restore, and the test must not start one
            print("SKIP %-12s no clock started yet (slot 9 holds no clock_ms); not a failure" % s); continue
        verb(app, binn, "clock", "advance", "3h"); time.sleep(1); t1 = ms(app)
        if t1 <= t0: raise RuntimeError("control failed: advance 3h did not move the clock (%d -> %d)" % (t0, t1))
        verb(app, binn, "load", "9"); time.sleep(1); t2 = ms(app)
        if t2 != t0: raise RuntimeError("load left the clock at %d, saved value was %d" % (t2, t0))
        print("OK   %-12s saved %d, advanced to %d, load restored %d" % (s, t0, t1, t2))
    except Exception as e:
        bad += 1; print("FAIL %-12s %s" % (s, e))
    finally:
        if os.path.isdir(slot) and "already exists" not in str(sys.exc_info()[1] or ""): shutil.rmtree(slot, ignore_errors=True)
sys.exit(1 if bad else 0)
