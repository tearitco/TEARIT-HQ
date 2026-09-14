Play Mode, the Entity Harness, and "move" Common Events — Design Doc
=====================================================================
Promoted from `11.brainstorm/2026-09-14/PLAY-MODE-ENTITY-HARNESS-AND-
MOVE-EVENTS-BRAINSTORM.md`, same day, direct instruction ("remember
this? wanna make a design document before it"). VISION + ARCHITECTURE
PHASE — nothing in this doc is built yet. Two real open design
questions remain unsettled (§6) — read those before writing any code.

## 1. The actual goal

Give Common Events a real "move" command type — an entity that wanders
on its own, or pathfinds toward another named entity — so the
**trigger layer** (NIGHT_05, `touched_npc:NAME`-style triggers, see
`12.calendar/2026-09-13/sept-13-events-trigger-buildout-tracking.md`)
can be tested and proven end-to-end **without needing real player
movement injected**. An NPC (or the player's own piece) walking itself
into a trigger tile is a real, repeatable, automatable test case; a
human manually dragging a piece around every time is not.

This is explicitly **downstream of, and in service to, the
trigger-layer build-out** — not a parallel or competing priority.
"move" events are the tool that makes the trigger layer's own real
acceptance test repeatable without a human. See §7 for the real build
order.

## 2. Real, existing groundwork this builds on (checked, not assumed)

- **`xelector`** (`@.apps/piececraft-hq/ops/pc_generate_chunk.c`,
  `pc_menu_input.c`) — the real, existing player-controlled cursor
  entity. Arrow-key-driven today (`pc_menu_input.c` ~line 992:
  "xelector's real position actually changes (arrow keys)"). Already
  the mechanism player-piece movement is proven against.
- **`chicken`** — a real, existing NPC entity with real AI wander
  movement already built (`pc_clock_daemon.c`: "elapsed wall-clock ->
  game seconds, chicken wander on..."; `pc_generate_chunk.c` line 400:
  "give the chicken the master-ledger AI"). The closest real precedent
  for "move" Common Events — the wander behavior already exists for
  ONE hardcoded entity; the ask is to generalize it into a real,
  reusable Common Event command any entity can use.
- **The taskbar already has a real "play"/"pause" concept**
  (`khtpm_taskbar_manager.c` ~line 3689, `livedesk_build_player_menu()`)
  — almost certainly the real anchor for "8.player in tb." Not
  inventing a new taskbar concept from scratch, just extending an
  existing one.
- **pc-hq has no Play button yet** — confirmed, grepped
  `@.apps/piececraft-hq/` for "play-test"/"▶ Play"/"Play button",
  nothing found. Needs to be added as part of this work.
- **NEW, as of today (2026-09-14) — a real, proven right-click menu
  mechanism now exists and is the concrete foundation for §4's
  Play-Mode context menu**: `khtpm_core_render.c`'s
  `kh_open_cli_io_context_menu()` writes a real, generated
  `menu.chtpm` and launches it via `launch_khtpm_menu()` — the exact
  same real, separate-process, theme/CSS-rendered mechanism every
  entity's own right-click menu already uses (real nav numbering, real
  `[^]`/`[>]` focus prefix, real header row, because it genuinely IS
  the same mechanism, not a lookalike — direct correction this same
  session: "a window is an entity, and vice versa thats how u should
  think of it"). Cross-process actions that need to reach back into
  live, in-process state (that build's own CUT/COPY/PASTE) bridge
  through a real action file polled from `hq_idle_tick()` — the same
  real pattern `taskbar_settings_action.txt` already uses house-wide.
  **This is real precedent for how the Play-Mode context menu (§4)
  should be built** — not a new, bespoke popup mechanism, the same
  generated-`menu.chtpm` + `launch_khtpm_menu()` shape, with
  Move/Inventory/Ops/Stats/STOP as real `<item>` rows, STOP's own
  cross-process "turn off Play Mode" effect bridged the same
  action-file way CUT/COPY/PASTE already are, if that state genuinely
  needs to live back in the original process (unconfirmed — piececraft-
  hq's own Play Mode state may not even be khtpm-family process state
  at all, see §6).

## 3. New Common Event command: "move"

Two real modes:

- **Wander** — real, generalized version of what `chicken` already
  does (currently hardcoded to one entity/one clock-daemon path). Any
  entity gets this behavior via a real Common Event command, not
  bespoke C.
- **Pathfind-to-entity** — move toward a NAMED target entity until
  adjacent/touching. The real mechanism that lets an NPC walk itself
  into range of another entity's trigger tile, closing the loop with
  the trigger layer without a human moving anything.

## 4. Play Mode

A real, distinct interaction state, toggled ON via any of:

- The taskbar's own "8.player" (extending the existing play/pause
  concept, §2), OR
- An individual "▶ Play" button on a specific entity, OR
- A "▶ Play" button in pc-hq itself (does not exist yet — real, scoped,
  needs building as part of this work).

While Play Mode is ON, a real, NEW context menu appears on entities
(not the existing right-click menu — a Play-Mode-only one), built on
§2's real `menu.chtpm`-generation mechanism, Fire-Emblem/tactics-RPG
style:

- **Move** — shows a real "tactics range" style movement overlay (the
  reachable-tile highlight a tactics RPG shows before you commit to a
  move — a real, distinct UI element from the plain click-to-place the
  desktop's own tile-drag system already has; don't conflate the two).
- **Inventory**
- **Ops**
- **Stats**
- **STOP** — exits Play Mode entirely (the one item that isn't about
  this specific entity — it's the global play-mode-off switch, always
  present).

## 5. The entity harness

The real, single, going-forward mechanism for moving ANY
player-controlled entity. Direct instruction: "thats how we will
officially move all player entity characters going forward."
`xelector` (player cursor) and `chicken` (NPC wander) are the two
real, already-built movement implementations in pc-hq today — **these
get reused/generalized as the harness's own real backing movement
code**, not reinvented. The harness is the real, shared entry point
every future player-controlled entity routes through, instead of each
getting its own bespoke movement code the way xelector/chicken
currently each have their own.

## 6. Real open questions — settle before writing code

1. **Is "Move"'s tactics-range overlay a genuinely new rendering
   primitive, or does it reuse something the desktop's own tile-grid/
   drag system already has?** piececraft-hq is a **separate engine**
   (`chtpm_parser_pal`/`prisc+x`, confirmed this session — nothing to
   do with the khtpm desktop family §2's context-menu mechanism lives
   in). Almost certainly new pc-hq-side rendering, not a
   `khtpm_core_render.c` concern at all — confirm before assuming
   otherwise. (The context-menu ITEMS themselves — Move/Inventory/Ops/
   Stats/STOP — are real `khtpm_core_render.c`-family UI per §2/§4; the
   tactics-range overlay that "Move" opens into is a different, pc-hq-
   side question.)
2. **Exact relationship between "the entity harness" and the existing
   `event_pkg`/Common Events system** — is "move" a Common Event
   *command* (compiled into `event.pal`, same family as Show Text/
   Change Gold), or a separate, always-available action outside the
   event-authoring system entirely (more like a built-in verb the
   harness itself exposes)? §3 assumes the former, matching
   `EVENT-COMMAND-REGISTRY-ARCHITECTURE.md`'s own data-driven-registry
   convention — confirm this is really the intent before building
   either way.
3. Inventory/Ops/Stats are named but not scoped at all yet — each is
   its own real design question (what data backs "Stats"? does
   "Inventory" reuse anything db-hq's Database tab already has?) — out
   of scope for this doc, flagged for their own pass later.
4. Does STOP need to gracefully interrupt an in-progress "move" (an
   entity mid-pathfind when Play Mode turns off), or is that an edge
   case to handle later?

## 7. Sequencing / real build order

1. Real trigger layer first (NIGHT_05's proposal — see the Sept 13
   tracking doc's own "next step"). Not started yet.
2. Enough of "move" (wander is probably the smaller, provable-first
   half) to drive one real automated trigger test.
3. The fuller Play Mode UI (context menu via §2's mechanism,
   tactics-range overlay, Inventory/Ops/Stats) as its own, later,
   separately-scoped pass.

Don't try to build all of this in one sweep — each step above should
land, get verified live, before starting the next.

## Next step

Not started. Waiting on the trigger-layer track (step 1 above) to
actually begin first. When this starts: settle §6's two load-bearing
open questions (rendering ownership, Common-Event-command vs
built-in-verb framing) before writing any code, per this house's
standing "ask first on a real design fork" rule.
