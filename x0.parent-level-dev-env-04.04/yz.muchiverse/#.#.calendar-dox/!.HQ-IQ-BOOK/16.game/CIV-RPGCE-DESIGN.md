# civ-rpgce — design (2026-10-14)

Paper only. No window, no binary. Same pets formula as the other toys. The SNES game being copied is Civilization: settlers, huts, and founding a city. Not the later district games.

## What the owner asked

The settlers use the same party and switch as the pets. They are not drawn as the terumon or as Asa. Each settler is a viking (or another) sprite, recolored, taken from a different sheet so the units are not the same picture. They walk. They can enter a hut. They can start a city. Fog of war is per unit. Switching to a settler shows that settler's fog, not a shared map reveal.

## What already exists

- A pet is an actor. The party list is `party.pdl`. Movement is an events-hq page, the same walk used everywhere else. Do not invent a second movement model.
- Map tiles can be the OHRRPGCE sheets already on disk. The settler pictures come off those sheets too, starting with `320px-Vikings_tilemaps_000.bmp.png`. One sprite, recolored per settler, or a different cell from another sheet when a recolor is not enough. See `TCG-RPGCE-DESIGN.md` for the folder.
- There is an old desk named civ-test. Its actions are empty. This toy does not extend that desk and does not start from it.

## The map

One map of tiles. Each tile is land, water, or hut. A settler stands on one tile and wears its own viking color. The player controls one settler at a time. The others stay where they are until you switch. Switching is the same control change df-hq already uses for pet and player. It does not move the camera by a new rule. It changes which settler is the walker, and which fog table is drawn.

## Fog

Each settler has its own set of seen tiles, a list of x,y. When that settler steps, the tile it stands on and the eight around it are added to its list. Tiles not in the active settler's list are drawn as the fog color. Tiles in the list stay visible even after the settler leaves them. Another settler's seen tiles are not drawn while you are not that settler. So two settlers can have walked different roads, and swapping which one you control swaps which roads you can see. Units that are not the active settler are drawn only if their tile is in the active settler's seen list.

Fog is data on that settler, a file next to its `stats.pdl`. It is not a second map.

## Huts and cities

A hut is a tile with an event page, the same shape as a sign or a chest. The first time that settler enters it, the page grants one line of text. The page runs once per settler, so a second settler can still open the same hut.

Starting a city: the active settler stands on an empty land tile and the player picks Found. That settler leaves the party and a city marker is written on that tile: name, owner id, size 1. The first slice has no production, no food, no research. The city is a marker the other settlers can see if the tile is in their fog. Growing a city is a later page.

## What the first build is, after this page is accepted

1. One map, two settlers in different viking colors, switch which one walks.
2. Each settler's own seen-tile list, drawn only for the active settler.
3. One hut event and one Found that writes a city marker.

No palace, no units besides the settlers, no combat, no tech tree in that slice.
