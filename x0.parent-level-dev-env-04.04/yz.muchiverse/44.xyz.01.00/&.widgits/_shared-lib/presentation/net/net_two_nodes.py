#!/usr/bin/env python3
"""net_two_nodes.py - Phase 2 network proof: a pet's IRC post on THIS machine reaches a node on ANOTHER machine, and back.
  python3 net_two_nodes.py [--remote debil@10.0.0.16] [--local-ip 10.0.0.238] [--port 9950] [--keep] [--password]
Uses the 044.pal-chat-irc ops verbatim (palnet_peer, chat_post_message, chat_inbox_watcher; self-contained C). Each node gets a scratch project root (NOT a house, no xyzfs/users, no wallets):
local /tmp/claude-1000/phase2/nodeA, remote ~/phase2-net/nodeB (binaries built from the same sources by net_setup_remote.sh). PALNET_BIND/ADVERTISE = the node's LAN address, PALNET_SEEDS = the other
node (the presence folder is local, so seeds are the cross-machine discovery). LAN TEST ONLY: palnet_peer has no authentication yet (PAL-NET-STANDARD sec. 7).
Checks (each can fail; the negative one proves it): A->B line arrives in B's rooms/lobby/messages.txt, B->A line arrives in A's, a token nobody posted is in neither. Always tears down by pid.
Exit 0 = all pass."""
import os, sys, time, subprocess, shutil, uuid, shlex
HERE = os.path.dirname(os.path.abspath(__file__)); HOUSE = os.path.abspath(os.path.join(HERE, "..", "..", "..", ".."))
SRC = os.path.join(HOUSE, "044.pal-chat-irc\U0001F465\ufe0f+2", "ops"); OPS = "/tmp/claude-1000/phase2/bin"      # built fresh from source: the +x binaries inside the app folder can predate PALNET_BIND (the local one was from 2026-10-05)
args = sys.argv[1:]; opt = lambda k, d: args[args.index(k) + 1] if k in args else d
REMOTE, LIP, PORT = opt("--remote", "debil@10.0.0.16"), opt("--local-ip", "10.0.0.238"), opt("--port", "9950"); RIP = REMOTE.split("@")[1]; KEEP = "--keep" in args
A = "/tmp/claude-1000/phase2/nodeA"; B = "$HOME/phase2-net/nodeB"; RB = "$HOME/phase2-net/bin"
PW = "--password" in args      # password login (export SSHPASS=... first; nothing is stored anywhere)
def ssh(cmd, t=60): return subprocess.run((["sshpass", "-e", "ssh", "-n", "-o", "PubkeyAuthentication=no"] if PW else ["ssh", "-n", "-o", "BatchMode=yes"]) + ["-o", "ConnectTimeout=8", REMOTE, "sh -c " + shlex.quote(cmd)], capture_output=True, text=True, timeout=t)
def rdf(p):
    try: return open(p, errors="replace").read()
    except OSError: return "(missing)"
def envs(root, bind, seed): return {"PRISC_PROJECT_ROOT": root, "PRISC_PROJECT_ID": "pal-chat-irc", "PRISC_NET_ROOT": root + "/presence", "PALNET_BIND": bind, "PALNET_ADVERTISE": bind, "PALNET_SEEDS": seed}
procs = []; fails = 0
def check(name, ok, why=""):
    global fails
    print(("OK   " if ok else "FAIL ") + name + ("" if ok else "  " + why)); fails += 0 if ok else 1
def wait_for(fn, secs):
    t0 = time.time()
    while time.time() - t0 < secs:
        if fn(): return True
        time.sleep(1)
    return False
