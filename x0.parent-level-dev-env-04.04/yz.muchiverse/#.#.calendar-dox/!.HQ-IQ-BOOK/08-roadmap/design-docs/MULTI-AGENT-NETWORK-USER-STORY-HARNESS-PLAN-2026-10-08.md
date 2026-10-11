# Multi-agent network experiments and the full user-story harness (plan, 2026-10-08)

Status: **PLAN, nothing built.** Written by Claude from the owner's direction (2026-10-08): "we should be experimenting with multi agents, on networks, visiting each other's desks, doing quests, trading mined cones from the blockchain. Full harness presentations of this soon, even from install. Then we can secure the entire user story."

Marks: ✅ exists and was checked, 🟡 partial, ❌ missing. "Unverified" means I read it but did not run it.

## 1. The user story, end to end

Install → log in → get a desk → meet another agent on a network → visit their desk → take a quest from a stone → do it → be paid in cones (escrow) → trade cones → leave. Each arrow is a step a harness must drive and a person must be able to watch.

## 2. What exists for each step (checked)

| Step | What exists | State |
|---|---|---|
| Install | `tearit-install` `curl | sh` + payload repo; `make-payload.sh` builds the payload from the live house; the older journey graph `07-install-and-ship/USER-JOURNEY-COMPLETION-GRAPH.md` (2026-09-02) walks install → publish-a-toy with per-step status | 🟡 Linux only; the graph's statuses are 5 weeks old, re-check before use |
| Users on one machine | many uuid users under `xyzfs/users`, `current_login.txt` selects the current one | ✅ one *current* user at a time |
| Peers | `palnet_peer.c`: symmetric peer process, discovery by a presence directory, TCP data; **address hardcoded to 127.0.0.1** (lines 138 and 166) | 🟡 same machine only |
| Mining | `chain_miner`, `chain_new`, faucet; several chains incl. test chains and **cones** | 🟡 cones has an open block-0 bug (per the audit) |
| Trading | `chain_send` checks derived balance and propagates via the peer outbox; `chain_escrow` lock/payout/refund | 🟡 **no signing**: `tx_id` is a dedup key, the `agent`/`from` fields are honor fields |
| Quests | `^.grave` board (user data), stone design (headstones doc, 1c: contract + escrow) | 🟡 board real; stones/contracts designed only |
| Visiting a desk | Transfer Player / `mr_transfer_desk` moves state between maps; harness `transfer_map_access` (17 checks) | 🟡 state files, not a visit protocol; **no concept of a guest on someone else's desk** |
| Agents that act | XOD stack (LLM brain + FSM + TOM) drives WSR; Eden entities act by event pages; ghosts not yet | 🟡 |
| Watching it | CSV Lab-style windows; no network, trade or visit viewer | ❌ |

## 3. The core security gap, stated first

**There is no identity proof anywhere between two machines or two users.** Chain transactions are unsigned, escrow trusts a field, and a visitor would act on another user's desk. So every scenario below runs in one of three **trust tiers**, and the harness labels which tier it proves:
- **T0 honor/play money, same machine, scratch houses** (everything here can start now).
- **T1 same machine, real separate OS users or containers, signed messages** (needs the signing gate).
- **T2 two machines (the Mac at 10.0.0.144), signed** (needs configurable peer address + T1).

Real value (the cones chain with real mining) stays out of T0. Securing the user story means moving each scenario up a tier with the same harness; the scenario text does not change.

## 4. The scenarios (each is a pal harness on scratch houses, written before the feature)

Common rules: scratch houses under `/tmp`, **never** the live desk or `xyzfs/users`; deterministic seeds; a mutant per scenario; a timeline file the presentation reads; key files and wallets never leave the scratch tree.

