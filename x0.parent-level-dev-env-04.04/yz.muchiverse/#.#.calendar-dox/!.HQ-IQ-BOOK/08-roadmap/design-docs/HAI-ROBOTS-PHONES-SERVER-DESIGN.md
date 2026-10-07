# Robots, phones and the server — how the hai entities talk (design, 2026-10-06, part 2 of GRAVEYARD-GHOSTS)

**Status:** design + skeleton folders only; nothing runs. Part 1 is `GRAVEYARD-GHOSTS-DESIGN.md` (read it first: the 🪦 quest board
`^.grave` and the 👻 ghosts `^.ghost`). **Manager of the work:** Claude (appointed by the owner). Working names, may change.

## 0. Owner's words (2026-10-06, kept so the intent survives)

> "in the future `^.hai` robots should be added, which adds a robot to the desktop, that has its own chats, and can do tasks, and
> even spawn sub bots, similar to quests. ... when they have tasks for humans, they should create 'computer emoji' and 'phone
> emoji' phones so the entities can communicate with each other, and we can see their history. and the 'computer / server' that
> manages all the entities through their phones (entities read the phones). `^.hai-server` `^.hai-phone`."

## 1. The cast (new pieces in bold)

| Glyph | Folder | What it is |
|---|---|---|
| 🪦 | `^.grave` | quest board (part 1) |
| 👻 | `^.ghost` | babysitter-managed workers sent out by a grave (part 1) |
| **🤖** | `^.hai-robot` | a long-lived desktop robot: own persona, own chats, own tasks; can spawn sub-bots |
| **📱** | `^.hai-phone` | a message endpoint owned by one entity (or a human); its history is visible |
| **🖥️** | `^.hai-server` | the entity that manages all entities through their phones |

How they differ: a **ghost** is dispatched for a quest and answers to a grave. A **robot** lives on the desktop on its own (its chats,
its tasks), may post quests on a grave, and may spawn sub-bots. Both are pals (entities); a robot generalizes `robot_chat_001`
(the first real robot pal, `ROBOT-CHAT-BLUEPRINT.md` §2.3).

## 2. Why no new subsystem (reuse map — read before building)

| Need | Already in the house | Where |
|---|---|---|
| A thing with a glyph, a menu, a desk position that survives reset | pal folder + `DESK` row | `xyzfs/users/<uuid>/home/livedesk/pals/<name>/`; placed by `tp_place_desktop.+x` |
| Per-entity conversation / history files | every pal already has `chat_history.txt`, `history.txt`, `interact_relay.txt`, `last_signal.txt` | checked in real pals (`robot_chat_001`, `dsr_castle_a`) |
| "A thing changed, repaint / re-read" | append-only marker file that GROWS (`st_size` increase), never mtime, one writer | `02-architecture/CENTROID_GOLD_STD.md` rule 8 |
| An op that runs an action for a robot | registered event COMMAND, zero recompile | `#.ref/menu/event_commands.registry.pdl` (`ROBOT-CHAT-BLUEPRINT.md` §2.2) |
| A brain | HORN (workers) or a local model (students) | `^.hai-horn/`, `&.widgits/open-hai/` |
| A window with nav-numbered rows, restylable | layout files + overlay + nav index | `18.pc-hq/IN-GAME-LAYOUTS-PLAN.md` |
| Agents leaving each other human-readable notes (the precedent) | `13.agent-coms/` (KILO, GROK, CODEX folders) | HQ-IQ-BOOK |

## 3. Phones (`^.hai-phone`, 📱)

A phone is a pal. Its folder is its whole state, plain files, append-only:

```
<phone>/phone.pdl     owner (entity id or "human:<name>"), glyph, state (idle|ringing), created, created_by
<phone>/inbox.txt     messages TO the owner      — written ONLY by the server (single writer)
<phone>/outbox.txt    messages FROM the owner    — written ONLY by the owner (or its brain)
<phone>/history.txt   the readable merged conversation, written by the server (what the viewer window shows)
```

**Message line** (one line per message, appended with a single write under `PIPE_BUF` so appends do not interleave):

