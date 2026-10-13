# Los Vientos: playability from what already runs (2026-10-12)

The city on the gta-hq board is Los Vientos. Room id stays `city`. This note is for the next agent who adds play, not a new renderer. Read it, then `BRIXELS-DESIGN.md` and section 0 of `CDDA-HQ-AND-DF-HQ-HANDOFF-2026-10-12.md`. Append a dated note when a fact changes. Do not erase the lines above the note.

## 1. Max leverage

Use these. Do not replace them with a new game loop, a new map format, or a new 3D mesher.

| Already running | Where | What the next slice should do with it |
| --- | --- | --- |
| Board plus the gta daemon | `@.apps/gta-hq/ops/gta_hq.c`, board session under `board-viewer/pieces/sessions/` | One verb or one data row. Rebuild only with `sh ops/build_gta_hq.sh` from `@.apps/gta-hq`. Restart only the `gta_hq.+x` whose args are the house and the app. |
| INT, arrows, Enter possess | same daemon as rpg-pet: INT on, arrows step the chosen person, Enter on the xelector possesses them | Prove a walk on Los Vientos before adding a new control. |
| Doors | `rooms.pdl` `DOOR` rows. City doors go to `castle`, `bank`, `store`, `school`, `hotel` and back | The first playable loop is: walk to a door, arrive in that interior, leave. Room ids stay. Display names are The Tower, The Vault, The Corner, The Lot, The Motel. |
| Events | events-hq and `.pal`. blueprintz B1 is a read-only tree | A mission is one events-hq page, not a new quest format. |
| Art | `state/flag_art.txt`. Empty means `brixel`. On the gta board, `\` writes `92` to the gta interact relay and cycles brixel, then RPG Maker, then CDDA, and stays on the painted scene | Other books still flip scene and tiles. Do not change that. |
| 2D and 3D | Key `0` toggles. 3D is the maker atlas: floor slot is the ground picture, a non-zero wall slot extrudes two levels and shows that picture | People and buildings already use this. A new standing thing is a picture in the wall slot, not a new mesh. `bv_render_3d.c` reloads when `cells.rgba` mtime changes. Do not restore camera values. |
| People | `shared/brixels/<id>-front.txt`, 32 by 48. Voss, Nico, Lucia, Vic, Rio, Tess, Kane | A new person is another file plus a `party.pdl` row. The loader is `blit_figure`. |
| City buildings | Five `World_B` scenery rows, 2 by 2 tiles, painted in order: tower, vault, shop, lot, motel. Trees are `Outside_B` | A new building face is a paint in that order, or a new scenery row. It is not an RPG Maker sheet. |
| Look at it | Key `p` writes `/tmp/entity-menu-frame.png` from the 2D frame. `state/scene.png` is the daemon picture | Tell the owner to look. A clean compile is not evidence. |

Work stays on branch `grok` in the grok worktree. The live checkout is often `claude` and dirty. Do not switch it. Do not `git add` anything under `xyzfs/users`. Do not commit `+x` binaries, `scene.raw`, `scene.png`, or flag files.

## 2. What Los Vientos is tonight

Verified on the board picture after commit `c6eee9c80`:

- Green island, blue water, five buildings with roof, windows, and a door, two trees, the clay car, and seven standing people.
- The car is large on purpose of the data, not because of a CDDA tile. `brixels.pdl` says the frame is 8 by 4. Each unit is drawn at half a map tile (`T / 2`). On this overmap `T` is 32, so the chassis is about 4 tiles by 2. A building is 2 by 2. A person is one tile. The owner asked; leave that size until they ask to change it.
- 3D map glyphs for the city: `B` buildings, `t` trees, `P` people. The GPU height is the wall slot (two levels), not a walked-around model of each window. `extrusion.pdl` records `B` 4, `t` 2, `P` 3 for the file. The raymarcher maker path still raises a wall whenever the wall slot is non-zero.
- Sockets in `brixels.pdl` are not checked when something moves. The car is not drivable.

## 3. First play slices, in order

Each slice is done only when the owner can do it on the open board.

1. **Walk the city.** INT on, arrows move Voss or a chosen crew member across Los Vientos, including up to a building. Do not add a new movement model.
2. **Enter and leave.** Step on a city door and arrive in that interior. Step on the interior door and return to the matching street cell. The five `DOOR` rows already name the trips.
3. **One spoken beat.** Reuse `pet_says` or an events-hq page so the person inside the building says one line. No new dialogue system.
4. **Sit in the car.** Reuse a door-style move onto the driver socket. The picture stays the current car. Do not scale it and do not write vehicle physics.
5. **One clock beat.** Reuse the existing day phase (dawn, day, dusk, night) so the street picture changes. The phase tint is already in the renderer.

Stop after the slice you were asked for. Crew sprites, building faces, and extrusion are already in. Do not redraw them as part of a play slice.

## 4. Names

| Room id | Label | City scenery |
| --- | --- | --- |
| city | Los Vientos | the overmap |
| castle | The Tower | World_B at 9,2 |
| bank | The Vault | World_B at 4,5 |
| store | The Corner | World_B at 13,5 |
| school | The Lot | World_B at 4,8 |
| hotel | The Motel | World_B at 13,8 |

Player: Voss. Crew, in `party.pdl` order: Nico (green jacket), Lucia (red, long hair), Vic (cap), Rio (blonde), Tess (yellow jacket), Kane (beard, brown coat).