- **S0, install.** Build a payload with `make-payload.sh` into a scratch dir, install it into `/tmp/h1` and `/tmp/h2` with the real `bootstrap.sh`, create user A and user B (`seed-user.sh`). *Pass:* both start, paths contain no source-house absolute path (the relative-path rule), no key or wallet file in the payload. Also catches the leak check from the install doc.
- **S1, discover.** Two houses, two peers on different local ports (needs the peer to take a port argument; today it is fixed). *Pass:* each lists the other in its presence directory; kill one and the other notices.
- **S2, mine and trade.** On a *test* chain, A mines (faucet/miner), sends N cones to B via `chain_send`, B's derived balance rises, `chain_escrow audit` says `conserved=1`. *Mutant:* a send larger than the balance must be refused.
- **S3, visit.** Define a minimal **visit protocol** (new): B writes a `VISIT | guest | grants | expires` row in A's `visits.txt`; B's agent gets a *guest mailbox* on A's desk, not access to A's user data; everything B does there is `GUEST_ACT` rows in A's ledger. *Pass:* a visit without a grant is refused; the grant expires; A's files are untouched except the mailbox and ledger.
- **S4, quest across desks.** A posts a stone with one quest and an escrowed reward; B visits, claims, does it; the check passes; `quest.pal` returns a verdict; payout; stone live/archive/consume per script; A and B balances and the audit agree. *Mutants:* settle twice; pay above the lock; a refund after payout.
- **S5, three agents.** Add C: two takers on the same quest with an equal-split rule; one abandons. Checks fairness and the claim log.
- **S6, the presentation.** One scripted run of S0 to S5 that writes a **timeline** (`TIME | actor | action | evidence path`), takes frame dumps of the windows involved (relay-driven, per the handoff recipe), and generates one HTML report (the same builder as the session report: tabs, collapsible, search). This is the thing shown to a person; it is also the regression suite.
- **S7, two machines.** Same scripts with the peer pointed at the Mac (`10.0.0.144`); needs the configurable address first. Report which scenarios pass on T0 versus T2.

## 5. Agents that play the scenarios

1. **Scripted FSM agents first** (deterministic, no model): S0 to S5 must pass with these, or the harness is testing the model, not the system.
2. **Then model-driven agents** using the XOD shape (LLM brain, FSM controller, TOM layer, GOAP fitness): free providers only, Gemma on the Mac (`gemma3` is on the Mac Ollama), Groq/OpenRouter free models tested today. Give each agent a persona, a goal on a stone, and a budget (MP).
3. Compare scripted versus model runs on the same seed; differences go to the delegation bank's feedback ledger as graded rows.

## 6. What must be built, in order (each step has a harness and a mutant)

1. **Peer takes address and port as arguments** (replace the two hardcoded 127.0.0.1 lines) and a harness for S1. Tier W/M.
2. **S0 install harness**, including the payload leak check and relative-path check. Tier W.
3. **S2 trade harness** on a test chain; fix or document the cones block-0 bug first. Tier M.
4. **Stone contract ops** (settle with escrow, items via held rows) per headstones doc 1c, with S4's mutants. Tier M.
5. **Visit protocol** (S3): small, new, security-critical; design review by the owner before code. Tier C then M.
6. **Signing gate** (T1): a signing scheme for chain tx and visit grants; until it exists, every report says "T0 honor". Owner decision.
7. **Presentation builder** (S6) and a **network/trade/visit viewer window** in the khtpm house shape so a person can watch it. Tier M.
8. **Two-machine run** (S7).

## 7. Risks and rules specific to this work

- A visiting agent is untrusted input. Treat everything it writes as data, never as a command; the guest mailbox is the only writable place.
- Never run these against the live desk; they create users, chains and wallets. Scratch houses only, torn down after, with a count of files created.
- Free-tier budget: 200,000 tokens/day per Groq model; S5/S6 with model agents must meter tokens and stop at the quota (exit code 3 pattern from `csv_lab`).
- Heavy runs wrapped in `nice -n 15 ionice -c3` (weak machine).
- Cross-machine run touches the Mac: only with the owner's go-ahead and only inside a scratch folder there.

## 8. Decisions that belong to the owner

The visit protocol's permissions (what a guest may do); the signing scheme and when real cones may be traded; whether the first public presentation is T0 honor play money or waits for T1; which agents (scripted, model-driven) appear in it; and whether Eden or a quest-board game is the setting.


## 9. First cross-machine result (2026-10-08, later) and what it was NOT

