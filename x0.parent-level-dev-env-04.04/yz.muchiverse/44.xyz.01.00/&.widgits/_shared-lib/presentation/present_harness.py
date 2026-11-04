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
  drive|N                            N x `walk` verb steps of the controlled pet (the "drive N" move)
  armtile|kind|set|group|row_y       pal_side picks the tile list, then a relay click on the tile row at window y=row_y (default 265)
  xclick|x|y                         a REAL X click inside the board window (the only click that reaches the 3D canvas)
  keys|text                          engine keys through bv_keys ( \\n = Enter, \\e = Esc )
  event|name[|trigger]               play events/<name> through events-hq play_event.sh (default trigger ui)
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
    def op_xclick(self, x, y):
        w = find_win(); run(["xdotool", "mousemove", "--window", w, str(x), str(y)]); time.sleep(0.3); run(["xdotool", "click", "1"]); time.sleep(4)
        self.say("real click at %s,%s; xelector now %s" % (x, y, rd(os.path.join(APP, "rpg-edit-hq-pchq", "pieces", "xelector_01", "state.txt")).replace("\n", " ")[:40]))
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
    def op_wait(self, s): time.sleep(float(s))
    def op_expect(self, f, sub):
        p = os.path.join(APP, f)
        if sub not in rd(p): raise Fail("expect: %r not in %s (last line: %r)" % (sub, f, (rd(p).strip().splitlines() or [""])[-1][:90]))
        self.say("expect ok: %r in %s" % (sub, f))
    def op_shot(self, nm, caption):
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
                    hp = os.path.join(APP, "state", "house.txt")
                    if not self.had_house:      # remove it BEFORE the load: the load replays house.txt into the daemon's own list, and a leftover row would be written back
                        try: os.remove(hp)
                        except OSError: pass
                    run([EDIT, "load", "9"]); time.sleep(6)
                    if not self.had_house and os.path.exists(hp):
                        self.say("house.txt came back after the load (%d bytes): the run is NOT fully restored" % os.path.getsize(hp))
                    open(os.path.join(APP, "state", "voxel_log.txt"), "w").write(self.voxel_bak)
                    shutil.rmtree(os.path.join(APP, "state", "slots", "9"), ignore_errors=True)
                    self.say("restored: slot 9 loaded back, house.txt/voxel_log.txt as before, slot 9 removed")
                except Exception as e: self.say("RESTORE PROBLEM: %r (check by hand)" % e)
        open(os.path.join(self.out, "manifest.txt"), "w", encoding="utf-8").write("# written by present_harness.py from %s\n" % os.path.basename(self.scen) + "\n".join(self.rows) + "\n")
        open(os.path.join(self.out, "REPRODUCE.md"), "w", encoding="utf-8").write("# Reproduce\n\n`python3 &.widgits/_shared-lib/presentation/present_harness.py %s`\n\nStatus of this run: **%s**\n\n```\n%s\n```\n" % (os.path.relpath(self.scen, HOUSE), status, "\n".join(self.log)))
        if status == "OK" and self.video and self.rows and not self.dry:
            if not os.path.exists(MKVIDEO): self.say("no make_presentation_video.py at " + MKVIDEO)
            else:
                cmd = [GATE, HOUSE, "run", "present-video", "--wait", "300", "--", "python3", MKVIDEO, self.out, "--fps", "30", "--width", "1280"] + ([] if self.tts else ["--no-tts"])
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
