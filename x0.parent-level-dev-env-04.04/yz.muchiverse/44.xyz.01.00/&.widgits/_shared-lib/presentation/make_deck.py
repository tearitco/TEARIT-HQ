#!/usr/bin/env python3
"""make_deck.py <deck.txt> [--out DIR] [--no-video] [--no-tts]
A slide deck from a text file -> 1280x720 PNG slides + manifest.txt -> narrated MP4 (make_presentation_video.py, behind cpu_gate).
Deck file (fields split by '|', bullets by ';'):  TITLE | title | subtitle | narration
                                                   SLIDE | heading | bullet one ; bullet two ; ... | narration
Lines starting with # are comments. Doc: 10-user-docs/PRESENTATION-VIDEO-PIPELINE.md (section "Slide decks")."""
import os, sys, re, subprocess, textwrap
from PIL import Image, ImageDraw, ImageFont
HERE = os.path.dirname(os.path.abspath(__file__)); HOUSE = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
GATE = os.path.join(HOUSE, "&.widgits", "_shared-lib", "ops", "+x", "cpu_gate.+x")
MK = os.path.expanduser("~/Desktop/🧩️Piecemark-IT/中.SP_00.00/🗡️.crswrd.media-archive/make_presentation_video.py")
FB = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"; FR = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"
W, H = 1280, 720
def font(p, n): return ImageFont.truetype(p, n)
def slide(kind, head, sub, bullets, n, total, out):
    im = Image.new("RGB", (W, H), (18, 22, 34)); d = ImageDraw.Draw(im)
    d.rectangle([0, 0, W, 10], fill=(90, 160, 255)); d.text((48, H - 40), "LyfeDesk  -  proposed plan, 2026-11-04  -  %d / %d" % (n, total), font=font(FR, 18), fill=(120, 130, 150))
    if kind == "TITLE":
        for i, l in enumerate(textwrap.wrap(head, 26)): d.text((70, 210 + i * 78), l, font=font(FB, 66), fill=(235, 240, 255))
        for i, l in enumerate(textwrap.wrap(sub, 60)): d.text((74, 450 + i * 40), l, font=font(FR, 30), fill=(150, 190, 255))
    else:
        d.text((60, 44), head, font=font(FB, 46), fill=(235, 240, 255)); d.line([(60, 112), (W - 60, 112)], fill=(60, 80, 120), width=3)
        y = 140
        for b in bullets:
            lines = textwrap.wrap(b, 66)
            d.ellipse([66, y + 14, 80, y + 28], fill=(90, 160, 255))
            for l in lines: d.text((104, y), l, font=font(FR, 31), fill=(220, 226, 240)); y += 42
            y += 16
    im.save(out)
def main():
    a = sys.argv[1:]
    if not a: print(__doc__); return 2
    deck = os.path.abspath(a[0]); name = os.path.splitext(os.path.basename(deck))[0]
    out = a[a.index("--out") + 1] if "--out" in a else os.path.join(os.getcwd(), name)
    snaps = os.path.join(out, "snapshots"); os.makedirs(snaps, exist_ok=True)
    rows = [l.rstrip("\n") for l in open(deck, encoding="utf-8") if l.strip() and not l.startswith("#")]
    man = []
    for i, l in enumerate(rows, 1):
        f = [x.strip() for x in l.split("|")]
        kind = f[0]; fn = "%02d_%s.png" % (i, re.sub(r"\W+", "_", f[1])[:30])
        if kind == "TITLE": slide("TITLE", f[1], f[2], [], i, len(rows), os.path.join(snaps, fn)); nar = f[3]
        else: slide("SLIDE", f[1], "", [b.strip() for b in f[2].split(";") if b.strip()], i, len(rows), os.path.join(snaps, fn)); nar = f[3]
        man.append("%s | 6 | %s" % (fn, nar.replace("|", "/")))
    open(os.path.join(out, "manifest.txt"), "w", encoding="utf-8").write("\n".join(man) + "\n")
    open(os.path.join(out, "REPRODUCE.md"), "w", encoding="utf-8").write("# Reproduce\n\n`python3 &.widgits/_shared-lib/presentation/make_deck.py %s`\n" % os.path.relpath(deck, HOUSE))
    print("slides:", len(rows), "->", out)
    if "--no-video" not in a:
        cmd = [GATE, HOUSE, "run", "deck-video", "--wait", "300", "--", "env", "PRESENT_TTS=" + os.path.join(HERE, "remote_tts.py"), "OMP_NUM_THREADS=1", "PET_TTS_THREADS=1", "MKL_NUM_THREADS=1", "OPENBLAS_NUM_THREADS=1", "TORCH_NUM_THREADS=1", "nice", "-n", "19", "ionice", "-c3", "python3", MK, out, "--fps", "30", "--width", "1280"] + ([] if "--no-tts" not in a else ["--no-tts"])
        r = subprocess.run(cmd, capture_output=True, text=True); print("video rc=%d %s" % (r.returncode, (r.stdout + r.stderr).strip()[-140:]))
    return 0
if __name__ == "__main__": sys.exit(main())
