# Codex: a small voxel sandbox with an RPG Maker body

2026-11-25. For a new agent who has not seen this house. The owner watched today's Minecraft / RPG Maker result and called it disappointing. The picture he wanted is `/home/no/Videos/rmc.mp4` (93 seconds, 648x512, watched as frames every 6 seconds). The book is already open. Do not start a second one.

## Start here

The live window is rpg-edit `0x1c00003`. World state is book `rmmc`, page `rmmc-0000`. Camera mode is `2`. The proof that the ray hits the field is `XO/31.Nov/nov-25/3.rmc-expected/9-pov2-field.png` (HUD `pos 32,32,19`). That frame is the handoff picture. It is not the video.

Do these, in this order, and dump after each one. Stop when a dump shows the thing. Do not rebuild the book, do not load Earth, do not press `f`, `r`, or `t`.

1. **The body is under the floor the camera stands on.** Events are drawn at `Y = 1 + lift`, and lift is the glyph's extrusion minus one (`bv_render_3d.c`, the `ADDBOX` after `g_mk.evlift`). Grass extrusion is 2, so the Actor1 box sits near Y 2. The chase camera's anchor is voxel layer 18 (`pos_z=35`, then `pos_z - floor_z - 1` with `floor_z=16`). The cyan rectangle in the proof is the xelector outline at that anchor, not the charset. `Actor1_0_2_1.rgba` is 1536 bytes at `#.NNEST_ASSETS/tsots-characters/frames/`, and the session binary contains the `#.NNEST_ASSETS/tsots-characters/frames/%s.rgba` lookup. Move the event box onto the same Y as the anchor, then dump. If the lookup works, the box becomes the blue tunic. If it stays a flat color, the walk from `g_mk.key` did not open the file. Say which.

2. **The broad floor is a dark slab.** `cells.txt` is `1,3` (grass top, dirt side) on the grass cells. The GPU frame does not show that on the field. The green strip is the trench and the raised block is the corner, both atlas extrusions. `maker_view=1` (key `7`) did not change the pixels. `rpg-edit-hq-pchq/pieces/system/terrain_legend.txt` colors `.` as dirt `110,70,40` and `,` as grass `90,170,60`. The desk glyphs were switched to those letters (grass `,`, trench `.`, sand `:`, water `~`, pillar `s`, tree `T`) and `pc_generate_chunk` rewrote `board_full`. The next dump was the same dark slab. Find which mesh that slab is before painting more slots. There is no snow color in the legend. The raised corner glyph is still `N`.

3. **`pc_generate_chunk` puts the xelector back at 8,8,19.** That cell looks off the near edge (HUD `pos 32,32,3` was a lie in one dump; the reset that was read from disk was `8,8,19`) and the picture becomes sky with a grass horizon. After every chunk rebuild, set `xelector_01/state.txt` to `pos_x=32`, `pos_y=32`, `pos_z=35` and `selector_x/y` to 32. Yaw 180 looks toward decreasing row, so the body has to sit with ground in front of it.

4. **The 3D daemon is a separate process.** It had died once, and dumps after that were the frozen sky. The one running now started fresh, so `cam_home.txt` is `0 0 0 0` and a file pan of 0 is a zero offset. An older daemon still had the Earth home `107 68 0 89` and subtracted it, which threw the eye off the map. Read `cam_home.txt` before you change pan. The session binary is `board-viewer/pieces/sessions/1795602987-684836/ops/+x/bv_render_3d.+x`. Editing the house source does nothing until that copy is rebuilt. Do not relaunch the window to do it.

5. **Break and place are not built.** Call `vox_op` from an event page onto the ledger. Do not add a branch in `game_core.c`. A desk edit is a projection, regenerated from the ledger. Do not hand-edit `board_full` to fake a hole.

6. **Done is still the four dumps in "What done is".** Do not narrate `9-pov2-field.png` as that. The bottom bar still says `Earth moth`. That label is the stamp set, not the book. The book label on the top bar says `RM sandbox`.

Leave the Earth moth book, Teario, and Draw Battle alone. Commit on your own branch, pathspec only. Do not push unless the owner asks.

## What the video is

A sandbox window titled `test`. The world is a Minecraft-shaped heightfield: grass on dirt, dirt cliffs, a tree, a stone pillar, a patch of snow, and a trench the player dug. The camera sits behind a character and can look around. A white wireframe cube marks the block in front of the character. The player walks, jumps, breaks a block, and places dirt and wooden planks.

The body is not a Minecraft Steve. It is an RPG Maker walking sprite: brown hair, blue tunic, white trousers, the classic charset, standing in the world and changing pose as it walks. Close up, the face is the sprite's face, pixelated, not a 3D head. The sprite is the body.

The on-screen help, in Japanese, is the control list. It is part of that demo, not a string to copy into LyfeDesk:

