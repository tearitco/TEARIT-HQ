# Game-Readiness Gap Analysis — 2026-08-27

**GAP #1 RETIRED (2026-08-29)**: superseded by
`MAPS-TILES-ZLEVELS-CONSOLIDATED-SPEC.md` (same directory) — start
there, which routes to the real, detailed, in-progress spec
(`TILE-SYSTEM-DESIGN.md`, real autotile/tileset code already built and
verified) and its own real next-step list. Not a separate design
problem anymore; don't re-open gap #1 as if it still needs fresh
design.

**Gap #0 (message-box rendering/suspension)**: RESOLVED by direct user
decision, recorded in full in `CURSWORD-SOUL-VISION.md` §4 — continuous
mode pauses via the existing `livedesk:clock:pause` mechanism unless
marked non-blocking; turn-based mode folds the message into the
current turn with a settable turn-cost variable.

**Gap #2 (event commands)**: also since substantially advanced this
session — Message + Character commands (Task 1), all remaining Party/
Actor commands, and full Flow Control (Loop/Break/Conditional Branch/
Exit Event/Label/Jump to Label/Comment) are now real and shipped. See
`!.OPEN-2do-events-db-networking-2026-08-28.md` for the real execution
record.

**Real question originally asked**: "what is next before we should
really be making individual games using events/palette... more palette
stuff will have to be done, till we can use the RPG Maker palettes for
demo. but also events, etc?"
**Real question asked**: "what is next before we should really be
making individual games using events/palette... more palette stuff
will have to be done, till we can use the RPG Maker palettes for demo.
but also events, etc?"

This is a grounded status check (real code/docs read, not guessed) of
everything still standing between today's real event-scripting work
and an actual playable RPG-Maker-style game. Sourced from a direct
codebase survey plus `EVENTS-PAL-BUILDOUT-PLAN.md` and
`COMMON-EVENTS-MANAGER-HANDOFF.md` (both in this same directory).

---

## What's solid today

Event **logic** is real and verified: Control Switches/Variables, Call
Common Event (including nesting), Conditional Branch, Autorun/Parallel
triggers with a name-based switch field. All compile to real
`prisc+x` VM bytecode, not shell stubs. See
`COMMON-EVENTS-MANAGER-HANDOFF.md` for full verification evidence.

## The three real gaps, in the order they block each other

### 1. Palettes / tilesets / map-authoring — 0% built for game tiles
"Palette" today (`&.widgits/palettes/`) is a UI color-theme/emoji
picker — no tile/tileset/chipset concept, no map grid, confirmed by
direct grep across every actively-worked directory (khtpm renderer,
db-hq, events-hq, palettes, tile-picker). `&.widgits/tile-picker/` is
real but places single emoji glyphs as live desktop GL windows, not
RPG-Maker-style tile chips on a map grid.

A real tileset implementation (`tileset.c`/`.h`) exists only inside
the disconnected prototype `201.rpg-maker-clone/` — not wired to
db-hq/events-hq or the desktop-tile/pal metaphor. **"Use RPG Maker
palettes for a demo" is not a polish step — it's a whole missing
subsystem** (map grid + tile chips + placement UI) that needs a fresh
design, not a port.

### 2. Event commands — the ones that make branches visible to a player
`EVENTS-PAL-BUILDOUT-PLAN.md`'s own staging table lists these as
explicitly NOT YET BUILT, separately from what shipped this session:
- **Message**: Show Text, Show Choices, Input Number, Select Item,
  Show Scrolling Text — flagged "UI-driven, no persistent state file,"
  a genuinely different implementation shape than the KV-int commands
  done so far (see gap #0 below — this is why it's blocked on more
  than just "write the command").
- **Character**: Transparency, Followers, Show Animation/Balloon,
  Erase Event — flagged "needs a real sprite/rendering layer per
  entity" (doesn't exist yet).
- ~19 more GET/SET_KV_INT-shaped commands still queued behind Control
  Switches as the template (mechanical, not blocked on new infra).

Switch/variable IDs are name-based only right now. The numeric
0-to-memory-limit ID scheme (your own stated design: "numbered from
0-memory limit but also addressable by name") is half-built — only the
name half exists.

### 3. Player/map runtime loop — does not exist in the real house code
No "walk a map, touch-trigger an event" loop exists anywhere under
`*.monads/`, `&.widgits/`, or `xyzfs/`. It exists only in two
disconnected prototypes:
- `201.rpg-maker-clone/` — real collision/touch-vs-action triggers,
  but its own `CRITIC_REPORT.md` rates it 6/10 play loop, 4/10 editor
  usability, explicitly stubs Show-Text/switch-name editing, and has
  no tileset atlas ("colored rectangles + single-character glyphs").
  Its own review calls it **needs a rebuild, not a port**.
- `300.rpg-xyz`/`300.rtp-xyz` — design docs only, no completion claim,
  framed as "a proving ground."

`EVENT_AI_VISION.md` (referenced in `EVENTS-PAL-BUILDOUT-PLAN.md`
Stage D+) already documents this as blocked: "Movement/Character/
Screen/Audio/Battle/Scene/System... all genuinely blocked on infra
that doesn't exist yet (tile/collision movement, a rendering/audio
layer, a battle system)."

---

## Gap #0 — the thing that blocks gap #2's message commands specifically

Before Show Text/Show Choices can even be built as real commands (not
just before they're polished), there's a prerequisite decision neither
plan doc resolves yet: **where does a message box actually render?**
The KV-int commands done so far (switches/variables/branches) have no
visible UI footprint — they're pure state. A message box is the first
command that needs an actual on-screen surface, and none of these are
decided:
- Does it render inside the entity's own live desktop-tile window
  (extending the khtpm renderer with a new overlay element type), or
  in a separate game-viewport window spawned per play session?
- Is it the SAME rendering surface the eventual map/tileset view (gap
  #1) will use, or a standalone thing built first and reconciled
  later? Building it twice would be wasted work — this needs a design
  decision before either Show Text or the map-view work starts, not
  after.
- Does pausing the desktop's own event loop while a message box is
  "waiting for player input" conflict with the Autorun/Parallel
  trigger cadence this session just built? (Parallel triggers keep
  polling every ~1s regardless of what's on screen today — a blocking
  message box may need to suspend that, which touches
  `common_events_manager.c`'s real tick loop.)

This is genuinely a design question, not a mechanical build — it
should get a real answer (probably from you, since it's a product
decision about how the desktop-tile metaphor and "a game" relate to
each other) before Stage 2's message commands or gap #1's tile system
get scoped in detail.

---

## Bottom line, in build order

1. **Decide gap #0** (message-box rendering surface + relationship to
   the Parallel-trigger tick loop) — a real product decision, needed
   before either of the next two can be scoped precisely.
2. **Design and build the tile/map system** (gap #1) — the long pole.
   Needs a fresh design; the disconnected prototype's tileset code is
   not a safe starting point per its own critic report.
3. **Build the message/choice commands + a player/collision runtime
   loop** (gaps #2 and #3) wired to the REAL common-events system this
   session built — not ported from `201.rpg-maker-clone`, whose own
   review calls for a rebuild.
4. **Numeric switch/variable IDs** alongside the existing name field,
   if RPG Maker parity there still matters once the above is real.

Event logic is genuinely ahead of everything else right now. The next
real work is design-heavy (gap #0, gap #1), not more mechanical command
building — more Control-Switch-shaped commands (~19 queued) can proceed
in parallel since they don't touch any of this.
