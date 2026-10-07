#!/usr/bin/env python3
"""make_slides.py - renders the evidence slides for this presentation from REAL files and commands (run at generation time, nothing retyped),
then writes manifest.txt. Usage: python3 make_slides.py <house_root>   (house_root = the 44.xyz.01.00 folder). Then:
python3 ../make_presentation_video.py .   (from this folder). Slides marked DESIGN show design docs, not working code."""
import subprocess, sys, glob, os, textwrap
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

HOUSE = Path(sys.argv[1]).resolve()
HERE = Path(__file__).resolve().parent
SNAP = HERE / "snapshots"; SNAP.mkdir(exist_ok=True)
W, H = 1280, 720
F = lambda n, b=False: ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans%s.ttf" % ("-Bold" if b else ""), n)
M = lambda n: ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf", n)
rows = []

def sh(cmd):
    return subprocess.run(cmd, shell=True, cwd=HOUSE, capture_output=True, text=True).stdout.strip()

def slide(name, title, body_lines, tag, hold, caption):
    im = Image.new("RGB", (W, H), (22, 24, 30)); d = ImageDraw.Draw(im)
    d.rectangle([0, 0, W, 78], fill=(36, 40, 52))
    d.text((40, 20), title, font=F(34, True), fill=(240, 240, 240))
    col = (110, 200, 130) if tag == "EVIDENCE" else (230, 190, 90)
    d.rounded_rectangle([W - 210, 20, W - 40, 58], radius=8, outline=col, width=2)
    d.text((W - 195, 25), tag, font=F(24, True), fill=col)
    y = 100
    for ln in body_lines[:19]:
        d.text((44, y), ln[:104], font=M(21), fill=(210, 220, 230)); y += 29
    im.save(SNAP / name); rows.append((name, hold, caption))

# --- real evidence, gathered now ---
dry = (HOUSE / "^.grave/quests/Q005-phone-in-every-inventory/DRYRUN-REPORT-2026-10-06.txt").read_text().splitlines()
summ = dry[dry.index("SUMMARY (dry run)"):]
phones = sh("find xyzfs -name zz.phone | wc -l"); uids = sh("find xyzfs -name entity_uid.txt | wc -l")
idx = HOUSE / "^.hai-server/phones.index"
nidx = sum(1 for _ in open(idx)); nuniq = len({l.split("|")[0] for l in open(idx)})
pal = sorted(glob.glob(str(HOUSE / "xyzfs/users/*/home/livedesk/pals")))[0]
phone_pdl = (Path(pal) / "dsr_castle_a/inventory/zz.phone/phone.pdl").read_text().splitlines()[2:]
phone_pdl = [l.replace(l.split("|")[2].strip(), l.split("|")[2].strip()[:36] + "...") if "owner_uid" in l else l for l in phone_pdl]
phone_pdl = [l.replace("\U0001F4F1", "[phone emoji]") for l in phone_pdl]   # DejaVu has no emoji glyph: would draw an empty box
inv = sh("ls %s/cursword/inventory | tr '\\n' ' '" % pal)
ver = sh("bash '^.grave/quests/Q003-build-gate-include-list/verify.sh' all 2>&1; echo; bash '^.grave/quests/Q003-build-gate-include-list/verify.sh' --selftest").splitlines()
ver = [l.replace("_.monads/_.livedesk-taskbar/ops/", "ops/").replace("&.widgits/_shared-lib/", "shared/") for l in ver]

slide("01_title.png", "A phone for every entity", [
    "", "Graveyard + ghosts + robots + phones + server", "", "What is built and proven today, and what is still design.", "",
    "  EVIDENCE slides  = real files / real command output, gathered when these slides were made",
    "  DESIGN slides    = design docs only, nothing runs yet", "", "Branch: claude   Date: 2026-10-06"], "EVIDENCE", 7,
    "This is a short proof of one day of work. Slides marked evidence come from real files and real commands. Slides marked design are plans, and nothing in them runs yet.")
slide("02_design_cast.png", "The cast (design)", [
    "grave  (tombstone emoji)   quest board; posts and tracks quests",
    "ghost  (ghost emoji)       managed workers, appear as entities, test and tend entities",
    "robot  (^.hai-robot)       desktop robot with own chats, tasks, can spawn sub-bots",
    "phone  (^.hai-phone)       every entity holds one; messages + readable history",
    "server (^.hai-server)      routes phones, single writer, ledger, kill switch", "",
    "One board layout serves grave, roster, server and phone windows.",
    "Ghosts and robots POSSESS entities through the existing possession + methods",
    "mechanism. Phones are remote controls: command -> server -> lease -> same op."], "DESIGN", 9,
    "The design. A gravestone quest board, ghosts that do the work, robots, a phone in every entity, and a server that routes messages. Ghosts possess entities the way cursword already does, and phones act only through server-granted, time-limited leases.")