```
<epoch_ms>|<from>|<to>|<kind>|<ref>|<text>
```
`kind` ∈ `say` · `task` · `ask-human` · `answer` · `status` · `spawn` · `done` · `fail` · `command` · `lease` · `release` · `result` (the last four: §3e). `ref` is a quest id (`Q001`), a thread id, or `-`.
Newlines in `text` are escaped as `\n`. Example: `1791334000123|robot_7|human:owner|ask-human|Q012|may I restart the taskbar? (approve/deny)`.

**"Entities read the phones":** an entity (or the thing driving it) polls its own phone's `inbox.txt` by **size growth**, reads only the
new bytes from its saved offset, and acts. No timers, no mtime, no hash — the house rule. The phone's glyph changes to a ringing
state while the inbox has unread lines (the owner sees which entity is waiting on what).

**Human tasks:** when a robot or ghost has a task for a human (needs approval, a decision, a secret, a physical action) it posts an
`ask-human` message; the server places a 📱 for that human if there is none (placed like any pal) and rings it. Opening the phone
shows the whole history; the reply is an `answer` line in the human phone's outbox, which the server routes back.

### 3b. DECIDED (owner, 2026-10-06): a phone in EVERY entity's inventory, from now on and retroactively

> "we will put a phone in every entity's inventory from now on and retroactively"

This is built on the inventory model in `&.widgits/_shared-lib/khtpm_inventory.c`: **an item IS an entity** — a whole entity package
directory at `<entity>/inventory/<name>/`; Take and Place are plain directory renames. So the phone is a real phone pal at
`<entity>/inventory/zz.phone/` (pal.pdl with glyph 📱, `phone.pdl`, `inbox.txt`, `outbox.txt`, `history.txt`). Consequences:

- **It shows up as an item** (with its 📱 glyph) in the hotbar / inventory HUD with no extra drawing code. **Place** drops the phone onto
  the desk or into pc-hq as a visible 📱 entity; **Take** by another entity moves it with its history intact (nothing is deleted).
- **Slot order is alphabetical by directory name** and `inventory_slot.txt` stores a slot number per entity, so a new item would shift
  the selected slot of entities that already have items. The phone is therefore named **`zz.phone`** so it always sorts last and
  never disturbs existing slot numbers.
- **One hook covers "new" and "old":** every entity process is started through `livedesk_spawn_desk()` (`khtpm_taskbar_manager.c`);
  it (and `livedesk_ensure_cursword()`) calls a shared `phone_ensure(entity_dir)`: if `<entity>/inventory/zz.phone/` is missing, create
  it from `^.hai-phone/_TEMPLATE`, then recurse into `<entity>/inventory/*` so items that are themselves entities get phones too.
  Idempotent, one `access()` per entity when nothing is missing (startup latency matters: the 2026-10-06 slow-start bug). Retroactive =
  every existing entity gets its phone the next time it is spawned; a one-shot CLI (`phone_ensure_op`, dry-run by default) does the whole house at once.
- **Other creators** (placement ops `tp_place_desktop*`, `file_explorer_manager.c`, the events-hq `event_*_to_pal` scripts, robot-chat) need
  no change as long as the spawn hook exists; they may call `phone_ensure` directly if they want the phone before first spawn.
- **Git:** pal data is tracked (about 1,500 files, 54 entities including nested ones at the time of writing). The phone's *skeleton*
  (`pal.pdl`, `phone.pdl`) may be tracked; the **ledgers `inbox.txt` / `outbox.txt` / `history.txt` must be gitignored** (runtime history,
  grows forever) via `**/inventory/zz.phone/{inbox,outbox,history}.txt`. The ledger of record is the server's (`^.hai-server/.../ledger.txt`, also ignored).
- **Server discovery:** the server does not rescan the house for phones; it keeps `phones.index` (append-only, one line per phone path)
  written by `phone_ensure` when it creates one, plus a rebuild-by-scan command for repair.

### 3c. Phone numbers, entity ids and the chain (owner question, 2026-10-06)

> "can they have phone numbers related to entity id? are the entities hashed and prepared to write to the network:chain tx file for
> mining yet? we should pay some attention to that if we're not"

