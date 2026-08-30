# Real default level/map fixtures (2026-08-30)

Per `CURSWORD-DESKTOP-3D-AND-PIECECRAFT-INSCENE-DESKS-DESIGN.md` §7.3
("there should be a default in the piececraft-hq copy folder... called
default-legacy & default-pdl"). Both are the SAME real default map
content - `chunk_0_0`'s 32 z-level terrain grids + `world_01`'s
animals/phymoji_entities/state - just in two different formats.

- **`default-legacy/`** - verbatim copy of the current, existing
  piececraft storage shape (`chunks/chunk_0_0/chunk_0_0_z<N>.txt`,
  `world_01/{animals,phymoji_entities,state}.txt`). No conversion, no
  format changes - this is what "File → default" loading in the OLD
  shape would read.

- **`default-pdl/`** - the real §6 hybrid format: `default.pdl` (a
  `BOARD` manifest row: name, path, cols=16, rows=16, z_min=0, z_max=31,
  thumbnail emoji) pointing at `desks/boards/default/`, which holds the
  same real chunk/entity files, just relocated under that directory
  per §6's own convention.

Neither is wired into the game's own File/Desk menus yet - these are
real, on-disk fixtures only, per §8 step 1. Wiring real nav rows
(`board_viewer.chtpm`) to read/offer these is §8 step 2, not started.