**Done:** `palnet_peer` no longer hardcodes `127.0.0.1`. It reads `PALNET_BIND` (listen address), `PALNET_ADVERTISE` (host peers dial) and `PALNET_SEEDS` (`host:port,...` to dial when the presence directory has nobody; the presence directory is a local folder, so seeds are how a remote peer is found). Default stays loopback because a non-loopback bind accepts any connection with no authentication. Commit `2652fdafd` (in `claude`, not pushed).
**Verified by hand, with real ssh to the LAN hosts (key login works for all three: debil `10.0.0.16`, Mac `10.0.0.144`, jb desktop `10.0.0.187`; each has gcc):** one peer on this machine (`10.0.0.238`), one on debil, one on the Mac, each in a scratch folder under `/tmp` (only `palnet_peer.c` was copied; no user data, no keys), each bound to its own address. A line appended to the local outbox **arrived in both remote inboxes**. The Mac needed a fix (no `MSG_NOSIGNAL` on macOS). Scratch folders and processes were removed afterwards.
**Update (same day, after the owner asked that peers remember each other's address and said the network is hidden, so no authentication worry for now):** `palnet_peer` now puts its listening `host:port` in HELLO, saves every peer it meets in `known_peers.txt` (project root), redials them after a restart with **no seeds**, and drops the duplicate connection. Re-run across this machine, debil and the Mac: phase 1 with seeds, each remote got the line **once** and each node remembered 2 peers; phase 2, all three restarted with no seeds, the line arrived once via remembered addresses; reverse direction (debil to the other two) once each. Scratch folders removed. Still headless and still hand-run; the harness case (S1) and the windows are next. Earlier finding kept for the record: **Found (first run):** every message arrived **twice** on each remote (both sides dial each other, so there are two connections); needs a connection dedup by node id. The cones chain, mining, escrow and the IRC/forum windows were **not** involved yet.
**What this was NOT, and why it is not yet the demo:** I injected the line by appending to a file, headless. That proves the transport, not the user story. The owner's requirement is that the evidence is **visible and relay-driven**: an agent opens the window's menu, reads its inventory and balance, and acts as a person would. Nothing in section 4's scenarios counts until it is shown that way.

## 10. The visible version: IRC and Forum windows as the stage

Network menu rows 1 and 2 are **IRC Chat** and **Forum** (`livedesk:open-network:irc|forum`); the apps are `&.hq-apps/irc-chat-hq` and `forum-hq`, backed by `044.pal-chat-irc` and `041.pal-forum`, which already use `palnet_peer` (the chat button script starts a persistent peer with `own_kind=irc_node`). So the first visible demo needs no new window: it is the existing IRC/Forum windows on this machine, debil and the Mac, each started with its own `PALNET_*` addresses, driven through the relay. Steps, each with a frame dump and the inbox/ledger text:
1. Start each machine's own house build with this peer (the houses on debil and the Mac are older and need updating from `claude`; update through a scratch checkout, not over a live desk).
2. Open IRC on each machine; relay-type a line on one; show it appearing in the others' windows.
3. Open Forum; post; show it replicate.
4. Add the chain windows (cones balance, send) and the quest board once those are on the same peer.
**Open question for the owner:** use the existing `044.pal-chat-irc` stack (older, built on the legacy engine) or the newer `irc-chat-hq` window as the demo surface. I have not checked which of the two is wired to the peer today.

## 11. File transfer, drag and drop, entities between houses (the FTP question)

**Not figured out beyond a spec.** `HQ-FTP-LAN-SYNC-SPEC.md` (2026-10-07) designs a no-server LAN sync (framed TCP protocol `HQFTP/1` with HMAC, pairing with a human in the loop, shares and a deny-list for private data, sync semantics, Mac hazards, a pal loopback harness, a build order). `&.hq-apps/hq-ftp/` is a **placeholder window** (an `.xhtpm` and a launcher; the network menu row says "not built"). Nothing transfers bytes. Drag and drop **exists inside one house**: `02-architecture/DRAG-AND-DROP-BETWEEN-MENUS.md` (read from code, not exercised with a real drag) describes a placed entity as a drag source and a window with `drop_action` as a target, via X11 XDND carrying the entity's directory path. **Across houses nothing exists:** an entity is a folder with an identity (uid, hash, phone), so a transfer must regenerate identity, keep the type (the folder's `meta.pdl` kind) and refuse unsupported types; the spec's deny-list and the install doc's pack rules apply. Suggested order: build `ftp_manifest` + `ftp_apply` on scratch trees first (the spec's step 1), then the window, then drag-and-drop between a file explorer and a remote share.

## 12. The screen recorder (`151.screen-rec+01.02`): how it fits, and what it needs to be drivable

**Read 2026-10-08 (not run).** A small OBS-style recorder: asks the compositor for a screen-capture session through xdg-desktop-portal, receives video over PipeWire, previews it in a GL window, and encodes `.mp4` with libx264. Two binaries (`system/screen_rec`, `system/screen_rec_gui`), a `button.sh` with `deps | compile | run | kill`, file-based receipts and a control file `pieces/control/record_command.txt` (`start` / `stop`), plus a test harness (`tk_click`, `tk_screenshot`) with a scenario script. Built binaries date from Oct 5. **This machine runs Wayland (GNOME)**, which is why it uses the portal: classic X11 grabs are blocked.
**Good for us:** it is already controllable by file (an agent writes `start`/`stop`), its output is a plain mp4, and it matches the house shape (daemon + receipts).
**What blocks it as the evidence tool:**
- The portal shows a **picker dialog the first time each daemon starts**; there is no persisted restore token (its own doc lists this). A person must click it, or a restore token must be stored, or the demo must start the recorder once by hand.
- **Its last recorded test result is a FAIL from 2026-07-27** (`gui_display.receipt.txt` never reported `checksum_match=1`, "preview pipeline is broken"). I do not know whether that is still true; it must be re-run.
- **No audio** (no mic or desktop audio, so no narration), **no RTMP/stream output**, thumbnails never cleaned.
- It captures the compositor's monitor or window, so relay-driven khtpm windows are in the picture only if the picker is pointed at the whole screen; per-window capture is possible but needs the picker choice.
**To make it drivable for presentations (proposed order):** (1) re-run its own scenario and report the real result; (2) store the portal restore token so `start` needs no click; (3) a `mark` command that writes a timestamped chapter row into the presentation timeline (S6) so the video and the evidence list line up; (4) add narration (audio) from the TTS pipeline already used for the NIGHT lessons, mixed in afterwards with ffmpeg; (5) optional RTMP for live sharing. Each step gets a pal harness case; the recorder is "visible" evidence only after it records a relay-driven session whose frames match the dumps.

## 13. How soon, honestly (estimates, not commitments)

| Milestone | What you would see | My estimate |
|---|---|---|
| A. Peer on 3 machines, harness S1 passing (loopback + LAN), duplicate-connection fix | text proof, no video | about a day |
| B. IRC and Forum windows live on this machine + debil + Mac, relay-driven, frame dumps + timeline | **first shareable visual proof** of network chat across machines | about 3 to 5 working days (mostly updating the remote houses and getting the windows wired to the configured peer) |
| C. Screen-rec re-run, restore token, chapter marks → **first video** of milestone B | mp4 of B for stakeholders | +2 to 3 days after B |
| D. Cones mining and send between the three, shown in a chain window, with escrow on a test chain | the trade story | +1 week |
| E. Auction and quest board with escrow (stone contracts) and an agent that opens its own menus and reads its own inventory | the full user story at T0 honor level | +2 to 3 weeks |
| F. Install from the payload on a clean machine as the opening scene | "from install" | after A and the relative-path fixes; +3 to 5 days |
| G. Signing and real value | "secure the user story" | owner decision on scheme first; not estimable yet |
These assume steady work, no new surprises on the older houses, and that the open questions in section 8 and 10 are answered. The video milestone (C) is the first thing worth showing outside; B is the first thing worth showing the owner.


## 14. A Friends pane in the IRC window: users, friends, addresses, profiles (OWNER request, 2026-10-08)

**OWNER:** the chat should have a list of users and friends, with their IPs, profiles and so on; "or is that for forum?"; there should be ways to hook up with friends; a new pane, on the right.

**What exists today (checked in the files, not run):**
- **IRC window (`irc-chat-hq`) has no member or friends list.** Its template has a left sidebar (rooms, a "join/new #" field) and a main panel (status line, messages, a "you" user field, the composer). The only user concept is the current user name.
- **The forum has the social half:** per-user `users/<name>/following.txt` (the "follow" screen), a global feed from the users you follow, and DM rows (`DM|from|to|time|text`). So follows and DMs are forum features; an IRC friends pane can reuse the same files rather than invent new ones.
- **Entities have phones** (`zz.phone`, `^.hai-phone`): a phone number per entity, a contact model for agents.
- **Addresses:** the peer now writes `known_peers.txt` (`host|port|node_id|kind|last_seen`), which is exactly the "IPs and ports of people I have met". It does not yet record a human name or profile.
- **Not present anywhere:** a profile record (display name, status line, avatar glyph, notes) and a place to keep "friends" distinct from "peers I happened to meet".

**PROPOSAL: a right pane (third column) in `irc-chat-hq` with three tabs: Online, Friends, Me.**
- **Online** = who is on the network now: every node in `known_peers.txt` whose heartbeat or connection is live, with the user names seen in the room's recent messages. Columns: name, address (`host:port`), kind, last seen, a dot for connected.
- **Friends** = people you chose to keep: `friends.pdl` in the *user's* data (never in git, per the install doc):
  `FRIEND | name | node_id | host | port | note | added`. Rows are appended; removing writes an `UNFRIEND` row. A friend stays listed offline, with last-seen, and their address is remembered so reconnecting is automatic (the peer already redials remembered addresses).
- **Me** = your own profile, shown to peers: `profile.pdl` (`NAME`, `STATUS`, `GLYPH`, `ABOUT`, optionally a public key later). It is sent to a peer in a `PROFILE|...` line right after HELLO and cached by the receiver as `profiles/<node_id>.pdl`, so a profile you see is the last one that peer sent, not something you typed about them.
- **Actions (each a cli_io or button row, reachable by nav number so a relay can drive it):** *Add friend* (from an Online row), *Remove*, *DM* (opens the DM thread; reuses the forum's DM rows), *Follow* (writes the forum's `following.txt` line), *Copy address*, *Connect by address* (a field that takes `host:port` and adds it to `known_peers.txt`; this is "hooking up with a friend").
- **Presence wording:** "online" means connected or seen within the stale window (the peer's `STALE_SEC`, 15 s); never claim more.
- **Privacy rules:** a profile is whatever the user put in `profile.pdl`, nothing is read from the rest of their desk; wallets, histories and keys are never part of a profile; friends and cached profiles live in user data.
- **Safe default for the demo:** with no profile, send only the node's name and kind.

**Layout and renderer note:** this should be a template change only (`irc-chat-hq.xhtpm` gains a `<panel>` on the right and `vars=` rows from `irc_chat_ui.txt`), plus manager output. Do not add a `layout_*` branch to the shared renderer; if the three-column width needs a window default, set it in the app's config. **Unverified:** whether the sidebar/panel path handles a third column without a CSS change; try it on the swatch/scroll paths first.

**Where friends meet the other plans:** headstone comments (quest takers appear as Online rows), visits (a guest is a friend with a grant), auctions and trades (pick a friend as counterparty), and the Friends tab is the natural place to show a peer's cones address once chain wallets are linked to profiles.

**Harness cases (write first, scratch houses):** a peer meeting writes `known_peers.txt`; `PROFILE` after HELLO is cached and a second `PROFILE` replaces it; add/remove friend appends rows and the list shows them offline; connect-by-address with a bad value is refused; a profile never contains a path or a key. Mutant: let the receiver trust a peer-supplied `node_id` field that does not match the connection, and the case for spoofing must fail.

**Build order:** (1) manager reads `known_peers.txt` and publishes Online rows; (2) the right pane in the template, read-only; (3) friends add/remove; (4) profile send/receive; (5) DM and follow links. Steps 1 and 2 give a visible pane in the first demo.
