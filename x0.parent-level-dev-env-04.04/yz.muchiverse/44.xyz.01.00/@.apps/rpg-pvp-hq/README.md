# rpg-pvp-hq (started 2026-10-20)

City shooter for the pets (GoldenEye-style, AI and human controlled the same way). Design and plan: `#.#.calendar-dox/!.HQ-IQ-BOOK/16.game/RPG-PVP-HQ-DESIGN.md`.

## What this is now (2026-10-20, later)
A rename-copy of `@.apps/cdda-hq` (house recipe, same as mcl-hq / df-hq): the full window (INT mode, Book/Page, Shop, Menu, Help, Inventory, Craft, Body, Look, Player, game clock with day/night, pets in the footer, chat), the engine `ops/rpg_pvp_hq.c`, 21 event page packages in `events/`, `time.pdl`. Opened from the toys menu (`button.sh`). Its two event commands `rpg_pvp_hq_verb` and `rpg_pvp_pet_says` are registered in `#.ref/menu/event_commands.registry.pdl`. Events, S1 ledger and blueprintz: `BOOK 16.game/RPG-PVP-HQ-EVENTS.md`. The copy still carries cdda-hq's placeholder rules (one zed, wounds, bandage, cellar).

The city generator below is a separate op (`ops/rpg_pvp_city.c`, state in `state/city/`) with its own small viewer window `rpg-pvp-city.xhtpm`; it is not yet joined to the engine.

## What exists (step P1, first half)
`ops/rpg_pvp_city.c` - a seeded city generator and checker, shown in its own viewer window (template `rpg-pvp-city.xhtpm`, shared renderer, no renderer C). Window menu: City (new city small/medium/large/huge or the demo seed 7), Check, Weapons; the canvas is `state/scene.raw` (512x384) written by the op.

```
sh ops/build_rpg_pvp_city.sh
ops/+x/rpg_pvp_city.+x gen <seed> [w h]    # state/city.txt (map), state/city.pdl (rows), state/city.png
ops/+x/rpg_pvp_city.+x check               # re-reads the files, tests the invariants, exit 1 + reason on failure
ops/+x/rpg_pvp_city.+x status | weapons | view   # view = rewrite the window files (state/scene.raw, scene.receipt.txt, ui.txt)
RPGPVP_STATE=<dir> ...                   # use another state folder (scratch tests)
```
`tuning.pdl` holds the numbers as data: the WEAPON table is READ from `007-goldeye+01.00/src/main.c` (a starting point, not a balance decision) and the CITY row (block size, building size, loot, fill).

## Checked (2026-10-20, scratch folder XO/rpgpvp_t)
Same seed gives identical bytes (map, rows, png); 1,200 cities (seeds 1-300 x sizes 64x48, 96x64, 48x48, 160x120) all pass `check`; five hand-broken copies each fail with the right reason (door removed, door sealed, bad glyph, short row, loot in a wall). The first sweep failed 450 of 1,200: a partition wall could seal a building's door; fixed and re-swept.

## Not built yet
Walking, guns, hits, the pc-hq board export, AI, 3D, building: steps P1 (walking, board) to P8 of the design. The checks above were run by hand, not by a pal harness yet (house rule: harnesses are pal).
