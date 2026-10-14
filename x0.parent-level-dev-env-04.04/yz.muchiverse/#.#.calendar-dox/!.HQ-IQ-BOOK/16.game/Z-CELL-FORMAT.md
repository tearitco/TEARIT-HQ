# Z-cell format (roadmap M1) - design v1, 2026-10-13

Status: **paper design plus a fixture and a parse check, both run (see section 7). Nothing in any game reads this yet.** Not yet reviewed by grok or the owner. Decisions behind it: `GAMES-DECISIONS-2026-10-13.md` sections 3-5. Files: `44.xyz.01.00/shared/zcell/` (`zcell-fixture.pdl`, `zcell_check.c`, `bad/`).

## 1. What it has to hold (SAID by the owner)
- Real z-levels: cellars, floors, stairs, and up to sky and orbit (planes, spaceships).
- Cell size flexible: fine near the ground, a region (1, 4, 16 ... tiles per side) at altitude or for a far-away large building. One standard, used with sense.
- Scale is a concept (a name in the concept bank), not a number baked into code.
- A vehicle is a grid of cells in the same format as a building; each cell holds a brixel part.
- Rotation: 90-degree steps now, room for free angles later.

## 2. The model
A **world** is one self-contained stack of grids: a building, a column (ground to orbit), or a vehicle. A world has a width and height; each **level** `z` is one `w x h` grid of single-character cells; a **legend** says what each character means. Everything is `.pdl` pipe rows, like `rooms.pdl` and `brixels.pdl`, so the house parsers and editors work on it.

| Row | Meaning |
|---|---|
| `ZWORLD \| id \| kind \| w \| h \| note` | kind = `building`, `column` or `vehicle` |
| `ZLEVEL \| world \| z \| span \| alt_m \| scale_concept \| label` | `span` = tiles per cell side, power of 2. `alt_m` = base altitude in metres (rises with z). `scale_concept` = a concept-bank id (M2 gives these real rows) |
| `ZLEGEND \| world \| char \| name \| flags` | flags: `walk solid stair air opaque seat`. In a vehicle, `name` is a brixel id from `shared/brixels.pdl` |
| `ZGRID \| world \| z \| y \| chars` | one row of exactly `w` characters; every level has rows `y = 0..h-1` |
| `ZLINK \| world \| z \| x \| y \| kind \| to_z \| to_x \| to_y` | stair/ladder. Both ends must be `stair` cells and the far end must link back |
| `ZREGION \| world \| z \| x \| y \| span \| ref_world` | this cell is the far stand-in for `ref_world` (a whole building seen from far as one cell) |
| `ZPLACE \| world \| vehicle_world \| z \| x \| y \| rot_deg` | a vehicle sits in a world. Rotation is stored in **degrees**; v1 accepts multiples of 90, so free angles later change the check, not the file |
| `ZSEAT \| vehicle_world \| x \| y \| role` | a seat to walk to. The cell must carry the `seat` flag. Engaging the driver seat is an event (CDDA-style), not part of this format |

## 3. Rules that follow
1. **z is an integer, contiguous, 0 = ground, negative = below.** Altitude `alt_m` must rise with z, and `span` never shrinks going up. That is how "fine near the ground, coarser above" is stated.
2. **Coarse levels are not special cells.** A level with span 4 is the same grid of characters; its cells each stand for 4x4 tiles. What is drawn inside is chosen by the detail-level renderer op (G2) from distance and the detail setting, not stored here.
3. **A vehicle's own grid always has span 1.** Placed in a coarser level, its footprint rounds up to whole cells: `ceil(side / span)`. The fixture checks a 3x2 car becomes 1x1 cells at span 4, and a rotated one swaps width and height.
4. **Large building from far:** `ZREGION` marks one cell of a coarse level as the stand-in for a finer world. The finer world is loaded only when the viewer is near; the region cell is what is simulated and drawn from far.
5. **Seamless climb vs scale switch (owner: both).** The climb is rule 1 (levels of one column get coarser). The scale switch (orbit to system map, pick a body, land) is not in this format: it is a move from one `column` world to another, owned by the flight milestone F3. This format only has to be able to describe both ends.
6. **Sharing:** one consumer reads it inline; the pure parse in `zcell_check.c` is a text-include candidate once two games read it (house convention; never header + link).

## 4. What a game reads from it
A game's room today is `ROOM` plus fixed tile sheets in `rooms.pdl` (`cdda-hq`) with doors as teleport events. A z world replaces a *building* room: its levels are the floors, and a `ZLINK` becomes the same kind of event a door is today (an events-hq page with a transfer node). That keeps one mechanism for moving between places, and means no game needs a new movement rule to adopt it.

## 5. Not decided (needs the owner or grok)
- What the cell *contains* beyond terrain (furniture, items, creatures) - proposed: other row types keyed by `world,z,x,y`, not more characters in the grid. Grok should say what CDDA needs.
- Whether very large grids stay in `.pdl` rows or move to a binary cache (the text form is the source, a cache is derived).
- Free-angle rotation details (left open on purpose).
- Whether `scale_concept` ids are enforced against the bank before M2 exists (v1 only checks that the field is present).

## 6. For grok to check
Does a CDDA building (cellar, floors, roof, stairs, ladders) and a DF dig-down level map onto `ZLEVEL/ZGRID/ZLINK` without loss? What does CDDA store per cell that this would force into extra rows? Say "fits" or name the gap.

## 7. Proof (observed 2026-10-13, built fresh)
`gcc` builds `zcell_check.c`; `zcell_check zcell-fixture.pdl ../brixels.pdl` printed `VALID: 3 worlds, 9 levels, 6 links, 0 errors, 0 warnings`, with per-z counts (house cellar/ground/first/second all 35 cells; ground 19 solid, 16 walkable, 2 stair, checked by hand) and both vehicle placements. **Negative tests:** eight bad copies in `bad/` each exit 1 with the intended message: short row, unknown legend char, missing reciprocal stair link, 45-degree rotation, seat on a non-seat cell, span not a power of 2, altitude that falls, vehicle footprint off the grid.

**Not verified:** no game loads the file; the fixture's car parts exist in `brixels.pdl` but their sizes are the seed's placeholders; the check is a C op run by hand, not yet a pal harness case; `scale_concept` names are not looked up anywhere; the stair cells are a convention of this fixture, not tested against any engine's walking code.

## 8. How to use / modify
- Check a file: `gcc -O2 -o zcell_check zcell_check.c && ./zcell_check yourfile.pdl ../brixels.pdl` (exit 0 valid, 1 errors, 2 cannot open).
- Add a floor: add a `ZLEVEL`, `h` `ZGRID` rows, and `ZLINK` rows at both stair ends.
- Add a vehicle: a `ZWORLD ... vehicle`, a legend of brixel ids, the grid, `ZSEAT`, and a `ZPLACE`.
- Add a new rule to the checker: add a bad copy in `bad/` first and watch it pass wrongly before you add the check.
