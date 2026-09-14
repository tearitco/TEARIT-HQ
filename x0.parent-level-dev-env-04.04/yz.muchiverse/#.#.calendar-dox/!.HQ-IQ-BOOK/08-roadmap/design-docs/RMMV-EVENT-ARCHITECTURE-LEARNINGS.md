RMMV Event Architecture — Learnings for the Master Ledger
=============================================================
Reference doc, 2026-09-14. Written after reading RPG Maker MV's real
engine source directly (`rpg_objects.js`, `rpg_managers.js` — local
copy, `RMMV_TSOTS]LINUX=elf?/__.Tearrmv SpaceShop388.m/www/js/`) so
future work doesn't have to re-read these ~10,600/~2,800-line files
again. Every claim below cites a real function/field name from that
source. Companion to `EVENT-TRIGGER-LAYER-PLAN.md`, which this doc
directly informs (§ "master ledger" question raised there).

## 1. How RMMV represents a trigger

`Game_Event`'s `_trigger` (`rpg_objects.js:8705`, `this._trigger =
page.trigger;`) is a plain integer straight off the event page's JSON:
`0`=Action Button, `1`=Player Touch, `2`=Event Touch, `3`=Autorun,
`4`=Parallel. No string labels, no separate condition sub-language —
the number IS the trigger type. `isTriggerIn(triggers)` (line 8583)
just checks `triggers.contains(this._trigger)`.

## 2. When the check happens — no ledger, no watcher, no cross-process anything

**Important, real finding: this house's proposed append-only-log +
separate-watcher design has NO analog in RMMV.** RMMV is single-
threaded JS — `Game_Player.prototype.update()` (line 7666) runs
`updateNonmoving()` (7753) every frame the player *isn't* moving, which
directly calls `checkEventTriggerHere([1,2])` in-process, in the same
call stack, no file, no IPC. That function (7878) does
`$gameMap.eventsXy(x,y).forEach(...)` — a live, synchronous, in-memory
scan of every event object on the current map, checking each one's
`_trigger` against the caller's filter array. `checkEventTriggerThere`
(7884) does the same one tile ahead of the player's facing direction,
for Action Button/Event Touch.

So RMMV's real trigger mechanism is: **poll everything in-memory, every
frame, synchronously, in one process.** There's no signal that outlives
a single tick and no cross-process handoff at all — because RMMV never
has more than one process to hand off between. Our house's design is
structurally different by necessity (khtpm's desktop and piececraft-hq
genuinely ARE separate processes, this house's own standing rule is no
linking between them) — the append-only-log + watcher shape is the
right answer for *our* constraint, not something RMMV already solved
and we're copying. Don't assume more 1:1 transfer than this.

## 3. Persistent trigger-adjacent state — the real "ledger" analog

Three flat global stores, all keyed simply, all just plain JS
objects/arrays:

- **`Game_Switches`** (line 515) — `this._data = []`, indexed by a
  single global numeric `switchId`. Global, not scoped to a map/event.
- **`Game_Variables`** (line 556) — same shape, numeric `variableId`.
- **`Game_SelfSwitches`** (line 582) — `this._data = {}`, keyed by a
  **composite key**: `[this._mapId, this._eventId, c.selfSwitchCh]`
  (confirmed at line 8640, `selfSwitchCh` is a letter, `'A'`-`'D'`).
  This is the one genuinely useful shape for us — **scope = map + event
  + sub-key**, exactly the granularity a shared ledger needs (a switch
  meaningful only to one specific NPC on one specific map, not global).
  `.value(key)` / `.setValue(key, value)` (594-604) just do
  `this._data[key]` — JS auto-stringifies the array key.

## 4. How a triggered event actually runs — no compile step

`Game_Interpreter.setup(list, eventId)` (line 8800) stores the page's
raw command list (`this._list = list` — a live array of `{code,
indent, parameters}` objects straight from the event page's JSON) and
an `_index` cursor. `update()` (8830) is called every frame by whatever
owns the interpreter and walks forward via `executeCommand()` (8923),
switching on `this.currentCommand().code`, until it hits a wait or runs
out of commands. **No compilation step at all** — this is structurally
different from this house's own `event.pal` → `cmd_N.sh` → `mr_
<command>` binary chain, which compiles once and executes a real shell
script. RMMV interprets the same live object list every single frame
instead. Our compiled-chain approach is not "RMMV but worse" — it's a
different, valid tradeoff (compile once, no per-frame JSON walk) suited
to our process-per-window architecture.

## 5. `$gameSystem`/`$gameSwitches`/etc. as RMMV's own "master ledger"

`DataManager.makeSaveContents()` (`rpg_managers.js:430`) is the closest
real RMMV concept to a single master ledger: it flattens `$gameSystem`,
`$gameSwitches`, `$gameVariables`, `$gameSelfSwitches`, `$gameActors`,
`$gameParty`, `$gameMap`, `$gamePlayer` into one plain object and
serializes it whole (`extractSaveContents`, 446, reverses it). It's not
a single flat keyed store though — it's a bag of separately-typed
sub-objects, each with its own internal key shape (global numeric ID,
or the `[map,event,ch]` composite). There is no single universal
`KEY|VALUE` table in RMMV — each concern (switches, variables,
self-switches) is its own small store with a key shape matched to its
own real scope.

## 6. Concrete recommendation for THIS house's master ledger

Match `Game_SelfSwitches`' real scoping shape (§3) — not a flat global
list like `Game_Switches` — since our real need (an NPC's trigger state
on one specific map) is exactly what self-switches solve. Use this
house's own established `.pdl` row convention (already proven:
`condition.pdl`'s `COND | trigger | <value>` rows, cited in
`EVENT-TRIGGER-LAYER-PLAN.md` §2) rather than inventing new file
syntax:

```
LEDGER | <map_id> | <entity_id> | <key> | <value>
```

e.g. `LEDGER | cdda_sample | t | touched | 1`. One shared, append-
friendly, `.pdl`-row-shaped file both khtpm (desktop pals) and
piececraft-hq can write to and read from with the exact same
`pdl_open()`/row-parsing code every other `.pdl` consumer in this house
already has — no new parser. `board_events.txt` (the trigger-layer
plan's proposed append-only signal log) stays a SEPARATE, transient
file — it's a stream of "this just happened" events for the watcher to
consume once; this ledger is the durable, keyed, queryable state
(closer to RMMV's self-switches) a Common Event's `condition.pdl` might
check ("has this NPC already been talked to") before firing again.
Don't conflate the two — RMMV itself keeps its transient per-frame
poll and its persistent self-switch store as two separate mechanisms
too (§2 vs §3), which is real precedent for keeping ours separate as
well.

## What does NOT transfer from RMMV

- The synchronous per-frame poll (§2) — we have no single process to
  poll from; keep the file-relay watcher design.
- The live-interpreted command list (§4) — we have a working compiled
  chain already; no reason to switch to per-frame interpretation.
- `Game_Switches`' flat global numeric-ID scoping — too coarse for a
  multi-process, multi-entity house; `Game_SelfSwitches`' composite key
  is the real, useful shape (§6).

## 7. Addendum (2026-09-14) — a real, closer-to-home precedent found AFTER §6, and it changes the answer

Checked this house's own existing "ledger"/shared-state prior art before
finalizing anything, per direct instruction ("those sorts of things
should always be referenced"). Two real, concrete things change §6's
recommendation:

**`101.lpns+map+4/data/master_ledger.txt`** — a real, WORKING, already-
proven in-house ledger for a turn-based multiplayer board game. Format
(header row, real, currently in the file):
```
timestamp|epoch|player|turn|action_data|action_type
2026-07-24T19:00:00|1|alice|0|x:1,y:0|move
```
Critically, per `ledger-4-agent-trace.md` §6: **this is ONE append-only
file that is ALSO the replayed source of truth for all game state** —
`game_turn_input.c`/`game_compose_frame.c` both reconstruct player
positions by replaying the whole ledger from `config.txt`'s starting
values forward. This directly CONTRADICTS §6's recommendation to keep
`board_events.txt` (transient signal) and a separate keyed `LEDGER`
file (durable state) apart — this house has already built, and proven
live, the single-unified-append-log-as-source-of-truth pattern instead,
in a directly comparable turn-based-game context (closer to
piececraft-hq than RMMV is). **This is a better precedent to follow
than RMMV's §3 split, being real, in-house, and already working** —
worth strongly reconsidering §6's two-file split in favor of one
append-only ledger the trigger watcher both reads (for new lines) and
can replay (for "has this already fired" checks), rather than
inventing a second keyed store.

**`TPMOS_DRAGON_COMPAT.md`'s explicit standing architecture rule**
(house-wide, not project-specific): *"Architecture Decision:
Self-Contained vs Shared... Recommendation: Keep projects
self-contained. Code duplication is acceptable for portability."* This
matters directly for the "where should this ledger live" question the
user asked: the house's own standing guidance leans AGAINST a shared
top-level ledger directory that both khtpm (desktop) and piececraft-hq
reach into — favoring each project owning its own state.

**The one real, existing cross-project hand-off precedent**:
`44.xyz.01.00/exchange/` — a real, currently-EMPTY, top-level directory
(built for the drag-drop-test harness's pet-import case,
`dd_check_import.c`: `<exchange_dir>/<pet_id>/state.txt` +
`piece.pdl`). It's narrow and explicit — a one-time hand-off drop zone
for moving ONE entity from one app to another — not a continuously-
polled shared state bus. This is the real shape a THIN cross-app
boundary should take here, if one is ever needed, not a general shared
ledger directory.

**Revised recommendation**: piececraft-hq's `board_events.txt` (and any
replay-ledger built the `master_ledger.txt` way, per this addendum's
first point) should stay SELF-CONTAINED inside piececraft-hq's own
project directory, matching house convention — not moved to a new
top-level shared dir. The actual "sharing" the trigger layer needs
between khtpm (desktop, where events-hq's editor lives) and piececraft-
hq already exists and doesn't need a new mechanism: each Common Event's
own `event_pkg/` directory (with its `condition.pdl`/`event.pal`) is
already the real, established shared contract both events-hq's editor
and any future pc-hq-side bridge script read from — this was already
true before today, nothing new to build there. A genuinely new
top-level shared ledger directory is not needed for the trigger layer
as currently scoped; keep it self-contained, per house convention,
unless a real, concrete future need for cross-app hand-off arises — and
if it does, model it on `exchange/`'s narrow shape, not a shared bus.

## 8. Networked / multiplayer mode (2026-09-14) — real, proven, two-tier, already built

Direct question raised: does the ledger need to account for multiple
real-time viewers of the same session, including over a network? Yes,
and this house already has BOTH tiers of that solved, as two proven,
real, separate mechanisms that compose cleanly with the ledger shape
already recommended above (§7) — no redesign of the ledger itself
needed, just an optional second process attached to it later.

**Tier 1 — local-only, no sockets, already proven by `101.lpns+map+4`
itself.** Multiple players/NPCs on ONE machine already share one
`master_ledger.txt` today (§7) — any local process can append to it,
any local process can replay it, in arrival order, with zero network
code. This is already "local multiplayer" in the sense that matters
for a shared game session; it just doesn't cross a machine boundary.

**Tier 2 — cross-machine, real sockets, already built and proven in
production apps.** `palnet_peer.c` (`&.2.muchi-verse/PAL-NET-STANDARD.
txt` is its own governing spec — read that in full before touching
this, per its own header comment) is a standalone, reusable,
SYMMETRIC peer-to-peer companion binary already used by pal-chain,
pal-forum, pal-chat-irc, TSC_ELO, and the zoo/pet apps. Confirmed by
direct read of the real source:

- The GUI/game process itself never touches a socket. It only writes
  to its own append-only **outbox** file — the SAME shape as §7's
  ledger recommendation, not a new format.
- `palnet_peer` tails that outbox, and for every genuinely NEW line
  (byte-offset tracked, `read_outbox_new_lines()`), broadcasts it as a
  `DATA|<node_id>|<content>` message to every connected peer over a
  real local TCP socket (peer discovery itself is file-based — a flat
  "presence" directory scan, not sockets — matching this house's
  general file-relay convention even for discovery).
- Each peer's own **inbox** file is itself an append-only merged log
  (`<sender_node_id>|<content>` per line) — written in the order
  messages actually arrive at that specific peer.
- A newly-connecting peer is replayed the FULL backlog
  (`replay_backlog_to_peer()`), not just the latest value — a late
  joiner catches up completely. This was a REAL, confirmed, live-caught
  bug fix (see the function's own header comment): an earlier version
  only mirrored the latest line and silently dropped every event after
  the first, which is exactly wrong for "every trigger fired is its own
  event" — fixed at the shared, reusable level specifically because
  pal-chain's TX/BLOCK stream and pal-forum's posts/likes/DMs both need
  it, not worked around per-app.
- Honest limit: this gives per-peer ARRIVAL order, not a global total
  order with conflict resolution — that's what pal-chain's own mining/
  consensus layer is for, one level up. Fine for chat/forum/game-
  trigger events; not a substitute for real consensus if two peers
  could genuinely race to claim the same authoritative outcome.

**Why this doesn't change the ledger's own design (§7):** `palnet_peer`
consumes an append-only file as its outbox and produces an append-only
file as its inbox — which is exactly the shape already recommended for
piececraft-hq's own local ledger. Going from single-machine to
networked multiplayer is not a ledger redesign — it's pointing an
already-built, already-proven `palnet_peer.+x` companion process at the
ledger file that already exists, the same way pal-chain/forum/IRC
already do. Build the ledger once, local-only, LPNS-style, first;
networked mode is a later, additive, drop-in attachment, not a
rewrite — keep it explicitly OUT of scope for the trigger layer's own
first working version (§7's "smallest provable proof" bar still
applies), but design the ledger's append-only shape with this
attachment path in mind from the start so nothing has to change later.
