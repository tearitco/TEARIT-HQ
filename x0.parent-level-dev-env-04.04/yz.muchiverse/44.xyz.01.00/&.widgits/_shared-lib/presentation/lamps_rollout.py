#!/usr/bin/env python3
"""lamps_rollout.py - the shared lamp system (petslike_core.c lamps_export_room) works in every skin that has a lamp item + lamps.pdl. Verb-only, no window.
  python3 lamps_rollout.py [skin ...]
Per skin: save slot 9; buy a lamp (verb buy lamp); `lamps on` -> the room's 3D-book lights.pdl must hold a LIGHT row at the lamp's cell; `lamps off` -> the file must be back
to what it was before (absent, or the skin's own rows); then the game is loaded back from slot 9 and the room's light files, lamps_on.txt and slot 9 are put back. Needs a live
control: the 'on' step fails when the lamp row is missing, the 'off' step when it is still there. Refuses a skin whose slot 9 exists. Exit 1 on any FAIL."""
import os, sys, glob, shutil, subprocess, re, time
HERE = os.path.dirname(os.path.abspath(__file__)); HOUSE = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
SKINS = ["cdda-hq", "df-hq", "dsr-rpg", "gta-hq", "mcl-hq", "rpg-dust", "rpg-pet", "rpg-space", "rpg-edit-hq", "civ-rpgce", "tcg-rpgce"]
def rd(p):
    try: return open(p, errors="replace").read()
    except OSError: return None
bad = 0
for s in (sys.argv[1:] or SKINS):
    app = os.path.join(HOUSE, "@.apps", s); binn = os.path.join(app, "ops", "+x", s.replace("-", "_") + ".+x"); slot = os.path.join(app, "state", "slots", "9")
    on_p = os.path.join(app, "state", "lamps_on.txt"); on_was = rd(on_p); made = False; lights = {}
    def v(*a): return subprocess.run(["nice", "-n", "15", binn] + list(a), cwd=app, capture_output=True, text=True, timeout=90).stdout.strip()
    try:
        if not os.path.exists(os.path.join(app, "lamps.pdl")): raise RuntimeError("no lamps.pdl")
        if os.path.exists(slot): raise RuntimeError("slot 9 already exists: not overwriting")
        maps = glob.glob(os.path.join(app, s + "-pchq", "pieces", "system", "maps", "*", "*", "lights.pdl"))
        for f in glob.glob(os.path.join(app, s + "-pchq", "pieces", "system", "maps", "*", "*")):
            lights[f] = (rd(os.path.join(f, "lights.pdl")), rd(os.path.join(f, "lights.lamps")))      # every room's light files as found
        v("save", "9"); made = True
        out = v("buy", "lamp"); m = re.search(r"#(\d+) at (\d+),(\d+)", out)
        if not m: raise RuntimeError("buy lamp: " + out[:80])
        x, y = m.group(2), m.group(3); time.sleep(1)
        v("lamps", "on"); time.sleep(1)
        hit = [f for f in lights if re.search(r"LIGHT \| %s \| %s \|" % (x, y), rd(os.path.join(f, "lights.pdl")) or "")]
        if not hit: raise RuntimeError("lamps on: no LIGHT row at %s,%s in any room (bought: %s)" % (x, y, out[:60]))
        v("lamps", "off"); time.sleep(1)
        still = [f for f in hit if re.search(r"LIGHT \| %s \| %s \|" % (x, y), rd(os.path.join(f, "lights.pdl")) or "")]
        if still: raise RuntimeError("lamps off: the lamp row is still in " + still[0])
        print("OK   %-12s lamp at %s,%s: lights.pdl row present on, gone off" % (s, x, y))
    except Exception as e:
        bad += 1; print("FAIL %-12s %s" % (s, e))
    finally:
        if made: v("load", "9")
        shutil.rmtree(slot, ignore_errors=True)
        try:
            if on_was is None: os.remove(on_p)
            else: open(on_p, "w").write(on_was)
        except OSError: pass
        v("export3d")      # rebuild the 3D book from the restored game (lamps back to their old state)
        for f, (lp, ll) in lights.items():      # and make sure the room files are exactly as found
            for name, val in (("lights.pdl", lp), ("lights.lamps", ll)):
                q = os.path.join(f, name)
                if val is None:
                    if os.path.exists(q): os.remove(q)
                elif rd(q) != val: open(q, "w").write(val)
sys.exit(1 if bad else 0)
