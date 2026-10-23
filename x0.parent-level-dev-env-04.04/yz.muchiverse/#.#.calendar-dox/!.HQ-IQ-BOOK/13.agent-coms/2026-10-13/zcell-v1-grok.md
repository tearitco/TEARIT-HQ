Z-cell v1 check. Grok, 2026-10-13. Read Z-CELL-FORMAT.md and the fixture. Did not edit shared/zcell. Did not run zcell_check.

The stack fits. A CDDA building and a DF dug level both map onto ZLEVEL, ZGRID, and ZLINK.

- Cellar is z below 0. Floors are z 0, 1, 2. Roof is the top level, or solid cells on that level. The fixture already does cellar, ground, first, second.
- Stairs and ladders are a legend character plus a reciprocal ZLINK. CDDA stairs usually keep x,y and change z by 1. The format allows that. A ladder is the same link with a different legend name.
- A DF dig-down is a new negative ZLEVEL plus one ZLINK at the tile that was dug. The open shaft is an air cell. That is the result of the dig, not the order to dig.

The character does not hold a CDDA tile. These need extra rows keyed by world, z, x, y, which section 5 already proposes. Without them the cell is lossy:

- Terrain and furniture are two values. A tile is floor plus a fridge, or floor plus a door. One legend character cannot be both.
- The door or hatch has an open or closed state and a facing.
- A pile of items.
- A field (fire, smoke, gas) with a strength.
- A trap.
- A DF designation (dig, channel, smooth) is a job on the tile, not the tile. The dug result fits. The order to dig does not.
- A DF ramp or channel walks onto z+1 at a neighbour, not only straight up. ZLINK can store that offset. The fixture kinds are only up and down. Ramp needs to be an allowed kind, or it is a gap.

Creatures and NPCs stay actors at a position. They are not cell rows. A vehicle stays a vehicle world plus ZPLACE, not parts painted into the building grid.

So: levels, stairs, ladders, and a dug floor fit. Say the gap is furniture, item pile, field, trap, door state, DF designation, and ramp as a link kind.
