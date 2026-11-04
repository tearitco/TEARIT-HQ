#!/usr/bin/env python3
"""present_harness.py - drive the rpg-edit-hq board like a person, photograph every step, build a narrated MP4.

Reusable: the SCENARIO is a text file (presentation/scenarios/*.txt), one step per line, fields split by '|'. Lines starting with # are comments.
Doc: #.#.calendar-dox/!.HQ-IQ-BOOK/10-user-docs/PRESENTATION-VIDEO-PIPELINE.md  (section "Scripted drive harness").

  python3 present_harness.py <scenario.txt> [--out DIR] [--no-video] [--no-tts] [--no-restore] [--dry]

Every step runs behind cpu_gate (heavy-work rule); the live game is saved to slot 9 before and loaded back after (plus house.txt / voxel_log.txt
restored), so a run leaves the desk as it found it. A step that fails stops the run and writes FAIL into the receipt (no overclaiming).

Step vocabulary (all verified on 2026-11-04 except where noted):
  note|text                          caption-only marker, no frame
  book|<name>                        switch the board's book (pchq_board_action.sh load-map); waits for world_01 map_id
  verb|<verb> [args...]              run rpg_edit_hq.+x <verb> args (place item x y, voxel x y, goto room, control pet, walk, ...)
  drive|N                            N x `walk` verb steps (a pet that is blocked only bumps its step counter: use goto + moved for a visible move)
  armtile|kind|set|group|row_y       pal_side picks the tile list, then a relay click on the tile row at window y=row_y (default 265)
  xclick|x|y                         a REAL X click inside the board window (the only click that reaches the 3D canvas); checks the published pixel against the pointer marker's number and the picked cell against a projected round trip of the 3D raycast
  keys|text                          engine keys through bv_keys ( \\n = Enter, \\e = Esc )
  event|name[|trigger]               play events/<name> through events-hq play_event.sh (default trigger ui)
  mark|name / moved|name             remember the controlled pet's room,x,y / assert it changed since the mark
  clockto|HH                         advance the game clock to the next HH:00
  buyplace|item|x|y                  buy a catalog item and put it at x,y (verbs buy + place)
  brighter|shotA|shotB|ratio         assert the canvas of shot B is at least ratio x brighter than shot A
  pincam|yaw|pitch|dist / unpin      freeze the camera on the xelector cell (cam_look.txt) / release it
  calgrid|cols|rows|x0|y0|x1|y1      click a grid of pixels with the camera pinned, table -> calibration.txt
  collect|n|seconds                  wait for n clicks by a HUMAN with the real pointer, record each pick -> calibration.txt
  tap|x|y / rclick|x|y               relay tap / relay click at window pixels (a dropdown row needs rclick twice)
  hasrun|shot|x0|x1|y0|y1|min / norun|...|max   longest bright vertical run in a strip of a shot (scrollbar thumb detector; control and negative)
  wait|seconds
  expect|file|substring              (\\p = a literal | inside a field) file is relative to the app dir (state/..., or board/state/...); the step fails when the text is absent
  shot|name|caption                  window PNG -> snapshots/NN_name.png + a manifest row (hold = 6 s, narrated)
"""
import os, sys, time, subprocess, shutil, re, glob, datetime, shlex

HERE = os.path.dirname(os.path.abspath(__file__))
HOUSE = os.path.abspath(os.path.join(HERE, "..", "..", "..")) # 44.xyz.01.00
APP = os.path.join(HOUSE, "@.apps", "rpg-edit-hq")
SH = os.path.join(HOUSE, "&.widgits", "_shared-lib", "ops", "+x")
GATE, RELAY = os.path.join(SH, "cpu_gate.+x"), os.path.join(SH, "relay_drive.+x")
BVKEYS = os.path.join(HOUSE, "&.widgits", "board-viewer", "tools", "+x", "bv_keys.+x")
EDIT = os.path.join(APP, "ops", "+x", "rpg_edit_hq.+x")
PALSIDE = os.path.join(APP, "ops", "+x", "pal_side.+x")
ACTION = os.path.join(HOUSE, "@.apps", "piececraft-hq", "ops", "pchq_board_action.sh")
ARCHIVE = os.path.expanduser("~/Desktop/🧩️Piecemark-IT/中.SP_00.00/🗡️.crswrd.media-archive")
MKVIDEO = os.path.join(ARCHIVE, "make_presentation_video.py")

