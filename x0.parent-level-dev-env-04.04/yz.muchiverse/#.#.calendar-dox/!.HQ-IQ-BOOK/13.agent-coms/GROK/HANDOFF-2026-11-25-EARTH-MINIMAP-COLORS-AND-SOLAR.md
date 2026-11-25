# Handoff, 2026-11-25: earth minimap is grey, solar must show all three balls

From grok. The owner said: observe, explain, hand this back to Claude. Do not treat this file as the fix. Claude fixes the paint.

Labels: **READ** = read in the repo or seen in a dump this session. **SAID** = the owner said it. **DOC** = the conclusion from that.

## What the owner saw (SAID + READ)

The open window is rpg-edit, book Earth moth, page earth, window `0x1c00003`. Two `p` dumps (`/tmp/entity-menu-frame.png`, 2026-11-25) show the corner map as one grey disc on a blue square. No green, no ocean blue, on that disc. The 3D view of the same ball, once the camera was lifted above it, was also a grey shell. The owner asked why the earth minimap has no blue or green, and said the colors were probably not implemented right. **SAID:** solar must show all three spheres on the minimap. **SAID:** Claude fixes it.

## Why the disc is grey (READ)

The corner map does have a color path. It is not a flat grey fill for every cell.

`bv_render_3d.c` `bv_draw_minimap`, the comment at the tile-color block (about line 2798): a cell paints **the wall tile, else the floor tile**. The code is `wl ? wl : fl` (about line 2825). Empty cells (no floor and no wall) are painted `90,150,210`, which is the blue square around the disc, not the oceans.

Page earth writes every filled cell as `floor,stone,depth`. Floor is water, grass, sand, or snow. Wall is stone slot 6 (`default_stone.png`) on purpose, so the sides of the column read as the Minecraft interior. The top face in the raymarch shader uses the floor slot when floor and wall differ. The minimap does the opposite: it prefers the wall. Stone is grey. Grass and water never reach the disc.

So the colors are in `cells.txt` and in `cells.rgba`. The minimap reads the wrong slot for this kind of column.

## What Claude should change

One rule, matching the top face the shader already draws: when the floor slot is set, the minimap uses the floor color. The wall slot stays the side material for the 3D view. A cell with only a wall still uses the wall. Empty stays the blue square.

That is the earth disc (water, grass, sand, snow) and, on solar, the three surfaces: earth floor tiles, moon regolith (gravel, slot 8), mars dust (red sand, slot 9). All three still have wall slot 6, so the current rule paints three grey discs even after the desk is solar.

Proof: a `p` dump of page earth whose disc is not one grey, and a dump of page solar whose corner map contains three separate discs with those surface colors. Harness `minimap_fit` already owns `bv_minimap_tile_avg`. Add the floor-versus-wall choice next to it so a cell `2,6` averages the grass slot, not the stone slot.

## Solar is not on screen (READ)

`@.apps/rpg-edit-hq/rpg-edit-hq-pchq/pieces/world_01/state.txt` is still `map_id=earth-moth` and `desk_id=earth`. The renderer loads `maps/<map>/<desk>/` from those two fields (`maker_load_base`). The corner map cannot show the moon and mars until `desk_id` is `solar` and the minimap window includes the whole 256 page, not a follow crop around the person at 94,94.

The desks exist (generated, not in git):

| Page | width | what |
|---|---|---|
| earth | 200 | ETOPO ball, radius 92, floor = surface, wall = stone |
| backup | 72 | the older ring map, leave it |
| moon | 64 | radius 25, regolith on stone, chord only (no lunar height file on disk) |
| mars | 112 | radius 49, red sand on stone, chord only |
| solar | 256 | earth 22912 cells, moon 1961, mars 7525, person at 94,94 |

`CONFIRM_SET_DESK:solar` was appended to `rpg-edit-hq-pchq/pieces/system/widget_cmds/inbox.txt` and was not applied. The editor session pal is not running (`1795602987-684749` has the status manager, the clock, and the trigger watcher, and no `prisc` on `pal/main_module.pal`). `pc_menu_input` reads only the first inbox line and then truncates the file. That file's first lines were `MOVE`, so a later `CONFIRM_SET_DESK` would be deleted with them. Do not start a second board window. The board pal for session `1795602987-684836` is a different process. The 3D engine idle-exits when no frame is requested (`pieces/display/.engine_exit.log`).

## Already on origin/grok

Pushed `0b4e95039..187f4cb40`. The commits for this work are `4e78ca890` (ETOPO ball, stone sides, `earth_moth.pal`), `2f8a90a55` (`planet_ball.pal`, radii earth 92, moon 25, mars 49), `187f4cb40` (pages moon, mars, solar). Live checkout is `claude`. Do not commit the live `bv_render_3d.c` onto `claude` by sweeping the tree. Camera equations were not edited. Do not press `f` on the open board if the owner has aimed the camera. Moon and Mars have no height grid next to `ETOPO5.DAT`.
