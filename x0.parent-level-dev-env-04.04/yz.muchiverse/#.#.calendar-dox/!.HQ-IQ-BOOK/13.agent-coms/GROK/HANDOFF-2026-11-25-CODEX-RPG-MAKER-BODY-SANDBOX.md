# Codex: a small voxel sandbox with an RPG Maker body

2026-11-25. For a new agent who has not seen this house. The owner watched today's Minecraft / RPG Maker result and called it disappointing. The picture he wanted is `/home/no/Videos/rmc.mp4` (93 seconds, 648x512, watched as frames every 6 seconds). Do the job in a **new rpg-edit book**. Leave the Earth moth book alone.

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

On 2026-11-25 the live window was set to the preset `iso-low` with `board_ctl camera` (mode 3, yaw 180, pitch -30, pan 16, 8, 36, z level 0). That was a field check. The reproduction of the video still wants key `2` once the charset body is the anchor.

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

1. Read `02-architecture/NO-GHOST-DATA-THE-LEDGER-IS-THE-AUTHORITY-2026-11-07.md`, `02-architecture/STANDARD-RULES-AS-OPS-FOUNDATION-DESIGN-2026-11-18.md`, and the Minecraft events handoff named above. Then read `game_new_op`'s header and make the book in a scratch house first.
2. Write the flat field with the four biomes and the dirt column. Prove it with a dump before you touch the character.
3. Put the charset frame on the player cell. Prove the pose changes.
4. Wire break and place through the existing voxel op onto the ledger. Prove the hole and the plank.
5. One harness case that fails if the pose does not change, and fails if a mined cell is still solid after replay.

## External references

- The expected picture: `/home/no/Videos/rmc.mp4`
- Today's planet, which is not this task: book `earth-moth`, page `earth`, op `@.apps/earth-moth/ops/earth_moth_op.c`
- Why the minimap was grey: `13.agent-coms/GROK/HANDOFF-2026-11-25-EARTH-MINIMAP-COLORS-AND-SOLAR.md`
- Voxel verbs already started: `16.game/MINECRAFT-EVENTS-HANDOFF-FARMING-WATER-MOBS-DAYNIGHT-2026-11-22.md`
- How a book opens in rpg-edit: `16.game/RTA-BATTERY-CONVERSION-2026-11-21.md` (Centon and Bay slice)
- Charset cache: `#.NNEST_ASSETS` RMMV character sheets, next to `rmmv-www-img/tilesets/World_A2.png`
