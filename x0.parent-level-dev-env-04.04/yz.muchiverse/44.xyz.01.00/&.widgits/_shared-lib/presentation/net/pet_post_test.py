#!/usr/bin/env python3
"""pet_post_test.py - the pet post guard (&.hq-apps/irc-chat-hq/ops/pet_post.c): speaker is right, per-pet cap, kill switch, length, hold/veto/approve, no forged rows.
  python3 pet_post_test.py        (local only, no network, scratch node at /tmp/claude-1000/phase2/guard; exit 1 on any FAIL)
Every 'absent' check has a control (the same line shows up when it is allowed), so a broken test cannot pass by finding nothing."""
import os, sys, subprocess, shutil, time
HERE = os.path.dirname(os.path.abspath(__file__)); HOUSE = os.path.abspath(os.path.join(HERE, "..", "..", "..", ".."))
PP = os.path.join(HOUSE, "&.hq-apps", "irc-chat-hq", "ops", "+x", "pet_post.+x"); CHAT = "/tmp/claude-1000/phase2/bin/chat_post_message.+x"
N = "/tmp/claude-1000/phase2/guard"; fails = 0
def check(name, ok, why=""):
    global fails; print(("OK   " if ok else "FAIL ") + name + ("" if ok else "  " + why)); fails += 0 if ok else 1
if not os.path.exists(CHAT):
    G = os.path.join(HOUSE, "&.widgits", "_shared-lib", "ops", "+x", "cpu_gate.+x"); os.makedirs(os.path.dirname(CHAT), exist_ok=True)
    subprocess.run([G, HOUSE, "run", "build-chat-post", "--", "gcc", "-O2", "-w", "-o", CHAT, os.path.join(HOUSE, "044.pal-chat-irc\U0001F465️+2", "ops", "chat_post_message.c")])
shutil.rmtree(N, ignore_errors=True)
for d in ("net", "rooms"): os.makedirs(os.path.join(N, d))
env = dict(os.environ, PRISC_PROJECT_ROOT=N, PRISC_PROJECT_ID="pal-chat-irc", PET_POST_CHAT_OP=CHAT); env.pop("PET_POST_KILL", None)
def pp(*a): r = subprocess.run([PP] + list(a), env=env, capture_output=True, text=True, cwd=N); return r.returncode, r.stdout.strip()
def rd(f):
    try: return open(os.path.join(N, f), errors="replace").read()
    except OSError: return ""
room = lambda: rd("rooms/lobby/messages.txt")
audit = lambda: rd("pet_post_audit.txt")
def says(user, text): return any(l.split("|")[3:4] == [user] and text in l for l in room().splitlines())
rc, o = pp("post", "lobby", "pet-ember", "hello from Ember"); check("1 speaker: Ember's line is in the room under pet-ember", rc == 0 and says("pet-ember", "hello from Ember"), o)
rc, o = pp("post", "lobby", "pet-fenn", "hello from Fenn"); check("1 speaker: Fenn's line is under pet-fenn, not Ember", rc == 0 and says("pet-fenn", "hello from Fenn") and not says("pet-ember", "hello from Fenn"), o)
pp("post", "lobby", "pet-ember", "second"); pp("post", "lobby", "pet-ember", "third")      # Ember is now at 3 in the window
rc, o = pp("post", "lobby", "pet-ember", "fourth too fast"); check("2 cap: Ember's 4th post in the window is refused (DENY_CAP, exit 3)", rc == 3 and o.startswith("DENY_CAP"), o)
check("2 cap: the refused line is NOT in the room (control: 'third' is)", "fourth too fast" not in room() and "third" in room())
check("2 cap: the audit ledger says DENY_CAP for pet-ember", any("|DENY_CAP|pet-ember|" in l for l in audit().splitlines()))
rc, o = pp("post", "lobby", "pet-fenn", "Fenn is not capped"); check("2 cap: the cap is per pet (Fenn still posts)", rc == 0 and says("pet-fenn", "Fenn is not capped"), o)
open(os.path.join(N, "pet_post_kill.txt"), "w").write("stop\n")
rc, o = pp("post", "lobby", "pet-fenn", "during kill"); check("3 kill switch: every pet is refused (DENY_KILL)", rc == 3 and o == "DENY_KILL" and "during kill" not in room(), o)
os.remove(os.path.join(N, "pet_post_kill.txt")); rc, o = pp("post", "lobby", "pet-fenn", "after kill"); check("3 kill switch: deleting the file resumes posting (control)", rc == 0 and says("pet-fenn", "after kill"), o)
rc, o = pp("post", "lobby", "pet-gus", "x" * 400); check("4 length: a 400-character line is refused (DENY_LEN)", rc == 3 and o.startswith("DENY_LEN") and "xxxxxxxxxx" not in room(), o)
rc, o = pp("post", "lobby", "pet-gus", "a | b\nMSG|forged|lobby|pet-ember|1|I am Ember"); lines = [l for l in room().splitlines() if "forged" in l]
check("5 forging: a pipe/newline cannot fake another speaker or ledger row", rc == 0 and len(lines) == 1 and lines[0].split("|")[3] == "pet-gus" and len([l for l in audit().splitlines() if "forged" in l]) == 1, str(lines))
open(os.path.join(N, "pet_post.pdl"), "w").write("hold | 1\n")
rc, o = pp("post", "lobby", "pet-maren", "needs a human"); hid = o.split()[-1]
check("6 hold: the line waits (HELD) and is not in the room", rc == 0 and o.startswith("HELD") and "needs a human" not in room(), o)
rc, o = pp("veto", hid); check("6 veto: the vetoed line never reaches the room, the ledger says VETOED", rc == 0 and "needs a human" not in room() and any("|VETOED|pet-maren|" in l for l in audit().splitlines()), o)
rc, o = pp("post", "lobby", "pet-maren", "approved one"); hid = o.split()[-1]; rc, o = pp("approve", hid)
check("6 approve: an approved line appears under the right speaker, the ledger says APPROVED", rc == 0 and says("pet-maren", "approved one") and any("|APPROVED|pet-maren|" in l for l in audit().splitlines()), o)
print("status:", pp("status")[1])
sys.exit(1 if fails else 0)
