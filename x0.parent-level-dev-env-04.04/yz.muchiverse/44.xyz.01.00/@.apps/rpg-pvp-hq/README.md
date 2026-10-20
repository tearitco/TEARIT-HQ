# rpg-pvp-hq (started 2026-10-20)

City shooter for the pets (GoldenEye-style, AI and human controlled the same way). Design and plan: `#.#.calendar-dox/!.HQ-IQ-BOOK/16.game/RPG-PVP-HQ-DESIGN.md`.

## What exists (step P1, first half)
`ops/rpg_pvp_hq.c` - a seeded city generator and checker, shown in a window: toys menu entry `rpg-pvp-hq` (`toy.pdl` + `button.sh`, template `rpg-pvp-hq.xhtpm`, shared renderer, no renderer C). Window menu: City (new city small/medium/large/huge or the demo seed 7), Check, Weapons; the canvas is `state/scene.raw` (512x384) written by the op.

```
sh ops/build_rpg_pvp_hq.sh
ops/+x/rpg_pvp_hq.+x gen <seed> [w h]    # state/city.txt (map), state/city.pdl (rows), state/city.png
ops/+x/rpg_pvp_hq.+x check               # re-reads the files, tests the invariants, exit 1 + reason on failure
ops/+x/rpg_pvp_hq.+x status | weapons | view   # view = rewrite the window files (state/scene.raw, scene.receipt.txt, ui.txt)
sh button.sh run                          # open the window (also: toys menu)
RPGPVP_STATE=<dir> ...                   # use another state folder (scratch tests)
```
`tuning.pdl` holds the numbers as data: the WEAPON table is READ from `007-goldeye+01.00/src/main.c` (a starting point, not a balance decision) and the CITY row (block size, building size, loot, fill).

## Checked (2026-10-20, scratch folder XO/rpgpvp_t)
Same seed gives identical bytes (map, rows, png); 1,200 cities (seeds 1-300 x sizes 64x48, 96x64, 48x48, 160x120) all pass `check`; five hand-broken copies each fail with the right reason (door removed, door sealed, bad glyph, short row, loot in a wall). The first sweep failed 450 of 1,200: a partition wall could seal a building's door; fixed and re-swept.

## Not built yet
Walking, guns, hits, the pc-hq board export, AI, 3D, building: steps P1 (walking, board) to P8 of the design. The checks above were run by hand, not by a pal harness yet (house rule: harnesses are pal).