class Fail(Exception): pass

def gate(wait=120):
    r = subprocess.run([GATE, HOUSE, "check", "--wait", str(wait)], capture_output=True, text=True)
    if r.returncode != 0: raise Fail("cpu_gate refused: " + (r.stdout + r.stderr).strip()[-120:])

def run(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, text=True, **kw)

def rd(path, default=""):
    try: return open(path, errors="replace").read()
    except OSError: return default

def kv(path, key):
    for l in rd(path).splitlines():
        if l.startswith(key + "="): return l[len(key) + 1:]
    return ""

def ui_session(): return kv(os.path.join(APP, "board", "state", "ui.txt"), "bv_session")
def win_pid(): return rd(os.path.join(APP, "board", "module_parent.pid")).strip()
def find_win():
    r = run([RELAY, "findwin", "1300", "802"]); w = r.stdout.strip().split()
    if not w: raise Fail("no editor window (1300x802): launch it first (board_ctl recover)")
    run(["xdotool", "windowmap", w[0]]); time.sleep(1)      # an unmapped window cannot be photographed (X_GetImage BadMatch)
    return w[0]

class Harness:
    def __init__(self, scen, out, tts, video, restore, dry):
        self.scen, self.out, self.tts, self.video, self.restore, self.dry = scen, out, tts, video, restore, dry
        self.snaps, self.rows, self.log, self.n = os.path.join(out, "snapshots"), [], [], 0
        os.makedirs(self.snaps, exist_ok=True)
        self.had_house = os.path.exists(os.path.join(APP, "state", "house.txt"))
        self.voxel_bak = rd(os.path.join(APP, "state", "voxel_log.txt"))
        self.BRUSH = os.path.join(APP, "board", "state", "brush.txt"); self.brush_bak = rd(self.BRUSH) if os.path.exists(self.BRUSH) else None      # armtile leaves the brush armed: restore it (found 2026-11-04, the armed brush shortened the tile list)
        self.ids0 = self.placed_ids()
    def placed_ids(self):
        return {l.split("|")[1] for l in rd(os.path.join(APP, "state", "house.txt")).splitlines() if l.startswith("PLACE|") and l.count("|") > 1}
    def say(self, m): self.log.append(m); print(m, flush=True)

    def op(self, name, a):
        if name == "note": self.say("NOTE " + a[0]); return
        if self.dry: self.say("DRY  %s %s" % (name, a)); return
        if name not in ("wait", "note"): gate()
        getattr(self, "op_" + name)(*a)

    def op_book(self, nm):
        r = run(["sh", ACTION, ui_session(), "load-map", nm]); time.sleep(2)
        for _ in range(40):
            if kv(os.path.join(APP, "rpg-edit-hq-pchq", "pieces", "world_01", "state.txt"), "map_id") == nm: self.say("book -> " + nm); return
            time.sleep(2)
        raise Fail("book did not switch to " + nm)
    def op_verb(self, line):
        r = run([EDIT] + shlex.split(line)); self.say("verb %s -> %s" % (line, (r.stdout.strip().splitlines() or ["(no output)"])[-1][:100]))
    def op_drive(self, n):
        for i in range(int(n)):
            r = run([EDIT, "walk"]); time.sleep(0.8)
        self.say("drive %s steps; last: %s" % (n, (r.stdout.strip().splitlines() or ["?"])[-1][:80]))
    def op_armtile(self, kind, sset, group, row_y="265"):
        for w in ("__list__", kind, sset, group):
            run([PALSIDE, HOUSE, "list" if w == "__list__" else "pick"] + ([] if w == "__list__" else [w])); time.sleep(0.5)
        time.sleep(5); run([RELAY, win_pid(), "tap", "95", str(row_y)]); time.sleep(6)
        b = rd(os.path.join(APP, "board", "state", "brush.txt"))
        if "glyph=" not in b: raise Fail("no brush armed after clicking the tile row (row_y=%s)" % row_y)
        self.say("armed: " + [l for l in b.splitlines() if l.startswith("glyph=")][0])
    def op_xclick(self, x, y, tol=None):
        """A REAL X click, then two alignment checks (owner 2026-11-04: every mouse test must agree with the pointer marker and the 3D raycast):
        1. the number the window published (canvas_click.txt) = the commanded pixel minus a canvas origin that is the same for every click of the run (learned on the first click);
        2. the engine's pick, projected back through the same camera (click_proj.txt: pixel -> ray -> cell -> pixel), puts the click INSIDE the picked cell's projected box.
        A miss (ray=miss) is reported, not failed: an off-map click is a legitimate test input."""
        w = find_win(); sess = ui_session(); disp = os.path.join(sess, "pieces", "display")
        proj = os.path.join(APP, "rpg-edit-hq-pchq", "pieces", "display", "click_proj.txt")
        try: os.remove(proj)
        except OSError: pass
        run(["xdotool", "mousemove", "--window", w, str(x), str(y)]); time.sleep(0.3); run(["xdotool", "click", "1"]); time.sleep(4)
        x, y = int(x), int(y)
        pub = rd(os.path.join(disp, "canvas_click.txt")).split()
        if len(pub) < 2: raise Fail("xclick: the window published no canvas click")
        org = (x - int(pub[0]), y - int(pub[1])); self.origin = getattr(self, "origin", None) or org
        skew = int(os.environ.get("PRESENT_NEG_SKEW", "0"))      # negative test of this very check: PRESENT_NEG_SKEW=5 must make the run FAIL
        if abs(org[0] - self.origin[0] - skew * (self.n_x if hasattr(self, 'n_x') else 0)) > 1 or abs(org[1] - self.origin[1]) > 1:
            raise Fail("xclick: published click %s for pixel %d,%d implies canvas origin %s, the run's origin is %s (the pointer marker and the published click disagree)" % (pub[:2], x, y, org, self.origin))
        self.n_x = getattr(self, 'n_x', 0) + 1
        pr = rd(proj)
        if not pr: self.say("real click at %d,%d: published %s (origin %s ok); ray MISS (no cell picked)" % (x, y, pub[:2], self.origin)); return
        g = lambda k: next((l.split("=", 1)[1] for l in pr.splitlines() if l.startswith(k + "=")), "?")
        if g("inside") != "1": raise Fail("xclick: raycast round trip failed: click %s is outside the projected box %s of picked cell %s (err_px %s)" % (g("click_px"), g("cell_box"), g("cell"), g("err_px")))
        self.say("real click at %d,%d: published %s (origin %s ok); picked cell %s; round trip inside box %s, centre off by %s px" % (x, y, pub[:2], self.origin, g("cell"), g("cell_box"), g("err_px")))
    CAMF = os.path.join(APP, "rpg-edit-hq-pchq", "pieces", "system", "cam_look.txt")
    PROJ = os.path.join(APP, "rpg-edit-hq-pchq", "pieces", "display", "click_proj.txt")
    def op_pincam(self, yaw="180", pitch="-50", dist="16"):
        """freeze the camera on the xelector's cell (the engine's debug camera, cam_look.txt: look_on=1, no look_sel) so clicks do not move the view under the pointer"""
        xp = os.path.join(APP, "rpg-edit-hq-pchq", "pieces", "xelector_01", "state.txt")
        px = int(kv(xp, "pos_x") or 8); py = int(kv(xp, "pos_y") or 8)
        os.makedirs(os.path.dirname(self.CAMF), exist_ok=True)
        open(self.CAMF, "w").write("look_on=1\nat_x=%.1f\nat_y=1.0\nat_z=%.1f\nlook_dist=%s\nlook_yaw=%s\nlook_pitch=%s\n" % (px + 0.5, py + 0.5, dist, yaw, pitch))
        time.sleep(4); self.say("camera pinned on cell %d,%d (yaw %s pitch %s dist %s)" % (px, py, yaw, pitch, dist))
    def op_unpin(self):
        try: os.remove(self.CAMF)
        except OSError: pass
        self.say("camera pin removed")
    def op_calgrid(self, cols="4", rows="3", x0="330", y0="260", x1="900", y1="650"):
        """click a cols x rows grid of pixels with the camera pinned; every click must pass the xclick checks; the table (pixel, picked cell, centre offset) goes to calibration.txt"""
        tab = ["# px | py | published | cell | inside | centre_off_px"]
        for r in range(int(rows)):
            for c in range(int(cols)):
                x = int(x0) + (int(x1) - int(x0)) * c // max(int(cols) - 1, 1); y = int(y0) + (int(y1) - int(y0)) * r // max(int(rows) - 1, 1)
                self.op_xclick(x, y); pr = rd(self.PROJ); g = lambda k: next((l.split("=", 1)[1] for l in pr.splitlines() if l.startswith(k + "=")), "-")
                tab.append("%d | %d | %s | %s | %s | %s" % (x, y, g("click_px"), g("cell"), g("inside"), g("err_px")))
        open(os.path.join(self.out, "calibration.txt"), "w").write("\n".join(tab) + "\n"); self.say("calibration table: %d clicks -> calibration.txt" % (len(tab) - 1))
    def op_collect(self, n="6", seconds="180"):
        """YOUR clicks: wait until you have clicked n times in the window (real pointer, pointer marker on); record each pick (click pixel, cell, inside, centre offset) in calibration.txt"""
        tab = ["# your click px | cell | inside | centre_off_px"]; seen = rd(self.PROJ); t0 = time.time(); self.say("COLLECT: click %s times in the board window now (%s s)" % (n, seconds))
        while len(tab) - 1 < int(n) and time.time() - t0 < float(seconds):
            time.sleep(0.5); cur = rd(self.PROJ)
            if cur and cur != seen:
                seen = cur; g = lambda k: next((l.split("=", 1)[1] for l in cur.splitlines() if l.startswith(k + "=")), "-")
                tab.append("%s | %s | %s | %s" % (g("click_px"), g("cell"), g("inside"), g("err_px"))); self.say("your click %d: pixel %s -> cell %s inside=%s centre off %s px" % (len(tab) - 1, g("click_px"), g("cell"), g("inside"), g("err_px")))
        open(os.path.join(self.out, "calibration.txt"), "w").write("\n".join(tab) + "\n")
        if len(tab) - 1 < int(n): raise Fail("collect: only %d of %s clicks arrived" % (len(tab) - 1, n))
    def op_clickplace(self, x, y):
        """click a cell with a real mouse click, press Enter, and retry on neighbouring pixels while the editor says 'not placed' (someone standing there, off the floor). The camera follows the xelector, so one pixel is never the same cell twice."""
        lg = os.path.join(APP, "state", "voxel_log.txt")
        for dx, dy in ((0, 0), (70, 0), (-70, 0), (0, 45), (0, -45), (110, 30)):
            before = len(rd(lg).splitlines()); self.op_xclick(int(x) + dx, int(y) + dy); self.op_keys("\\n"); time.sleep(2)
            new = rd(lg).splitlines()[before:]
            if any(l.startswith("placed ") for l in new): self.say("placed on attempt at offset %d,%d: %s" % (dx, dy, new[-1])); return
            self.say("not placed at offset %d,%d (%s); trying the next cell" % (dx, dy, (new or ["no reply"])[-1][:60]))
        raise Fail("clickplace: no cell accepted the brush after 6 clicks")
    def op_keys(self, text):
        r = run([BVKEYS, ui_session(), text.replace("\\\\", "\\")]); time.sleep(7); self.say("keys %r -> %s" % (text, r.stdout.strip()[:40]))
    def op_event(self, name, trig="ui"):
        pl = os.path.join(HOUSE, "&.widgits", "events-hq", "ops", "play_event.sh"); wd = os.path.join(APP, "state", "world"); os.makedirs(wd, exist_ok=True)
        env = dict(os.environ, MUCHI_TARGET_ENT=wd)
        r = subprocess.run(["bash", pl, os.path.join(APP, "events", name), HOUSE, trig], capture_output=True, text=True, env=env, timeout=60)
        time.sleep(2); self.say("event %s (%s) rc=%d" % (name, trig, r.returncode))
        if r.returncode != 0: raise Fail("event %s failed rc=%d" % (name, r.returncode))
    def petpos(self):
        """room,x,y of the controlled character: state/pet_<active>.txt (state/active.txt names it; `goto <room>` changes it, `walk` only bumps a step counter when the pet is blocked)"""
        act = (rd(os.path.join(APP, "state", "active.txt")).split() or ["?"])[0]
        t = rd(os.path.join(APP, "state", "pet_%s.txt" % act)); g = lambda k: next((l.split("=", 1)[1] for l in t.splitlines() if l.startswith(k + "=")), "?")
        return "%s@%s,%s,%s" % (act, g("room"), g("x"), g("y"))
    def op_mark(self, nm):
        self.marks = getattr(self, "marks", {}); self.marks[nm] = self.petpos(); self.say("mark %s = %s" % (nm, self.marks[nm]))
    def op_moved(self, nm):
        now, was = self.petpos(), getattr(self, "marks", {}).get(nm)
        if was is None: raise Fail("moved: no mark named " + nm)
        if now == was: raise Fail("moved: the controlled pet is still at %s since mark %s" % (now, nm))
        self.say("moved ok: %s -> %s" % (was, now))
    def op_clockto(self, hh):
        """advance the game clock to the next HH:00 (clock verbs only go forward; the run's final load puts the clock back)"""
        m = re.search(r"(\d\d):(\d\d)", rd(os.path.join(APP, "state", "clock_ui.txt")))
        if not m: raise Fail("clockto: no time_label in clock_ui.txt")
        h, mi = int(m.group(1)), int(m.group(2)); mins = ((int(hh) - h) * 60 - mi) % (24 * 60) or 24 * 60
        r = run([EDIT, "clock", "advance", "%dm" % mins]); time.sleep(6); self.say("clock -> %s:00 (%s)" % (hh, (r.stdout.strip().splitlines() or ["?"])[-1][:50]))
    def op_buyplace(self, key, x, y):
        r = run([EDIT, "buy", key]); out = r.stdout.strip().splitlines()[-1] if r.stdout.strip() else ""
        m = re.search(r"#(\d+) at", out)
        if not m: raise Fail("buyplace: %r" % out[:80])
        r2 = run([EDIT, "place", m.group(1), str(x), str(y)]); time.sleep(3); self.say("bought %s as #%s, %s" % (key, m.group(1), (r2.stdout.strip().splitlines() or ["?"])[-1][:60]))
    def op_brighter(self, a, b, ratio):
        from PIL import Image
        def mean(nm):
            f = next((x for x in sorted(os.listdir(self.snaps)) if x.endswith("_" + nm + ".png")), None)
            if not f: raise Fail("brighter: no shot named " + nm)
            im = Image.open(os.path.join(self.snaps, f)).convert("L").crop((208, 105, 1094, 745)); return sum(im.tobytes()) / float(im.size[0] * im.size[1])
        ma, mb = mean(a), mean(b)
        if mb < ma * float(ratio): raise Fail("brighter: %s=%.1f then %s=%.1f is not %.2fx brighter" % (a, ma, b, mb, float(ratio)))
        self.say("brighter ok: %s %.1f -> %s %.1f (x%.2f)" % (a, ma, b, mb, mb / max(ma, 0.01)))
    def op_rclick(self, x, y):
        """relay click (a menu row needs TWO: the first focuses it, the second activates)"""
        run([RELAY, win_pid(), "click", str(x), str(y)]); time.sleep(3); self.say("relay click %s,%s" % (x, y))
    def op_tap(self, x, y):
        run([RELAY, win_pid(), "tap", str(x), str(y)]); time.sleep(3); self.say("relay tap %s,%s" % (x, y))
    def _run_len(self, nm, x0, x1, y0, y1, th=110):
        """longest vertical run of bright pixels (> th) in columns x0..x1 of shot nm: a scrollbar thumb is a long run, text never is"""
        from PIL import Image
        f = next((x for x in sorted(os.listdir(self.snaps)) if x.endswith("_" + nm + ".png")), None)
        if not f: raise Fail("no shot named " + nm)
        im = Image.open(os.path.join(self.snaps, f)).convert("L"); best = 0
        for x in range(int(x0), int(x1)):
            r = 0
            for y in range(int(y0), int(y1)):
                if im.getpixel((x, y)) > th: r += 1; best = max(best, r)
                else: r = 0
        return best
    def op_hasrun(self, nm, x0, x1, y0, y1, minrun):
        b = self._run_len(nm, x0, x1, y0, y1)
        if b < int(minrun): raise Fail("hasrun: %s longest bright run %d < %s (control failed: no thumb to hide)" % (nm, b, minrun))
        self.say("hasrun ok: %s run %d >= %s" % (nm, b, minrun))
    def op_norun(self, nm, x0, x1, y0, y1, maxrun):
        b = self._run_len(nm, x0, x1, y0, y1)
        if b > int(maxrun): raise Fail("norun: %s has a bright vertical run of %d > %s (a scrollbar thumb shows through the dropdown)" % (nm, b, maxrun))
        self.say("norun ok: %s longest run %d <= %s" % (nm, b, maxrun))
    def op_wait(self, s): time.sleep(float(s))
    def op_expect(self, f, sub):
        p = os.path.join(APP, f)
        if sub not in rd(p): raise Fail("expect: %r not in %s (last line: %r)" % (sub, f, (rd(p).strip().splitlines() or [""])[-1][:90]))
        self.say("expect ok: %r in %s" % (sub, f))
    def op_shot(self, nm, caption):
        try: run([BVKEYS, ui_session(), "~"]); time.sleep(5)      # an idle engine only redraws when woken: a changed lights.pdl or event shows after a nudge
        except Exception: pass
        self.n += 1; fn = "%02d_%s.png" % (self.n, re.sub(r"\W+", "_", nm))
        r = run([RELAY, "shot", find_win(), os.path.join(self.snaps, fn)])
        if not os.path.exists(os.path.join(self.snaps, fn)): raise Fail("shot failed: " + r.stderr[-100:])
        self.rows.append("%s | 6 | %s" % (fn, caption.replace("|", "/"))); self.say("shot " + fn)

    def go(self):
        steps = []
        for l in open(self.scen, encoding="utf-8"):
            l = l.rstrip("\n")
            if l.strip() and not l.lstrip().startswith("#"): p = [x.strip().replace("\\p", "|") for x in l.split("|")]; steps.append((p[0], p[1:]))      # \p in a field = a literal |
        status = "OK"
        try:
            if not self.dry:
                gate(); run([EDIT, "save", "9"]); self.say("saved the live game to slot 9 (restored at the end)")
            for name, a in steps: self.op(name, a)
        except Fail as e: status = "FAIL: %s" % e; self.say(status)
        except Exception as e: status = "ERROR: %r" % e; self.say(status)
        finally:
            if self.restore and not self.dry:
                try:
                    try: os.remove(self.CAMF)
                    except OSError: pass
                    run([EDIT, "lamps", "off"])      # lamps_on.txt is not part of a save
                    hp = os.path.join(APP, "state", "house.txt")
                    for i in sorted(self.placed_ids() - self.ids0): run([EDIT, "remove", i]); self.say("removed the test item #" + i)      # the daemon keeps its own list: take the items out in-game first
                    time.sleep(4)
                    if not self.had_house:      # remove it BEFORE the load: the load replays house.txt into the daemon's own list, and a leftover row would be written back
                        try: os.remove(hp)
                        except OSError: pass
                    run([EDIT, "load", "9"]); time.sleep(6)
                    if not self.had_house and os.path.exists(hp):
                        self.say("house.txt came back after the load (%d bytes): the run is NOT fully restored" % os.path.getsize(hp))
                    open(os.path.join(APP, "state", "voxel_log.txt"), "w").write(self.voxel_bak)
                    shutil.rmtree(os.path.join(APP, "state", "slots", "9"), ignore_errors=True)
                    if self.brush_bak is None:
                        try: os.remove(self.BRUSH)
                        except OSError: pass
                    else: open(self.BRUSH, "w").write(self.brush_bak)
                    self.say("restored: slot 9 loaded back, house.txt/voxel_log.txt as before, slot 9 removed")
                except Exception as e: self.say("RESTORE PROBLEM: %r (check by hand)" % e)
        open(os.path.join(self.out, "manifest.txt"), "w", encoding="utf-8").write("# written by present_harness.py from %s\n" % os.path.basename(self.scen) + "\n".join(self.rows) + "\n")
        open(os.path.join(self.out, "REPRODUCE.md"), "w", encoding="utf-8").write("# Reproduce\n\n`python3 &.widgits/_shared-lib/presentation/present_harness.py %s`\n\nStatus of this run: **%s**\n\n```\n%s\n```\n" % (os.path.relpath(self.scen, HOUSE), status, "\n".join(self.log)))
        if status == "OK" and self.video and self.rows and not self.dry:
            if not os.path.exists(MKVIDEO): self.say("no make_presentation_video.py at " + MKVIDEO)
            else:
                cmd = [GATE, HOUSE, "run", "present-video", "--wait", "300", "--", "env", "PRESENT_TTS=" + os.path.join(HERE, "remote_tts.py"), "OMP_NUM_THREADS=1", "PET_TTS_THREADS=1", "MKL_NUM_THREADS=1", "OPENBLAS_NUM_THREADS=1", "TORCH_NUM_THREADS=1", "nice", "-n", "19", "ionice", "-c3", "python3", MKVIDEO, self.out, "--fps", "30", "--width", "1280"] + ([] if self.tts else ["--no-tts"])
                r = run(cmd); self.say("video rc=%d %s" % (r.returncode, (r.stdout + r.stderr).strip()[-160:]))
        return status

def main():
    a = sys.argv[1:]
    if not a or a[0].startswith("-"): print(__doc__); return 2
    scen = os.path.abspath(a[0]); out = None
    if "--out" in a: out = a[a.index("--out") + 1]
    if not out: out = os.path.join(ARCHIVE, datetime.date.today().strftime("%B-%d"), os.path.splitext(os.path.basename(scen))[0] + "-" + datetime.datetime.now().strftime("%Y%m%d-%H%M%S"))
    h = Harness(scen, out, "--no-tts" not in a, "--no-video" not in a, "--no-restore" not in a, "--dry" in a)
    st = h.go(); print("STATUS:", st, "->", out); return 0 if st == "OK" else 1

if __name__ == "__main__": sys.exit(main())