GATE = os.path.join(HOUSE, "&.widgits", "_shared-lib", "ops", "+x", "cpu_gate.+x")
try:
    os.makedirs(OPS, exist_ok=True)
    for n in ("palnet_peer", "chat_post_message", "chat_inbox_watcher"):      # tiny compiles, still behind the cpu gate (house rule)
        subprocess.run([GATE, HOUSE, "run", "net-build-" + n, "--", "gcc", "-O2", "-w", "-o", OPS + "/" + n + ".+x", SRC + "/" + n + ".c"], capture_output=True)
    shutil.rmtree(A, ignore_errors=True)
    for d in ("net", "rooms", "presence"): os.makedirs(os.path.join(A, d))
    r = ssh("rm -rf %s && mkdir -p %s/net %s/rooms %s/presence && ls %s/palnet_peer.+x" % (B, B, B, B, RB)); check("remote node dir + binaries", r.returncode == 0, r.stderr.strip()[:80])
    if fails: raise SystemExit(1)
    eb = " ".join("%s=%s" % (k, v) for k, v in envs(B, RIP, "").items())
    rs = ssh("cd %s && for c in \"palnet_peer.+x irc_node pal-chat-irc - %s/net/outbox.txt %s/net/inbox.txt irc_node\" \"chat_inbox_watcher.+x\"; do set -- $c; n=$1; shift; (env %s nohup %s/$n \"$@\" > %s/$n.log 2>&1 < /dev/null &); done; sleep 1; pgrep -f 'phase2-net/bin' | wc -l" % (B, B, B, eb, RB, B)); check("remote peer + watcher started", rs.returncode == 0, rs.stderr.strip()[:80])
    time.sleep(2)
    rport = ssh("cat %s/presence/*.txt 2>/dev/null" % B).stdout; rport = next((l.split("=", 1)[1].strip() for l in rport.splitlines() if l.startswith("port=")), PORT)
    print("remote node listens on", RIP + ":" + rport)
    ea = dict(os.environ, **envs(A, LIP, "%s:%s" % (RIP, rport)))
    for cmd in (["palnet_peer.+x", "irc_node", "pal-chat-irc", "-", A + "/net/outbox.txt", A + "/net/inbox.txt", "irc_node"], ["chat_inbox_watcher.+x"]):
        procs.append(subprocess.Popen([os.path.join(OPS, cmd[0])] + cmd[1:], cwd=A, env=ea, stdout=open(A + "/" + cmd[0] + ".log", "w"), stderr=subprocess.STDOUT))
    def connected(): return os.path.exists(A + "/peers_now.txt") and open(A + "/peers_now.txt").read().strip() != ""
    check("nodes connected (A sees B in peers_now.txt)", wait_for(connected, 25), "peers_now.txt empty; A log: " + open(A + "/palnet_peer.+x.log", errors="replace").read()[-200:])
    time.sleep(4)      # let the two sockets settle (a duplicate link is dropped after the HELLOs)
    tok1, tok2, tok3 = [uuid.uuid4().hex[:8] for _ in range(3)]
    post = lambda root, env_, user, text: subprocess.run([os.path.join(OPS, "chat_post_message.+x"), "lobby", user, text], cwd=root, env=env_, capture_output=True, text=True)
    post(A, ea, "pet-ember", "hello from machine A " + tok1)
    rb = lambda: ssh("cat %s/rooms/lobby/messages.txt 2>/dev/null" % B).stdout
    check("A -> B: Ember's line reached the other machine", wait_for(lambda: tok1 in rb(), 20), "B ledger: " + rb()[-120:])
    ssh("cd %s && env %s PRISC_PROJECT_ID=pal-chat-irc %s/chat_post_message.+x lobby pet-fenn 'hello from machine B %s'" % (B, eb, RB, tok2))
    ra = lambda: open(A + "/rooms/lobby/messages.txt", errors="replace").read() if os.path.exists(A + "/rooms/lobby/messages.txt") else ""
    check("B -> A: Fenn's line reached this machine", wait_for(lambda: tok2 in ra(), 20), "A ledger: " + ra()[-120:])
    check("negative: a token nobody posted is in neither ledger", tok3 not in ra() and tok3 not in rb())
    if fails:
        print("--- diag A: peers_now:", rdf(A + "/peers_now.txt").strip()[:200], "| outbox:", rdf(A + "/net/outbox.txt")[-200:].strip(), "| inbox:", rdf(A + "/net/inbox.txt")[-200:].strip())
        print("--- diag B:", ssh("cd %s; echo peers:; cat peers_now.txt; echo outbox:; tail -c 300 net/outbox.txt; echo inbox:; tail -c 300 net/inbox.txt; echo rooms:; ls rooms rooms/lobby 2>&1; echo logs:; tail -3 *.log" % B).stdout)
    print("speaker check A:", [l for l in ra().splitlines() if tok1 in l or tok2 in l][:2])
finally:
    for p in procs: p.terminate()
    ssh("pkill -f 'phase2-net/bin/' ; sleep 1; pgrep -f 'phase2-net/bin' | wc -l", 30) if not KEEP else None
    for p in procs:
        try: p.wait(5)
        except Exception: p.kill()
sys.exit(1 if fails else 0)