**Status today (corrected after reading the code, same day):** partly. Entities have a human label (`instance_id.txt`: `DCA0`, `ROBOT1`, `CURS`),
a desk index (`livedesk_index.txt`), AND a SHA-256 `PAL | hash | ...` line in `pal.pdl` (53 of 54 entities; `tax_robot` has no `pal.pdl` at all). The house's own
editor design calls it "Hash = identity (NFT-ready)" (`#.livedesk/livedesk-editor-design.md`). But the hash is not a stable identity today:
`livedesk_ensure_pal()` writes it as a **content hash of the whole package folder** (`find | sha256sum`, recomputed on every ensure), so it changes whenever
any file in the entity changes, while other creators (`event_drop_handler.sh` ...) write a **random** hash that stays put. Nothing ties either to a wallet. The chain exists
(`041.pal-chain`: wallets, `chain_send`, a SHA-256 proof-of-work `chain_miner`, `chain-hq`) but only wallets created by hand
(`chain_create_wallet <wallet_id> <password>`) write transactions. A transaction is one line, `TX|<from>|<to>|<amount_millicones>|<ts>|<tx_id>`,
appended to `041.pal-chain/data/pending_tx.txt` (and the peer outbox) for miners to include in the next block. v1 limits, named in its own
standard: **no transaction signing** (any local process can forge a `from`), `wallet_id` is letters/digits/`_`/`-` only, and the peer
layer (`palnet_peer.c`) is same-machine only.

**Proposal (cheap now, expensive to retrofit later, so do it in the same pass as Q005 which touches every entity once anyway):**
1. **`entity_uid`** — written once to `<entity>/entity_uid.txt`, never changed, never reused. For an entity that already has `PAL | hash` the uid is that hash
   **frozen** (continuity with the identity the house already designed; 53 of 55 entities); otherwise a fresh random sha256. The human label
   (`instance_id.txt`) stays. **Follow-up needed:** `livedesk_hash_dir()` hashes the whole folder, so adding `entity_uid.txt` and `inventory/zz.phone/` changes every
   entity's content hash; exclude both from that hash (or accept one drift) when the spawn hook lands.
2. **`entity_hash = sha256(entity_uid)`** (hex). Everything below is a pure function of it, so it can be recomputed and verified by anyone.
3. **Phone number = a readable rendering of the hash**: 11 decimal digits from the hash, shown `NNN-NNNN-NNNN` (collision checked at
   creation against `phones.index`; on a clash, take the next 11 digits). The number is the phone's lookup key; `phone.pdl` stores
   `number`, `owner_uid`, `owner_label`. Numbers never change; if an entity is deleted its number is retired, not reissued.