- Direction: walk
- A: jump
- X: break
- Y: place
- B: choose the block (the HUD shows 土 dirt, then 木の板 wooden planks)
- LB / RB and the right stick: turn the camera
- Start: back to the first place
- Back: switch the demo's own renderer between DLL and JS

Ignore the DLL / JS switch. That is the demo's engine, not ours. The product window is the rpg-edit board.

## What was built today, and why it missed

Book `earth-moth`, page `earth`, inside the already-open rpg-edit window `0x1c00003`. A 200 by 200 heightfield painted from ETOPO5 into a ball of radius 92. Ocean, grass, sand, snow, and a few trees are slots on that ball. The sides of every filled cell are stone, so the minimap averages grey. Walk is four stamp buttons: North, East, South, West. There is no jump, no break, no place, no wireframe cursor, and no RPG Maker body animating. Moon, Mars, and a solar page were added beside it. That work is a planet sketch. It is not the sandbox in the video.

| The video | Today's page earth |
|---|---|
| A field you can walk across, with hills and a dug trench | A sphere of the whole Earth |
| A few biomes side by side: grass, dirt cliff, snow, trees | Biomes are latitude bands on a ball |
| Dig a few blocks down and see dirt under the grass | One floor tile per cell. Digging was never a verb on this page |
| Place dirt and planks, with a wireframe on the target block | No place, no break, no cursor |
| An RPG Maker character, walking, as the body | A board token. The sprite sheet is not the body |
| Camera behind the body | The owner's camera, which you must not reset |

Flat is acceptable. A whole planet is the wrong size. A 16 by 16 room is also the wrong size. The field has to hold a few biomes and a short dig.

## The map to make

New book. Suggested id `rm-sandbox`, title `RM sandbox`. One page, `field`. Do not add it as a page of earth-moth. Do not change earth-moth's desks, the open window's `desk_id`, or the camera.

Size, as a starting point the owner can reject: **96 by 96 cells, 24 levels of depth**. That fits grass, a dirt hill, a sand strip, a snow corner, and a tree clump, and a person can dig a trench without falling through the world. If 96 is heavy on this machine, 64 by 64 by 16 is the floor. Do not go to 200, and do not make a sphere.

Surface shape: a heightfield is enough. Hills are a few blocks taller. Under the grass is dirt for several levels, then stone. Water is a low corner, not an ocean wrapped around a ball. One tree is a trunk plus a leaf block. One stone pillar is the video's pillar. Snow is a rectangle of cells, not a latitude test.

The ledger is the only authority for a broken or placed block. A desk file is the projection. Follow `02-architecture/NO-GHOST-DATA-THE-LEDGER-IS-THE-AUTHORITY-2026-11-07.md`. A rule is an op called from an event page, not a new branch in `game_core.c`. `vox_op` under the Minecraft-events work already has place, mine, drops, daylight, and sleep (`16.game/MINECRAFT-EVENTS-HANDOFF-FARMING-WATER-MOBS-DAYNIGHT-2026-11-22.md`, case `vox`). Call that, or a thinner sibling, from this book's event pages. Do not invent a second mine.

## POV keys (read 2026-11-25 from bv_menu_input.c)

The numbers are keys, and they are the only way to change the point of view. Do not invent a pitch. Do not press `f`, `r`, or `t` to "get back" to a picture. `f` resets the current mode. `r` and `t` change pitch inside a mode.

| Key | What it writes | What you see |
|---|---|---|
| `1` | `camera_mode=1`, `render_mode=1`, pitch 6, pan offsets cleared | First person. The eye is at the anchor, nudged forward so it is not inside the block. |
| `2` | `camera_mode=2`, same pitch 6, pan cleared | Third person. The eye sits behind and above the anchor. An extra downward tilt (`tp_look_down_deg`) is added so the look is at the body, not over its head. This is the angle of `/home/no/Videos/rmc.mp4`. |
| `3` | `camera_mode=3`, yaw 180, pitch -90, `cam_pan_y` = `current_z` | Free roam. Here `cam_pan_y` is a height, not a row. Named presets in `board-viewer/tools/camera_presets.pdl` are this mode: `iso-low` is pitch -30, `lowside` is pitch -12, `topdown` is pitch -89. `board_ctl camera <app dir> <preset>` writes one of those. |
| `4` | `camera_mode=4`, pitch -90, pan set to the selector cell | Bird's eye. Here `cam_pan_x` and `cam_pan_y` are map column and row, not a height. The open board was left in this mode from Earth, which is why page `rmmc-0000` first showed sky. |
| `0` | flips `render_mode` between 1 and 0 | `0` is the flat RPG Maker tile picture. Entering it forces `view_2d_style=tiles`. It does not pick a 3D angle. |

