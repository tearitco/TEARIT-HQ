# egg-pals: cursor, tile picker, map/event editing, multi-project support

Exploratory design doc only - nothing here is built. Written to capture
and ground a direction before any of it gets scoped into real work, per
direct instruction: "lets update our design documentation to explain and
explore this first."

## 1. Context - what problem this solves

Right now egg-pals has exactly one hardcoded world
(`pieces/world_01/map_lobby/`), one fixed set of species/skills
registries, and no way to place things, script tile events, or start a
second, independent project. The direction being explored: make maps,
events, and whole projects into things the *user* creates/edits/saves/
loads through the game itself - modular, movable, editable - rather than
things only hand-authored once in source.

This is not a new problem in this project family. **1.TPMOS already has a
real, working answer**: `projects/op-ed/` ("OP-ED: The Sovereign RPG
Editor" - see its own `README.md`), a "Thin Engine RPG Maker" that does
exactly this: design worlds/pieces/events that are immediately playable
via the same PAL runtime. Matching this whole project family's own
practice (Moke-Pet's epoch loop, wraith's click dispatch, the xlector
active-target pattern - all ported, not reinvented), the plan below is to
**port OP-ED's concrete mechanisms into egg-pals's own conventions**, not
design a parallel editor from scratch.

## 2. The concrete OP-ED mechanisms to port

Read directly from `/home/no/Downloads/1.TPMOS_c_+rmmp.0100.0110/
projects/op-ed/` to ground each piece below in real, working code rather
than speculation:

- **"Sovereign Context" project model** (`README.md` §"The Sovereign
  Architecture"): each game is one self-contained folder under
  `projects/op-ed/games/<name>/` - maps, pieces, and scripts all live
  together, and "loading a game" is a context switch to that folder, not
  a single monolithic project file. Standardized on an existing file
  browser's Save-As/Load model rather than inventing new UI for it.

- **Maps are plain-text ASCII grids**, one file per map per z-level:
  `projects/<project>/maps/<map_id>_z<z>.txt` (real example:
  `projects/op-ed/maps/map_0001_z0.txt` - literally rows of `#`/`.`/
  letters, nothing fancier). This already matches egg-pals's own "if it's
  not in a file, it's a lie" convention exactly - no new file format
  philosophy needed, just a new file *kind*.

- **The picker/palette + placement op**
  (`projects/op-ed/manager/op-ed_manager.c`): a `glyph_palette_view`
  (a row of selectable glyphs the user cycles through with a key),
  an `emoji_mode` toggle (`pieces/emoji_mode.txt` - `0`/`1`, switches the
  active glyph set between plain ASCII and emoji), a tracked cursor
  position (`cursor_x`/`cursor_y`), and a single-purpose op,
  `pieces/apps/playrm/ops/src/place_tile.c`
  (`place_tile.+x <map_id> <x> <y> <tile_char>`), that does the actual
  write: opens the map file, replaces the character at `(x, y)`
  **UTF-8-safely** (walks variable-length UTF-8 sequences so a
  multi-byte emoji glyph doesn't corrupt the line - the same
  `decode_utf8`-class problem `system/emoji_gen_atlas.c` already solved
  once for this codebase), writes the file back, appends to a ledger,
  and touches a `frame_changed.txt`-style marker to signal a redraw.
  This is a *direct* template for a new egg-pals op, e.g.
  `ops/place_tile.c` - same shape as every other op here (self-contained,
  own `resolve_root()`, one verb).

- **Event editing via a dedicated PAL script builder window**
  (`projects/op-ed/layouts/pal_editor.chtpm` +
  `manager/pal_editor_module.c`): an "Event Builder" scoped to one piece
  + one event name, walking the user through picking ops/params rather
  than hand-writing `.asm`/`.pal` text, then writing out the resulting
  script - this is the same "op-ed-style guided builder" already
  name-checked (but not built) in `mutaclsym/dox/01-cdda-architecture.md`
  §4's FSM section. **egg-pals does not adopt `.chtpm` markup** (a
  convention already established for `compose_menu.c` - hand-printed
  text instead, matching TPMOS's *visual layout* without its markup
  engine) - the event-builder window would need its own hand-rolled
  equivalent of `pal_editor.chtpm`'s layout, not the file itself.

- **Z-level maps are already on OP-ED's own roadmap, not yet built even
  there** (`README.md` Phase 3: "Z-Level Editing: Full support for
  multi-layered maps") - worth noting because egg-pals *already has* a
  working z-axis (pet altitude, `ops/tick_pets.c`'s `ground`/`flying`/
  `digging`/`aquatic` bands from Step 6). Whether a pet's z-band should
  eventually mean "which map z-layer file it's standing on" is a real,
  open question worth exploring when this is actually up next - not
  decided here.

## 3. How the pieces map onto egg-pals's own roadmap

This isn't a separate roadmap - it's detail for where these slot into
`dox/01-architecture.md`'s existing numbered list:

- **The cursor** (already roadmap item 5, the xlector active-target
  pattern) is the same object used for two jobs: selecting a pet/item to
  act on it (already planned), *and* selecting a map cell to paint it
  while in an editor mode. One piece, one active-target mechanism, two
  modes - not two cursors.
- **The picker/palette window** is a new window class, closely related to
  (and probably sequenced alongside) item 7's "folder/storage windows" -
  both are "a window that shows many small things and lets you act on
  one," just a palette of placeable glyphs/tiles instead of a directory's
  contents.
- **The event-builder window** is the concrete mechanism item 3 (tile
  interaction / the fuller per-piece FSM engine) and item 9 (battles) both
  end up needing once pieces get custom scripted reactions instead of
  hardcoded C-op behavior - this is *how* a user (or the game itself)
  would author one of those scripts without hand-writing `.pal`.
- **Multi-project save/load** likely means introducing a project-selector
  screen at the top-level menu that sets which `projects/<name>/` folder
  everything reads/writes against, rather than always hardcoding
  `pieces/world_01/`. Mechanically cheap groundwork already exists for
  this: every op already resolves its root via the `PRISC_PROJECT_ROOT`
  env var (`resolve_root()`, copied into every `ops/*.c` file) - a
  project switch is "point that somewhere else," not a new indirection
  layer.
- **Colored tiles** (not just emoji/ASCII glyphs) means a map cell needs
  at least two independent properties - a glyph *and* a background
  color - where OP-ED's format only tracks one character per cell. This
  is a real format extension beyond what OP-ED itself does, worth
  deciding concretely (e.g. a second parallel file, or a richer
  per-cell `glyph|color` encoding) only once maps are actually up next.

## 4. Also noted along the way

`#.wussup🥚️.txt` picked up a line about chat while this was being
explored: chat should open as "a little dialog box window at bottom of
users screen... more invisible even than wraith" - folding this into
`dox/01-architecture.md` roadmap item 4 (basic pet chat) as a UI
placement detail for whenever that item is actually up.

## 5. Explicitly not decided here

- Exact map file naming/location under egg-pals's own `pieces/` layout
  (OP-ED's `projects/<name>/maps/` vs. something under
  `pieces/world_01/` - egg-pals doesn't have an OP-ED-style `projects/`
  tree today).
- Whether the picker window and the event-builder window are one binary
  with modes or two separate ones (every other window in egg-pals so far
  is one binary, one job - default to that unless a real reason not to
  shows up).
- The z-layer-file / pet-altitude-band relationship noted in §2.
- Colored-tile file format, noted in §3.

All four are real questions, deliberately left open until whichever
roadmap item makes them concrete instead of guessed at now.