4. **`wallet_id = "e" + first 24 hex of entity_hash`** (valid under the chain's `wallet_id` charset). Wallets are **derived, not created**
   for 54+ entities: an entity's wallet is only created in the chain when it first needs to transact (lazy), so there are no dormant
   passworded wallets lying around. The wallet credential for an entity wallet is held by the **server**, not the entity.
5. **Who writes to the chain:** entities never write `pending_tx.txt`. Only the **server** (single writer) writes transactions on their
   behalf. First use is **message anchoring**: a zero-amount `TX|<from wallet>|<to wallet>|0|<ts>|<sha256 of the message line>` per routed
   message (or per batch), giving the history a tamper-evident hash chain the miner includes in blocks. **Anchoring is only meaningful
   once transactions are signed** (v1 gap), so signing is a prerequisite quest before this is more than a log; until then anchors are
   advisory. Cross-machine anchoring waits on the peer layer leaving 127.0.0.1 (`CROSS-MACHINE-NETWORKING-PLAN.md`).

**History store limit (owner, 2026-10-06):** every phone has caps in `phone.pdl`: `history_max_lines` (default 2000) and
`history_max_bytes` (default 262144). When `history.txt` or `inbox.txt` exceeds a cap the **server** rotates it (the older part moves to
`history.1.txt` / `inbox.1.txt`, a fresh file starts; keep `rotate_keep` generations, default 3, oldest dropped). Readers that track a
byte offset by size growth must treat "size smaller than my offset" as *rotated: resync from 0* (the house already does this: the
dock's marker reader in `khtpm_core_render.c` resyncs on truncation). The owner's own `outbox.txt` is rotated by the owner's op
(`phone_send`) under the same caps, because only its owner writes it. The server's ledger is separate (`ledger_max_bytes`, rotate by month);
the **chain anchor** of a rotated line stays valid because it hashes the line, not its position.

### 3d. One board layout for every window (owner observation, 2026-10-06)

> "the grave and servers are able to use much the same layout"

Every window in this family is the same three parts: a **list of rows** (quests, ghosts, entities, phones, messages), a **detail / history
pane** (the tail of an append-only file), and **actions** (buttons, plus a reply field where text goes in). So there is **one generic
`board.chtpm` layout**, and each window is that layout plus a data source and a row template:

| Window | rows from | detail pane | actions |
|---|---|---|---|
| 🪦 grave | `^.grave/quests/*/QUEST.md` headers | the quest's `## Log` | post, assign, set status |
| 👻 ghost roster | `^.ghost/roster/*/ghost.pdl` | the ghost's `history.txt` | appoint, retire, send out, lease |
| 🖥️ server | phones.index (all entities) | `ledger.txt` tail | route policy, kill switch, release all leases |
| 📱 phone | the conversation (`history.txt`) | the selected message | reply (cli_io), call, forward |

Built on the layout machinery of 2026-10-06 (`<repeat>` over vars, `<overlay>`, nav numbering on every interactive row, minimize/close
chrome), so one fix to the board layout fixes every window, and the user edits it once in the layout studio
(`HQ-LAYOUT-STUDIO-DESIGN.md`). The data source is a small `.pdl` next to the window (`board.pdl`: `SOURCE`, `ROW`, `DETAIL`, `ACTION` lines),
not new C per window — the house rule: zero new per-project renderer code. Quest Q006.

### 3e. Ghosts and phones can possess and move entities (owner observation, 2026-10-06)

> "the phones can move entities just like the ghosts should be able to possess and move etc entities"

Possession already exists and must not be reinvented: `CURSWORD-POSSESSION-DESIGN.md` (the cursword and the xelector as possessors,
`xelector_01/state.txt` `possessed_id`), and `@.apps/PORTABLE_ENTITY_ARCHITECTURE.md` §4: *possessing an entity makes the host set
`active_target_id`, and the target's OWN `piece.pdl` METHOD rows become the actions* (`${piece_methods}`). Moving is already an op
(`&.widgits/entity-cli/move_entity.pal`, `move_entity_on_desk.sh`, `khtpm_move_range.c`; the pc-hq Move action, `@.apps/piececraft-hq/MOVE-RANGE-SURVEY.md`).

So the rule is: **a possessor is any entity that holds a possession; what it can do is exactly what the target's own methods offer.**
- **Ghosts and robots are possessors**, like cursword. A ghost sent to test an entity possesses it, runs its methods through the same ops
  a human uses (Move, Take, Place, Interact), and lets go. No parallel "bot control" path.
- **Phones are remote controls**, not possessors themselves. A message `command|<target>|move 12,7` in a phone's outbox goes to the
  server; the server checks the **lease** and policy, then runs the same op on the target and logs the result to both phones and the ledger.
  So "a phone can move an entity" means "the server executes the owner's command through the normal methods".
- **Lease** (new, small): a possession granted by the server to a possessor for one target, with a scope (`move`, `take`, `place`,
  `interact`) and an expiry, recorded as an append-only `LEASE` / `RELEASE` line in the server ledger and mirrored in the target's
  `possession.txt`. Cursword/xelector keep their existing `possessed_id` mechanism untouched; leases cover the new possessors.
- **Safety:** leases expire; one live lease per target; the owner's cursword/xelector and entities flagged `locked` can never be leased
  without the owner's explicit OK; every command and result is logged; the server window has **Release all** (kill switch); movement
  respects the existing move range rules (no teleporting through walls).
- New message kinds: `command`, `lease`, `release`, `result` (added to §3's table).

### 3f. Built from pal/ops events, tunable weights, and tomom as an advisor (owner, 2026-10-06)

> "we will try to build all these with pal/ops events, and tunable weights, inside the tomom system right? reusable events?"

Yes, with one line drawn. Everything here uses pieces the house already has; this section only says how they fit. Read-and-verified 2026-10-06: the Prisc+Ops standard
(`.pal` loop `exec`s compiled ops, append-only ledgers + size-growth cursors, `PRISC-OPS-ARCHITECTURE.md`), the data-driven event command registry
(`#.ref/menu/event_commands.registry.pdl`: a `COMMAND` block = `LABEL / FIELD1 / FIELD2 / PARAMS / TEMPLATE / END`, add a command by editing that one file, no recompile),
the FSM harness joints (`%.harnesses/harnecient-fsm/tunables.conf`: named values sourced by `run_plan.sh`, every value still a commented default today) and its dataset
(`observations.log`, one row per step; created by plan runs, does not exist in this checkout yet), and tomom (`#.Z.HUMAN_LLM/3.stage.llm.tomom...`: a real C pipeline incl. a `meta_rl`
curriculum selector that learns from feedback; dormant). **Not read yet:** whether tomom's trainer can consume a `.pdl` ledger directly.

**1. Events are small, deterministic, reusable units.** Each is one compiled op with a fixed argument list and one registry entry, so any event script (events-hq, a ghost plan,
a robot) can call it the same way. First set (names are working names): `phone.send` (append a validated line to the owner's outbox), `server.route` (the single-writer router:
outbox -> recipient inbox + history + ledger), `lease.grant` / `lease.release` (§3e), `ghost.assign` (give a quest to a ghost), `quest.score` (run the quest's `verify.sh`, record the verdict).
A "step" a ghost takes is an event call, not a free-form model action.

**2. Scoring is never a model's opinion.** `quest.score` runs the quest's own verifier script (like Q003's `verify.sh`) and records exit code + output. A model can propose; only a verdict
from a script moves a quest to `done`. This is the harness rule ("nothing is scored by the model saying done").

**3. Weights only choose among valid options, and every choice is a named joint.** Joints live in `^.hai-server/tunables.conf` (same sourced-assignment format and comment discipline as the
FSM's file; defaults commented, edit by hand today): which ghost gets a quest (`ghost_pick_weight_*`), lease length (`lease_default_s`, `lease_max_s`), routing priority and rate caps
(`route_max_msgs_per_min`), when to escalate to the owner (`escalate_after_fails`), history caps (§3c). No weight may bypass a rule: leases still expire, `locked` entities still need the
owner, caps still apply. A weight changes order and amount, never permission.

**4. Every use of a joint is a ledger row** in `^.hai-server/observations.log`: `<epoch_ms>|<event>|<joint>=<value>|<inputs>|<verdict>`. That file is the Stage-0 dataset
(`HARNESS-DELEGATION-PIPELINE.md` §7): hand-tune first; a heuristic or tomom can later *propose* new joint values from it.

**5. tomom is an optional advisor behind the server.** It reads `observations.log`, proposes joint values, and the server applies them as ordinary tunables under the same rules.
Everything works with tomom off. Training events (rl / irl / fsm) are runs of the same events with a recorded verdict, so a ghost's history is also its training data.

**IRL (learning from the owner's demonstrations) — status 2026-10-06.** "IRL" here is read as inverse reinforcement learning (confirm with the owner): learn what the owner wants from
what the owner does. Step 1 is built: **a human-only input log.** `entity_menu_history/<pid>.txt` receives both real X input and harness relay writes in one format, so it cannot say who did
what; the renderer now also appends every REAL X key/click (only from `kh_capture_key` / `kh_capture_click`, never from the relay poll) to `#.desktop/human_input/<pid>.txt`:
`<epoch_ms>|<pid>|<window label>|KEY|<code>` or `...|CLICK|<button>|<x>|<y>`. Tested live on the hotbar: a harness-written key did not appear in it; a real X key did (shift, 65505).
Step 2 (same day): every line also carries the CONTEXT: a key records the focused element (`focus=<nav>|id=|label=`), a click records the element under the pointer (`nav=<n>|id=|act=<its onclick>|label=`). Tested live: three real clicks on the hotbar logged `nav=2 id=hb1`, `nav=5 id=hb4`, `nav=8 id=hb7` with their actions; `$.crypts/irl-demos.sh` prints them as readable lines. Limit: a click that starts a window drag (title area / background) is handled before the capture, so it is not logged (the relay never had it either). Caveats: it records typed characters like the relay already did, including into text fields (no password-field exclusion); local only, gitignored.
Still needed before any learner: (2) demonstrations as event calls with context (which element, which nav number, what it did) rather than raw keys/pixels, (3) enough recorded demos with
verdicts, (4) a learner (first: simple preference counts that only PROPOSE tunable values; real IRL later). tomom's `meta_rl` learns from 1-10 feedback by gradient ascent (README-level; code not read): reinforcement from scores, not IRL.

**Built so far (Q009):** `phone_send_op`, `server_route_op`, `^.hai-server/tunables.conf`, ledger + observations rows, `verify.sh` (14 sandbox checks PASS). Not wired to a loop, not run on live phones.

## 4. The server (`^.hai-server`, 🖥️)

The server is the **one entity that manages all the others through their phones**:
- **Routes:** reads every phone's `outbox.txt` (by marker growth), validates, and appends to the target's `inbox.txt` and to both `history.txt` files.
- **Single writer of inboxes**, so ordering and permissions have exactly one place: who may message whom, rate limits, size limits, loop guards.
- **Keeps the global ledger** (`server/ledger.txt`, append-only): every message ever routed — the audit trail and the training data for later (RL/IRL).
- **Manages entities:** wakes/sleeps robots and ghosts, assigns quests (a `task` message carrying a quest id; the grave stays the source of truth for quest state), escalates `ask-human` upward.
- **Dashboard window** (layout-driven, nav-numbered): Entities | Phones | Traffic | Escalations | Ledger. Many servers are allowed (one per grave / project) the same way many graves are.

## 5. Robots (`^.hai-robot`, 🤖) and sub-bots

A robot is a pal with: a persona (instance-scoped Concept Bank later, blueprint §3.1), its own `chat_history.txt` (the chat the owner
has with it), a task list, a phone, and a brain. **Spawning sub-bots** is a `spawn` message to the server (the robot never forks
processes itself): the server checks the policy, creates the child pal (placed like any pal, with `parent=<robot>` in its `robot.pdl`
or `ghost.pdl`), gives it a phone and a quest, and records the family tree in the ledger. A sub-bot's results return to its parent by phone.

Limits that keep this safe (configured on the server): max depth, max live children per robot, max total entities, per-entity spend
cap, and a kill switch the owner can press from the server window. Spawning is the one action that is **always logged and, by default,
approved by the manager** until the owner says otherwise.

## 6. Placement and look

All of these are placeable from the **h-ai menu palette** exactly like the graves/ghosts (part 1 §6b): `tp_place_desktop.+x` makes the
pal + `DESK` row, so a reset brings them back. Robots and ghosts walk/sit on the desktop or in pc-hq like any entity; phones sit next to
their owner; the server sits where the owner puts it. Everything interactive is nav-numbered (house accessibility standard).

## 7. Phases

0. **Now:** this doc, the three skeleton folders with READMEs and templates.
1. A phone pal + the file protocol + two small C ops (`phone_send`, `phone_read`, per the prisc+ops rule: C ops, not shell) proven between two dummy pals by the relay; marker-growth read.
2. The server router (one prisc `.pal` loop + ops): outbox → inbox + histories + ledger; permissions and rate limits.
3. The phone viewer window (conversation view, reply field) and the ringing glyph.
4. A real robot (the generalized `robot_chat_001`) that chats, takes tasks and talks to a ghost by phone.
5. Spawn sub-bots through the server, with the limits above.
6. Grave integration: `task` messages create/advance quests; `done`/`fail` close them (manager verifies, per part 1).

## 8. Open questions for the owner

1. ~~One phone per entity?~~ **DECIDED 2026-10-06: one phone in every entity's inventory** (§3b). Still open: may an entity hold more than one (e.g. a second for a private line)? Default: no, threads by `ref`.
2. Does the human have **one** phone (all robots ring it) or one per robot? Default proposed: one human phone, conversations separated by sender.
3. One server for the whole house or **one per grave/project**? Default proposed: one house server first.
4. History retention: append-only forever, or rotate old months into an archive folder? Default proposed: forever, archive later.
5. Do ghosts get phones by default? Default proposed: yes (that is how a ghost reports to its grave and asks for approval).
6. Folder names `^.hai-robot` / `^.hai-phone` / `^.hai-server` OK as written?
