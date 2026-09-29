# Cross-Machine Networking Plan (Multi-User, Second Machine)

**Date:** 2026-09-27
**Status:** Plan only — not started. Scheduled for after the current AI-push work.
**Also filed at:** `/home/no/Desktop/github/work/XO/4.Cross_Machine_Networking/PLAN.md`

## Goal

Get a second physical/VM machine to: (1) have the house installed, (2) log
in as a real user of it, and (3) actually talk to the first machine's
instance over a real network — not the same-machine multi-process
simulation that exists today.

## What Already Exists (verified, not assumed)

- `044.pal-chat-irc👥️+2/ops/palnet_peer.c` — a real P2P layer: each
  session binds a TCP listener, discovers peers via `net/presence/`,
  and delivers messages to `net/inbox.txt`, watched by
  `chat_inbox_watcher` which triggers a redraw.
- `044.pal-chat-irc👥️+2/testing/test_multiuser_p2p.sh` — a real regression
  test for this chain, written after PITFALL 20/21 (2026-07-26): the P2P
  layer was completely broken (bad arg-collapsing in a launch call) for a
  while with nobody noticing, because `rooms/*/messages.txt` and
  `data/master_ledger.txt` are shared files on the SAME machine — any
  session redrawing for its own reasons picks up shared data with zero
  help from the network layer, giving false confidence.
- `#.#.calendar-dox/!.HQ-IQ-BOOK/08-roadmap/design-docs/IRC-FORUM-CHAIN-HQ-WINDOWS.md`
  — design doc for porting IRC/Forum/Chain CLI apps into X11-HQ windows
  (Phase 1 done per session memory, Phase 2 plan written).

## The Real Gap (verified by reading the code, not guessed)

`palnet_peer.c` hardcodes `127.0.0.1`:

- line 138: `addr.sin_addr.s_addr = inet_addr("127.0.0.1");` (bind)
- line 166: `fprintf(f, "host=127.0.0.1\n");` (presence file written for
  other local sessions to discover)

This means the existing "multi-user" test is real multi-*process*, same
machine — it has never actually gone over a network interface. Cross-
machine communication needs real code changes here, not just a
network-reachability test:

- Bind to a configurable interface (`0.0.0.0` or the machine's real LAN
  IP), not a hardcoded loopback address.
- Presence files need to carry the real, reachable IP of the writing
  machine, not a hardcoded `127.0.0.1` — needs a real "what's my LAN IP"
  lookup, with a manual override for machines with multiple interfaces/
  VPNs/NAT.
- Firewall/port considerations on both machines (open the port
  `palnet_peer` binds, or documented port-forwarding if behind NAT).
- Some house-wide account/identity model for "which user is this" across
  two machines — needs scoping (see Open Questions).

## Planned Work (in order)

1. **Install pass on the second machine**
   - Document the actual dependency list (X11 dev headers, Xft, gcc,
     fontconfig, etc.) — currently tribal knowledge from this machine's
     own setup, not written down anywhere as a checklist.
   - Clone/sync the repo to the second machine (git remote, or a tar
     transfer — TBD based on whether the second machine has git access
     to wherever this repo is hosted).
   - Full house build on the second machine (`build_core_render.sh` and
     equivalents for other apps) — first real cross-machine-portability
     test, likely to surface hardcoded-path assumptions.

2. **Login on the second machine**
   - Needs scoping: is "login" an OS-level user session, or the house's
     own `0.user-pal👤️/00.login-signup` flow? Check that module's
     current assumptions about running on a single machine/single xyzfs
     mount before assuming it "just works" on a second machine.

3. **Fix palnet_peer.c for real cross-machine binding**
   - Replace the hardcoded `127.0.0.1` in both the bind call and the
     presence file writer with a real LAN-IP lookup + config override.
   - Test bind/listen/connect between the two real machines (raw socket
     test first, before layering the house's chat UI on top — isolate
     "does the network path work at all" from "does the house's chat
     logic work").

4. **Multi-user communication test, real second machine**
   - Rerun (or extend) `test_multiuser_p2p.sh`'s scenario, but with the
     two peers on two real machines instead of two local processes —
     this is the test that actually closes the PITFALL 20/21 gap: local
     shared-file false-confidence cannot happen across two real machines
     with no shared filesystem, so a pass here is real signal in a way
     the current same-machine test structurally cannot be.
   - Watch specifically for latency/ordering issues that never show up
     on localhost (same-machine tests hide real network jitter).

5. **Write up findings**
   - Update this doc and `IRC-FORUM-CHAIN-HQ-WINDOWS.md` with what
     actually broke crossing the localhost boundary — expect surprises;
     don't assume the P2P layer's design is otherwise correct just
     because the local test passes.

## Open Questions (do not guess — ask before starting)

- Is the second machine on the same LAN, or does this need to work over
  the open internet (NAT traversal, relay server, etc. — a much bigger
  scope)?
- What does "log in on the other machine" mean concretely — a house user
  account, an OS account, or both?
- Is xyzfs (the house's own user data layout) meant to be per-machine, or
  eventually synced/shared across machines? This has large implications
  for the login flow and for what "multi-user" even means here (same
  house instance shared, vs. two independent house instances that talk
  to each other).

## Related

- `PRISC-OPS-ARCHITECTURE.md` — if palnet_peer.c's fix work involves
  process orchestration changes, follow the op+pal+IPC standard, not
  shell.
- session memory: `network-hq-windows-effort` — Phase 1 (IRC/Forum/Chain
  → X11-HQ windows) done, Phase 2 plan written, "heavy naive-agent
  landmines" flagged — read that context before starting Phase 2 work
  alongside this networking plan, they likely intersect.