`7` turns on `maker_view` (RPG Maker tiles drawn in 3D) and does not change the angle. Tab is the side-scroll picture (`render_mode=2`), not one of 1–4.

The video is key `2`. A mode-3 preset can show the field from the side, and it is not the chase camera behind the charset.

On 2026-11-25 the live window was set to the preset `iso-low` with `board_ctl camera` (mode 3, yaw 180, pitch -30, pan 16, 8, 36, z level 0, and `current_z=25`). That preset is written for a tall rpg-space desk. On this field the ground is about height 2, so 25 is the sky.

Key `2` was then sent with `board_ctl engine-key`. It did what the key does: `camera_mode=2`, pitch 6, yaw 180, pan offsets 0, z level 0. It did not clear `current_z`, which stayed 25, and the dump was a blue sky (`/tmp/rmmc-pov2.png`). `current_z` was then set to 4 with `board_ctl camset`, mode left at 2. The next dump (`/tmp/rmmc-pov2b.png`, also `XO/31.Nov/nov-25/3.rmc-expected/8-pov2-still-sky.png`) is still sky. The HUD reads `rmmc:rmmc-0000`, pos 8,8,3, `pick voxel`, `ray miss`. The minimap shows the green and tan field. The third-person ray does not hit a column.

So key `2` is on, and the picture is not ready to present as the video. Still missing, in this order: the eye has to stand on a cell the ray can hit, the top face has to be a grass tile and the side a dirt tile (the video's cube, not one tile on every face), the body has to be the RPG Maker charset in front of that camera, and break / place are not verbs yet. Do not record a presentation of the sky frame.

Later the same night the 3D daemon (pid 1246366) had exited, so dumps after 05:28 were the frozen sky frame even after `bv_state.txt` changed. A one-shot `bv_render_3d` plus `bv_compose_frame` in session `1795602987-684836` painted a new frame. The xelector is at 32,32 with `pos_z=35`. On a painted desk the renderer rewrites that as `pos_z - floor_z - 1` (floor_z is 16), so the anchor is 18, which is the solid layer in `board_full_z18.txt`. HUD then reads `pos 32,32,19`, `z=19`, `poss hero_01`. The picture (`/tmp/rmmc-live.png`, copy `XO/31.Nov/nov-25/3.rmc-expected/9-pov2-field.png`) is mode 2 behind a cyan box: a grass strip, a raised grass platform, and a dark floor. `arrow_config.txt` for this host is `tp_distance=7`, `tp_height=9`, `tp_look_down_deg=48`, `use_gpu_render=1`. `cam_home.txt` still says `107 68 0 89` (the Earth camera). A long-lived renderer subtracts that home from the file values, which is why pan 0 put the eye off the map. A fresh process stores the file values as the new home and the first frame's offset is zero.

The cyan box is the event color `40 90 200`, not the charset. `Actor1_0_2_1.rgba` is on disk (1536 bytes) under `#.NNEST_ASSETS/tsots-characters/frames/`. The GPU scene still drew the flat box. `maker_view=1` (what key `7` writes) did not change the pixels of the next render. Break and place are still not verbs. Do not narrate this frame as the video.

Reference frames from the video, and the house dump that is not one of them, are in `XO/31.Nov/nov-25/3.rmc-expected/`.

## The body

Use an RPG Maker charset the house already has (the Actor sheets under the RMMV tileset cache, `#.NNEST_ASSETS`, the same family as `World_A2.png`). The character on the video is the standard blue-tunic walker: three frames of a walk cycle, four directions.

The body in the world is that sprite, drawn upright on the cell the player occupies, and the frame index advances while the walk verb is repeating. A still token that never changes frame is the bug to avoid. A Minecraft player skin is the other bug to avoid.

Do not build a new animation format. The charset is already a grid of poses. The walk op picks the column. The renderer billboards that cell. If the board cannot billboard a charset frame today, say so in the proof and show the frame you did get on screen. Do not silently substitute a cube.

## What "done" is

A `p` dump of the new book, opened and looked at, that shows all of these at once or in two dumps:

1. Grass, one other biome (sand or snow), and a tree or the stone pillar.
2. The RPG Maker body, and a second dump one walk-step later where the pose changed.
3. A hole one or two blocks deep, with dirt visible in the wall of the hole.
4. One placed wooden-plank cell, with the white wireframe on a cell before the place.

The dump is `dump_frame_png_op` of the rpg-edit window, then the PNG opened. A ledger row that says a block was mined, with an empty picture, is not done.

## Do not

- Do not edit camera equations, `bv_menu_input.c`, or keybinds. Do not press `f`, `r`, or `t`. The owner set the camera. Keys that already exist: `q`/`e` yaw, `c`/`v` height, `w`/`s`/`a`/`d` pan, `1`/`2`/`3`/`4`/`0` modes.
- Do not relaunch the board to prove the new book. The book appears the next time the editor opens, the same way `bay-slice` does: `@.apps/<book>/rooms.pdl`, a desk, a symlink in `rpg-edit-hq-pchq/pieces/system/maps/`. Switching `desk_id` on the live world while the owner is on Earth is a separate ask.
- Do not kill Teario, Draw Battle, or `prisc` by a broad `pkill`.
- Do not wrap `button.sh run` or `reset` in `cpu_gate`. Do compile through `cpu_gate`.
- Do not commit `xyzfs/users`, ETOPO, generated desks, or binaries. Branch is `grok` only if you are Grok. Codex uses Codex's own branch. Explicit pathspec. Never `git add -A`.
- No Python harness. Pal plus a case plus a C op. Read `&.widgits/_shared-lib/harness/README.md` first.
- Licenses on city caches are still not checked. This task does not need those cities.

## Order of work

The book, the page, and a mode-2 dump of the field already exist. Follow "Start here". The old order below is how this book was going to be born. Do not repeat step 1.

1. Read `02-architecture/NO-GHOST-DATA-THE-LEDGER-IS-THE-AUTHORITY-2026-11-07.md`, `02-architecture/STANDARD-RULES-AS-OPS-FOUNDATION-DESIGN-2026-11-18.md`, and the Minecraft events handoff named above. The book was made with `rmmc_field_op`, not `game_new_op`. Page id is `rmmc-0000`, not `field`.
2. The flat field is on disk. The dump of it is `9-pov2-field.png`. The grass legend glyph did not change that slab.
3. Put the charset on the camera's Y. Prove the pose.
4. Wire break and place through `vox_op` onto the ledger. Prove the hole and the plank.
5. One harness case that fails if the pose does not change, and fails if a mined cell is still solid after replay.

## External references

- The expected picture: `/home/no/Videos/rmc.mp4`
- Today's planet, which is not this task: book `earth-moth`, page `earth`, op `@.apps/earth-moth/ops/earth_moth_op.c`
- Why the minimap was grey: `13.agent-coms/GROK/HANDOFF-2026-11-25-EARTH-MINIMAP-COLORS-AND-SOLAR.md`
- Voxel verbs already started: `16.game/MINECRAFT-EVENTS-HANDOFF-FARMING-WATER-MOBS-DAYNIGHT-2026-11-22.md`
- How a book opens in rpg-edit: `16.game/RTA-BATTERY-CONVERSION-2026-11-21.md` (Centon and Bay slice)
- Charset cache: `#.NNEST_ASSETS` RMMV character sheets, next to `rmmv-www-img/tilesets/World_A2.png`

## rmmc-0001 island biomes (2026-11-25, claude)

The island page (rmmc-0001, Centroid small page at 3x, 216 x 156) now has taller mountains, two lakes, a desert, a sand beach, snow caps, six trees and a dirt trench, painted by a generic op from DATA rows. This replaces the magic-coordinate approach of `rmmc_field_op.c`.

- `@.apps/rmmc/biomes.pdl` is the only place the numbers live: `RULE` (seed, noise scale, snow line, rock tile), `TILE` (atlas slot, Mineclonia PNG, tint), `BIOME` (shape, centre, radii, flat or cone, base and peak height, top / side / cap tiles, jitter), `TREE`. Biomes apply in file order, cones only raise. A variant is a file that `INCLUDE`s the base and restates one row.
- `@.apps/rmmc/ops/rmmc_biome_op.c` (built to `ops/+x/`): `rmmc_biome_op biomes.pdl <desk>` repaints in place and keeps the island outline and underside depth; `check ...` reads a desk back (`key value` lines); `png <desk> out.png [px]` draws a top view. The header documents the format and the exit codes.
- Desk files written: `cells.txt` (floor = top tile, wall = side tile, depth), `map.txt` (glyph = cell height: `1`..`9`, `A`..`Z` = 10..35, `a b c` = -2 -1 0), `extrusion.pdl`, `legend.txt` (glyph and slot legend), `cells.rgba` (16 px Mineclonia atlas), `cells.marker` (size old mod 4000 + 1), `events.txt` empty.
- Rerun: `cp` the backup (`/tmp/claude-1000/rmmc-0001-before/`, or rebuild with `rmmc_scale_op <centroid small> <desk> 3`) over the desk, then `rmmc_biome_op biomes.pdl <desk>`. Harness: `harness/rmmc_biomes.pal` + `cases/rmmc_biomes.pdl` (scratch desk only).
- NOT verified: the live 3D view (no engine was started). The 3D side rule read from `bv_raymarch.frag`: top face = floor tile, sides above the floor level = wall tile, hung blocks = rock slot. Heights were reasoned from `bv_render_3d.c` (`zh[]`, `zs[]`), not seen in the engine. The block under the plain's floor level shows the floor (grass top) tile on its sides at the island edge.
