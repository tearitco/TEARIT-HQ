# Brainstorm: Play Mode, the entity harness, and "move" Common Event commands

**Status: brainstorm only, not scheduled, no code written.** Started
2026-09-14, direct dictation while the trigger-layer track
(`12.calendar/2026-09-13/`) is still ahead of this in priority. Written
down now specifically so it isn't lost, per direct instruction ("wanna
write this down").

## The actual goal, restated

Give Common Events a real "move" command type - an entity that wanders
around on its own, or pathfinds toward another named entity - so the
**trigger layer** (NIGHT_05, `touched_npc:NAME`-style triggers) can be
tested and proven end-to-end **without needing real player movement
injected**. An NPC (or the player's own piece) walking itself into a
trigger tile is a real, repeatable, automatable test case; a human
manually dragging a piece around every time is not.

This is explicitly in service of the trigger-layer track, not a
separate feature request - the reason "move" events matter right now
is they're what makes the trigger layer's own acceptance test
(§ "how you'll test it," `12.calendar/2026-09-13/` orientation
section) reproducible without a human at the keyboard every time.

## Real, existing groundwork this builds on (checked, not assumed)

- **`xelector`** (`@.apps/piececraft-hq/ops/pc_generate_chunk.c`,
  `pc_menu_input.c`) - the real, existing player-controlled cursor
  entity. Arrow-key-driven today (`pc_menu_input.c` ~line 992: "xelector's
  real position actually changes (arrow keys)").
  Already the mechanism player-piece movement is proven against.
- **`chicken`** - a real, existing NPC entity with real AI wander
  movement already built (`pc_clock_daemon.c`: "elapsed wall-clock ->
  game seconds, chicken wander on..."; `pc_generate_chunk.c` line 400:
  "give the chicken the master-ledger AI"). This is the closest real
  precedent for "move" Common Events - the wander behavior already
  exists for ONE hardcoded entity; the ask is to generalize it into a
  real, reusable Common Event command any entity can use.
- **The taskbar already has a real "play"/"pause" concept**
  (`khtpm_taskbar_manager.c` ~3689, `livedesk_build_player_menu()`) -
  this is almost certainly the real anchor for "8.player in tb" -
  confirms this isn't inventing a new taskbar concept from scratch,
  just extending an existing one.
- **pc-hq has no Play button yet** - confirmed, grepped
  `@.apps/piececraft-hq/` for "play-test"/"▶ Play"/"Play button",
  nothing found. Needs to be added, per direct instruction ("which we
  need to add if one doesn't exist").

## The real, concrete shape (as dictated)

1. **New Common Event command type: "move."** Two real modes:
   - **Wander** - real, generalized version of what `chicken` already
     does (currently hardcoded to one entity/one clock-daemon path) -
     any entity can get this behavior via a real Common Event command,
     not bespoke C.
   - **Pathfind-to-entity** - move toward a NAMED target entity until
     adjacent/touching. This is the real mechanism that would let an
     NPC walk itself into range of another entity's trigger tile,
     closing the loop with the trigger layer without a human moving
     anything.

2. **Play Mode - a real, distinct interaction state**, toggled ON via
   any of:
   - The taskbar's own "8.player" (extending the existing play/pause
     concept above), OR
   - An individual "▶ Play" button on a specific entity, OR
   - A "▶ Play" button in pc-hq itself (**does not exist yet - real,
     scoped, needs building** as part of this work).

3. **While Play Mode is ON, a real, NEW context menu appears on
   entities** (not the existing right-click menu - a Play-Mode-only
   one), Fire-Emblem/tactics-RPG style:
   - **Move** - shows a real "tactics range" style movement overlay
     (the reachable-tile highlight a tactics RPG shows before you
     commit to a move - a real, distinct UI element from the plain
     click-to-place the desktop's own tile-drag system already has;
     don't conflate the two).
   - **Inventory**
   - **Ops**
   - **Stats**
   - **STOP** - exits Play Mode entirely (the one item that isn't
     about this specific entity - it's the global play-mode-off
     switch, always present).

4. **The "entity harness" - the real, single, going-forward mechanism
   for moving ANY player-controlled entity.** Direct instruction:
   "thats how we will officially move all player entity characters
   going forward." `xelector` (player cursor) and `chicken` (NPC
   wander) are the two real, ALREADY-BUILT movement implementations in
   pc-hq today - **these get reused/generalized as the harness's own
   real backing movement code**, not reinvented. The harness is the
   real, shared entry point every future player-controlled entity
   routes through, instead of each getting its own bespoke movement
   code the way xelector/chicken currently each have their own.

## Real open questions, not yet answered (don't guess past these when building)

- Is "Move"'s tactics-range overlay a genuinely new rendering
  primitive, or does it reuse something the desktop's own tile-grid/
  drag system already has? Given piececraft-hq is a **separate
  engine** (`chtpm_parser_pal`/`prisc+x`, confirmed earlier this
  session - nothing to do with the khtpm desktop family), this is
  almost certainly new pc-hq-side rendering, not a khtpm_core_render.c
  concern at all - worth confirming before assuming otherwise.
- Exact relationship between "the entity harness" and the existing
  `event_pkg`/Common Events system - is "move" a Common Event *command*
  (compiled into `event.pal`, same family as Show Text/Change Gold),
  or a separate, always-available action outside the event-authoring
  system entirely (more like a built-in verb the harness itself
  exposes)? The framing above ("new Common Event command type") assumes
  the former, matching `EVENT-COMMAND-REGISTRY-ARCHITECTURE.md`'s own
  data-driven-registry convention - confirm this is really the intent
  before building either way.
- Inventory/Ops/Stats are named but not scoped at all yet - each is
  its own real design question (what data backs "Stats"? does
  "Inventory" reuse anything db-hq's Database tab already has?) - out
  of scope for this brainstorm, flagged for their own pass later.
- Does STOP need to gracefully interrupt an in-progress "move" (an
  entity mid-pathfind when Play Mode turns off), or is that an edge
  case to handle later?

## Sequencing (per direct instruction this session)

This is explicitly **downstream of and in service to** the trigger-
layer build-out (`12.calendar/2026-09-13/`), not a parallel or
competing priority - "move" events are the tool that makes the trigger
layer's own real acceptance test repeatable without a human. Build
order, once work actually starts: real trigger layer first (NIGHT_05's
proposal), then enough of "move" (wander is probably the smaller,
provable-first half) to drive one real automated trigger test, THEN
the fuller Play Mode UI (context menu, tactics-range overlay,
Inventory/Ops/Stats) as its own, later, separately-scoped pass - don't
try to build all of this in one sweep.

## Next step

Not started. Waiting on the trigger-layer track to actually begin
first (see that doc's own "next step"). When this starts: settle the
two open questions above (rendering ownership, Common-Event-command vs
built-in-verb framing) before writing any code, per this house's
standing "ask first on a real design fork" rule.
