# Tile/Map System — real design starting point (2026-08-29)

Gap #1 from `GAME-READINESS-GAP-ANALYSIS-2026-08-27.md` — the actual
long pole before "a little game" is possible. Real events/commands
(gaps #0, #2) are now essentially complete as of tonight (Task 1 +
Flow Control both fully landed). This is the one that's left.

This is NOT a build - it's the real groundwork survey the gap analysis
itself called for before scoping detailed work, plus the one real
product decision that has to be made first.

## What already exists, confirmed real (not assumed)

- **`&.widgits/palettes/tilesets/tileset_registry.pdl`** — a real,
  working PDL registry mapping named tilesets (`outside`, `inside`,
  etc.) to real RMMV asset files (`rmmv/World_A2.png` etc.), already
  wired through `&.widgits/palettes/palettes-rmmv.chtpm` and a real
  `pallets.pdl` category. This is real infrastructure for *picking a
  tileset*, not yet for *placing tiles on a map*.
- **`&.widgits/db-hq/data/tilesets.pdl`** — db-hq's own database tab
  for tileset metadata (RPG Maker MV parity field, same shape as its
  other data tabs). Metadata only, no map-grid concept.
- **RMMV asset pipeline** — tonight's earlier `img_root` PDL-pointer
  refactor moved all real RMMV tile art to
  `#.NNEST_ASSETS/rmmv-www-img/`, with a compiled extractor already
  reading real tile atlases from it (confirmed working, used by
  palettes' own sprite rendering).
- **`201.rpg-maker-clone/`** — a real, separate, disconnected
  prototype with actual collision/touch-vs-action trigger logic and a
  tileset.c/.h implementation. Its OWN `CRITIC_REPORT.md` rates it
  6/10 play loop, 4/10 editor usability, "colored rectangles + single-
  character glyphs" (no real tileset atlas rendering despite having
  tileset code), and explicitly says **needs a rebuild, not a port**.
  Real, honest precedent to learn the shape from, not code to reuse.

## What does NOT exist anywhere in the real house code

- Any real map GRID data structure (rows/cols of tile references).
- Any real "walk a map, touch-trigger an event" runtime loop.
- Any rendering surface that shows a tile grid + a player sprite on
  top of it, moving.
- Any collision concept.

## The one real product decision that gates everything else

Where does a map actually render, and how does it relate to the
existing desktop-tile entity metaphor this whole house is built on?
Three real, distinct shapes, not a false trichotomy — pick one:

**Option A — a map is its own real desktop-tile window**, same family
as db-hq/events-hq/chat-hai (one more `g_is_map` mode in the shared
renderer, `khtpm_entity_menu_render.c`). A player sprite moves inside
that window via arrow keys/WASD; touching a tile with an event trigger
fires it the same way Autorun/Parallel triggers already do. Pro: reuses
the ENTIRE proven shared Elem/CSS/draw/input-relay stack this house
already has, zero new rendering technology. Con: tile movement needs
real per-frame position math this stack has never done before (every
existing window is static-until-clicked; this one needs a real game
loop).

**Option B — a map is a real, separate game-viewport process**,
outside the desktop-tile metaphor entirely — a dedicated "play the
game" window spawned by a real Play/Run action, closer to what
`201.rpg-maker-clone/` already attempts. Pro: doesn't need to fit the
existing static-UI assumptions the shared renderer's whole Elem/CSS
model was built around. Con: a second, real rendering technology
stack to build and maintain alongside the first; `201.rpg-maker-clone`'s
own critic report is the direct, honest warning about how much real
work that is.

**Option C — no live map rendering yet; a map EDITOR only** (place
tiles, save a real map data file, no player/movement/collision at
all) as the first real slice, deferring "walk around" to a second
pass. Pro: much smaller real scope, ships something real and testable
soon, matches this house's own "prove Shape 1 before Shape 2" delegation
discipline already used successfully for the event-command rollout
tonight. Con: doesn't get you "a little game" by itself — a Play mode
is still a separate, later real decision.

## Recommendation

**Option C first, then A** — matches how every other real subsystem in
this house got built this session (Scripting → Scratch visual editor →
now Flow Control, each a real, separately-shippable slice, not one big
bang). A real map EDITOR (grid of tile buttons, click to place from
the already-real tileset registry, save to a real map-data PDL file)
is buildable NOW with zero new rendering technology - it's a grid of
Elems, exactly like the Scratch block palette already is. "Walk the
map" (Option A, folded in once the editor's real data format exists)
is the real second slice, and only then does collision/touch-trigger
become a concrete, well-specified problem instead of a guess.

This recommendation is not a final answer - real user confirmation
needed before any implementation starts, this doc's whole point is to
have that conversation with real information instead of guessing.