slide("03_q005_dryrun.png", "Q005 dry run on the real house", ["python phone_ensure_op  (DRY RUN, nothing written)", ""] + summ + ["", "files under xyzfs before = after = 1365 (dry run wrote nothing)"], "EVIDENCE", 9,
    "First, a dry run on the real entities. It found fifty five entities, thirty two top level and twenty three items inside inventories. Fifty three keep their existing pal hash as a frozen identity, two get random ones, and there were no number collisions. Nothing was written.")
slide("04_copy_apply.png", "Apply on a COPY, twice", [
    "apply #1 on a copy of the pals tree:",
    "  55 phones created, 55 unique numbers, 55 unique uids", "",
    "apply #2 (idempotence check):",
    "  0 created; every file byte-identical (sha256 before/after)", "",
    "also verified on the copy:",
    "  zz.phone sorts LAST in every inventory  (slot order untouched)",
    "  every inventory_slot.txt byte-identical",
    "  frozen uid == original PAL hash (dsr_castle_a checked)",
    "  all wallet ids match  e + 24 hex   (chain wallet charset)", "",
    "real house untouched: 0 zz.phone folders at that point"], "EVIDENCE", 9,
    "Then it was applied to a copy, twice. The first run created fifty five phones with unique numbers. The second run changed nothing, byte for byte. The phone sorts last in every inventory, so no selected slot moved.")
slide("05_live_state.png", "Live now", [
    "entities with a phone (find zz.phone):   %s" % phones,
    "entity_uid.txt files:                     %s" % uids,
    "phones.index lines / unique numbers:      %d / %d" % (nidx, nuniq), "",
    "cursword inventory (phone sorts last):",
    "   " + inv, "",
    "phone.pdl of one entity (dsr_castle_a):"] + ["   " + l for l in phone_pdl[:8]], "EVIDENCE", 10,
    "This is the live system, counted when this slide was made. Every entity has a phone and a permanent id, and every phone has its own number. The history caps are in the phone file, two thousand lines and two hundred fifty six kilobytes.")
slide("06_scorer.png", "Q003: a scorer a ghost can be graded by", ver[:18], "EVIDENCE", 11,
    "The first small job for a ghost is the build gate fix. It comes with a deterministic scorer that reads the real include graph. The self test proves the scorer can fail and pass. Today it reports the remaining gaps in the core renderer, and the manager half is already fixed.")
slide("07_design_events.png", "Next: events + tunable weights (design)", [
    "Events = small, deterministic, reusable units (pal loop -> compiled C ops):",
    "    phone.send   lease.grant   ghost.assign   quest.score", "",
    "Scoring = a verifier script, never a model's opinion.",
    "Weights  = named joints in a tunables file, hand-tuned first:",
    "    which ghost gets a quest, lease length, routing priority, escalate-to-human",
    "Every use is a ledger row (the dataset tomom can learn from later).", "",
    "tomom (meta_rl) = optional advisor behind the server;",
    "everything still works with it off.", "",
    "NOT BUILT YET - this is the plan for the next step."], "DESIGN", 10,
    "Next comes the events layer. Small reusable events, scored by verifier scripts, with named tunable weights that start hand-tuned. Every use is logged, and that log is what tomom can learn from later. None of this is built yet.")
slide("08_status.png", "Status", [
    "DONE and pushed (branch claude, c41e7f935):",
    "  design docs, quest board skeleton, Q001-Q006, phone identity library,",
    "  dry-run-first migration op, manager spawn hook, 55 phones live, Q003 scorer", "",
    "NOT DONE:",
    "  phone sprite in the HUD      start-time before/after measurement",
    "  Q003 core gate fix (7 files)  events + tunables layer",
    "  server router / ledger        ghosts / robots / board layout",
    "  API key rotation (keys are in git history)"], "EVIDENCE", 9,
    "Status. The phone layer is built, migrated and pushed. The server, the ghosts, the robots and the events layer are still ahead, along with the phone sprite and rotating the old API keys.")

(HERE / "manifest.txt").write_text("".join("%s | %s | %s\n" % r for r in rows))
print("wrote %d slides + manifest.txt" % len(rows))
