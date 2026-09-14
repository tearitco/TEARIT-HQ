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
