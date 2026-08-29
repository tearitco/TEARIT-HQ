# Maps / Tiles / Z-Levels — where the real docs live (2026-08-29)

Written because this vision had drifted across disparate docs with no
cross-reference between them. This is the map of the maps — read it
first, then go to the doc that actually covers what you're touching.
Nothing in this file is itself the spec; it routes you to the real one.

## 1. Read `TILE-SYSTEM-DESIGN.md` FIRST — it's the real, primary,
in-progress doc for single-tile placement

(2026-08-27, same directory.) Answers the core question — **a placed
tile is a real `tp_desktop_window_rgb.c` entity, spawned on the
existing 80px desktop grid, not a separate map-canvas renderer.** Real
code already built and verified against it:

- `*.monads/*.livedesk-taskbar/ops/tile_autotile.c` — RPG Maker's real
  48/16/4-row autotile quadrant tables, ported and pixel-verified
  against real `World_A2.png` assets.
- `*.monads/*.livedesk-taskbar/ops/tile_registry.c` +
  `&.widgits/palettes/tilesets/tileset_registry.pdl` — a real,
  working multi-tileset registry.
- `desk_grid.pdl` support in `tp_desktop_window_rgb.c` — the 80px grid
  constant is now a real runtime value, not hardcoded.
- `publish_rmmv()` in `palettes_manager.c` — real, in-progress manager-
  side wiring for the RMMV tile category.

Its own §6 has the real, concrete next-step list (currently: wire the
autotile neighbor→shape-index mapping, wire the armed-brush "click
desktop to place" interaction, build the map-file loader). Start there
for anything about placing/rendering/autotiling a single tile.

## 2. `#.DOX/drag-drop-how2.md` — the real 2D→3D bridge precedent

Real, working, X11 XDND drag-drop: a desktop entity window dragged
into Mutaclysm's `gl_mirror` window, with a real `pet_import` trigger
on drop. **One-directional today** (desktop → Mutaclysm only); the
reverse direction and tile-specific (not just pet) transfer are real,
flagged, not-yet-built extensions — see `TILE-SYSTEM-DESIGN.md` §4a
for the real hook-point design task this implies.

`101.drag-drop-test=ON🀄️/` is the real, working **test harness** for
this same mechanism (reusable ops: `dd_drag_drop`, `dd_check_import`,
etc.) — useful for verifying changes to it, not a second spec.

## 3. `@.apps/piececraft-xyz/PIECECRAFT_XYZ_DESIGN.md` — the real spec
for multi-tile chunked maps and Z-levels

Covers ground `TILE-SYSTEM-DESIGN.md` does not: a whole chunked voxel
world (per-Z-level flat grid files, chunk compression/decompression
for distant terrain, deterministic seeded terrain gen, crafting/
survival/mobs). Has a real, already-answered open-questions section
(§10) and a real phase order (§11). Status: Phase 0 (board-viewer's
own terrain-legend data-driven retrofit) is done; Phases 1-5 (world
storage, place/break, compression, crafting, mobs) are real plan only,
nothing built.

Reuses, not reinvents: `xelector-context.md`'s real cursor pattern
(`101.mutaclsym…/dox/`) for block-targeting, and board-viewer's own
real, shared, data-driven rendering (`terrain_legend.txt`/
`ops_bank.txt`).

## 4. What's genuinely new, said for the first time in tonight's
conversation, and not yet in any of the docs above

Two real integration points, real open questions, not yet designed:

- **Cursword driving Z-level navigation and map-view control,
  xelector-style.** `PIECECRAFT_XYZ_DESIGN.md` §3 flags "vertical
  selector movement — needs a key from you" as unanswered; this
  session's conversation answered it with a different actor than that
  doc assumed (cursword itself, not just a keypress) — a real, new
  link between `CURSWORD-SOUL-VISION.md` and `PIECECRAFT_XYZ_DESIGN.md`
  that neither doc currently makes.
- **Extending the real `drag-drop-how2.md` mechanism to piececraft-xyz
  specifically** (today it only covers desktop↔Mutaclysm), plus a real
  answer on tile-sized vs. map-sized drag payloads (dropping one tile
  vs. dropping a whole map — genuinely different payload shapes).

Neither of these has a real design pass yet — they're real, confirmed
gaps, not implementation work you can start today.

## 5. How the RPG-Maker-events gap analysis relates

`GAME-READINESS-GAP-ANALYSIS-2026-08-27.md`'s own gap #1 ("tile/map
authoring — 0% built") is retired as of this doc — see that file's own
top-of-file note. It's not a separate design problem; it's
`TILE-SYSTEM-DESIGN.md` (single tiles, the near-term unblocking need
for "a player can touch-trigger an event") plus, later,
`PIECECRAFT_XYZ_DESIGN.md` Phase 1-2 (a whole walkable world) if/when
that's wanted instead of/in addition to single-tile placement.
