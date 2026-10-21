# Pet-hq must-haves

Survey of the pet boards (2026-10-21). One template. This note does not
change the camera, the keybind file the board viewer reads, or any
daemon. A later pass wires the gaps. Do not invent a fifth POV or put
an old camera back.

The board that already moves is piececraft
`pieces/system/keybinds.pdl` (the game-dev office uses that file).
`0` toggles 2D and 3D. `1` is first person, `2` third person, `3` free
roam, `4` bird's eye. `5` recentres. `pov_mode_6` is unbound (code
255). Those four names are the whole POV set.

## Why `0` and `1`–`4` do nothing on mcl-hq in INT

INT on mcl-hq does not drive the board camera.

The window writes `"<code> <ms>"` into the game folder
`state/interact_relay.txt` (and Esc/Enter into
`state/keyboard/history.txt`). `mcl_hq.c` reads that file. On `0` it
flips the app flag `view` between `2d` and `3d`. On `1` through `4` it
writes the app flag `pov` to that digit. The status line says the
picture is still sprint item 7. Nothing in that branch writes the
board session `render_mode` or `camera_mode`. The painted frame stays
`state/scene.raw` with `default_style | scene`.

The same stored-only branch is copied in `cdda_hq.c`, `df_hq.c`,
`gta_hq.c`, `rpg_pvp_hq.c`, `dsr_rpg.c`, `rpg_pet.c`, `tcg_rpgce.c`,
and `civ_rpgce.c`. Each file's header says "stored only: 3D is sprint
item 7".

A second gap sits in the app keybind tables. `mcl-hq/keybinds.pdl` (and
the same short file under cdda-hq, df-hq, gta-hq, rpg-pvp-hq, dsr-rpg,
rpg-pet, tcg-rpgce, and civ-rpgce) lists `pov_mode_1` and `pov_mode_4`
only. `pov_mode_2` and `pov_mode_3` are absent. `pet-trainer/keybinds.pdl`
and the piececraft board file list all four. The game-dev office was
not missing 1–4 in the board file. What is missing there is POV 6 (left
unbound on purpose) and any pet-daemon path, because that board is the
viewer itself. The copied pet tables are the ones that dropped 2 and 3.

## Must-haves

Every pet board uses the same chrome and the same camera contract.
Game rules stay in that game's daemon.

1. One footer. The pets (the party) live there. Do not stack a second
   bottom bar. Game acts are not footer buttons.
2. Left bar is acts for the open game only. Do not repeat the pet
   names there. Right bar is chat. Both bars have minimize. When a bar
   is minimized the map grows into that space. Both minimized means the
   map is the whole window.
3. Day and night come from the shared livedesk clock
   (`daylight.txt` phases), not a private timer.
4. Rooms, doors, and scenery are `ROOM` / `DOOR` / `SCENERY` rows.
   A door teleports in and out. The trigger cell is the floor in front
   of the door, not the door art itself.
5. The picture follows the pet who is playing. Opening a table, a
   match, or a vehicle swaps that same window onto that board. Leaving
   returns to the walk map. The footer stays the party.
6. INT number keys do the same job as the board viewer. `0` toggles
   2D and 3D on the board. `1`–`4` select the four POV modes named
   above, on the board, in the same session the viewer already uses.
   Writing `flag_pov` is not that. Arrows in INT still move the
   selected pet. Esc leaves INT.
7. People stay extruded sprites. A billboard flag is for height-2
   furniture (tables and stools): a transparent card that faces the
   camera, with no cube. Walls stay solid. Do not put the billboard
   path on people.
8. A building reads as a building. If the game's own tileset has no
   house picture, borrow one or two cells from RPG Maker MV, CDDA, and
   Dwarf Fortress so the blocks are not one repeated cobble. Trees
   stay trees.

## Survey (2026-10-21)

| Board | Footer / left acts | Doors | Clock | INT `0` and `1`–`4` move the board camera | Keybind rows for POV 2 and 3 |
| --- | --- | --- | --- | --- | --- |
| piececraft / game-dev office | board chrome, not a pet daemon | map cells | n/a | yes, viewer keys. POV 6 unbound | yes, in `pieces/system/keybinds.pdl` |
| mcl-hq | pet chrome | yes | yes | no, flags only | no |
| cdda-hq | pet chrome | yes | yes | no, flags only | no |
| df-hq | pet chrome | yes | yes | no, flags only | no |
| gta-hq | pet chrome | yes | yes | no, flags only | no |
| rpg-pvp-hq | pet chrome | yes | yes | no, flags only | no |
| dsr-rpg | pet chrome | yes | check per window | no, flags only | no (package copy of the board file does list 2 and 3) |
| rpg-pet | pet chrome | yes | check per window | no, flags only | no |
| tcg-rpgce | pet chrome | yes | check per window | no, flags only | no |
| civ-rpgce | pet chrome | yes | check per window | no, flags only | no |
| pet-trainer | trainer, not a city board | n/a | n/a | keys are listed; this file is the source the city tables claim to copy | yes |
| bet-rpg | one footer of pets, acts on the left. An older design line that put acts on the footer is out of date | yes, island | yes | not on the city daemon path yet | not surveyed as a second keybind file |

## What a later patch is allowed to do

Point the INT `0` and `1`–`4` handlers at the board session the viewer
already updates, and add the two missing `pov_mode_2` and `pov_mode_3`
rows so the city tables match pet-trainer. Leave `bv_render_3d.c`,
`bv_menu_input.c`, and `pieces/system/keybinds.pdl` alone unless the
owner asks for a camera change in that conversation. Do not restore a
previous camera position.
