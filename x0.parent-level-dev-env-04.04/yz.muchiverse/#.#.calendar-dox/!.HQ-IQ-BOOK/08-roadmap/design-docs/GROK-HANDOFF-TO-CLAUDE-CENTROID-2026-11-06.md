# Handoff to Claude, 2026-11-06: Centroid test page

From Grok, end of the 2026-11-06 centroid stretch. The owner asked for this so you can continue without re-deriving the session. The live checkout was `claude` the whole time. Commits were written on `grok` with a private `GIT_INDEX_FILE`. The owner then asked for a fast-forward of `main` and of `claude` onto that tip. Nothing was pushed.

House root: `/home/no/Desktop/github/work/NNEST-12.00/x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00`. Book paths below are under `#.#.calendar-dox/!.HQ-IQ-BOOK/`.

## 1. What the owner is building

Centroid University is one voxel campus on floating islands, imitating the owner's 2026-11-06 stone picture. One main island, bridges to two others, sky under the land and under the bridges. A door is Minecraft-like: the room is the voxels behind the door, not a teleport. The five-island editor town in `EDIT-ISLANDS-DESIGN.md` is a different map.

The editor book is `centroid`. Pages are `test` and `live`. `centroid-test` is a prebuilt campus. Pets do not raise the University in that run. Pets, a manager, hunger, planting, and beds are later (D40). Learning stays the existing phase table plus the Laplace weight in `harness_bank_op`. Do not add a new FSM.

First chunk named in the visual book, section 4: gate stair, front court, one tower, one roof over one room, a door, and one bridge to one smaller island. A removed bridge cell must leave the side island unreachable. The islands do not touch. The second bridge is the same span on another side.

## 2. What is built and was run

Observed on 2026-11-06, scratch harness or the live test page as noted.

- Book `centroid`, pages `test` and `live`, opened in rpg-edit.
- Sheet toggle is backslash (key 92). Key 0 (`render_mode` 48) lands on RPG Maker tiles (`view_2d_style=tiles`). Forward slash is not bound. Do not bind key 47.
- The test page is a tapered island. The maker third number is depth hung below the floor (the owner called this the drop-down floor). `1,0,0` is a floor with no body, so the sky shows under it. `0,0,0` with the z offset in use is air.
- A 5×5 room, wall slot 36, one opening. The door is a 0.1-thick slab (x+0.05 to x+0.95, map-y+0.45 to +0.55, height 1.0 to 4.0), event `8 10 132 75 49 door 0 2 1`. The picture is the office-page `door_civ` glyph, frame `door_0_2_1.rgba`. Cell 8,10 is floor. Cell 7,10 stays wall 36. `door_flip` moves the event and the bit, not the rock. Different rocks are later. Leave slot 36.
- A flat roof cap: 25 events `roof`, boxes y 4.0 to 4.35, over the room at origin 6,6. It is a cap with an overhang.
- One south bridge. Map is width 32, height 33. Main land is y 0..23. y 24..26 is air except x=6 and x=7, which are `1,0,0`. y 27..32 is an 8×6 isle (x origin 3) with its own taper. `split_ok` flood-fills kind 1 and fails if it reaches y under 24. Print `SPLIT ok`.
- Corner minimap on a painted maker page reads `g_mk` cells: no floor and no wall is sky, wall is brown, else grass. `minimap_margin`, `minimap_follow`, and a whole-desk mode stay off. A frame was checked at `/tmp/centroid-mm.png` (crop `/tmp/mm2.png`): border about 750,4–881,139, sky in the gap, two-cell bridge, smaller green island. Camera stayed put for that check.
- Top Harness tab. Left sidebar skips `harness` and `my-pals`. My Palettes stays on the left and in the top Palettes tab. My Pals is only under the Pals tab. The debug row at element 20 was removed (`n_tbhar` is not a leftover debug button).
- On page `test` the menu is seven rows: drop-down floor, wall, door, roof, bridge, all, clear. Keys are `floor`, `wall`, `door`, `roof`, `bridge`, `all`, `clear`. The xhtpm action passes `${tbhar.key}` so the label "drop-down floor" is not split on the space.
- `hpick` of those part keys, of `all`, and of the old `centroid/test` key runs `centroid_parts_op apply`. `apply all` stamps five times and prints `APPLY floor`, `APPLY wall`, `APPLY door`, `APPLY roof`, `APPLY bridge`. One named part also calls the full `stamp`, because the pieces share one desk. A click on a wiped page puts the wall, door, roof, taper, and bridge back, then appends one byte to `cells.marker`.
- `clear` still wipes the open page to 16×16 through `centroid_clear_op`. Do not run it unless the owner asks.
- Parts harness: `centroid/parts.pal`, verdict `PASS passed=29 failed=0` on 2026-11-06, including the five `APPLY` lines. The README row says 29.
- After the owner clicked the old `centroid/test` row, the live desk was 16×16 and `events.txt` was empty. That click was broken: `centroid/test.pal` only runs `centroid_prebuild_op check` on `court.pdl` and never writes cells. `hpick all` was then run on the live page. Observed after that: `width=32`, `height=33`, wall cells present, one door event, 25 roof events, bridge rows with two `1,0,0` cells in `0,0,0` air, `door.txt` `bit=0`. Those three files hash-match the `grok` blobs (`cells.txt` `03b45797…`, `events.txt` `2c61e4c1…`, `door.txt` `5b554782…`).
- Camera after the earlier safe nudge, and still after the stamp: `selector_x=7`, `selector_y=19`, `cam_yaw=160`. The interact relay for session `1793960181-3682232` was 0 bytes. `pal/main_module.pal` was pid 4018612. Do not restore an older camera. The owner sets it by hand. A restart of `main_module.pal` that drains a non-empty relay moves the camera. Restart the loop only if it died and the relay is empty. Key 126 is unbound and only bumps the screen.

`grok` tip before this handoff file: `79faddefd` "scoped feature: harness All runs the shown centroid parts". The centroid commits on `grok` that are not on the old `claude` tip `ebd3d9307` start at `39f9b543b` (39 commits). `xyzfs/users` is not in that range.

## 3. What I did not do

- Gate stair. Not started.
- One tower. Not started.
- Second bridge. Not started.
- Pets gathering, crafting, hunger, beds, or a manager distilling the blueprint. Design only (D38, D40).
- A new learning FSM. Not added, on purpose.
- A studio sheet. `cells-gen.rgba` is still an R/B swap of the Islands RPG Maker atlas. `0.good/` stays empty. No diffusion draft.
- Repaint of `map.png`. It is still the old 32×24 paint. The cell file is 33 rows.
- Walk collision as its own check. Occupied cells are the only block.
- A camera move to see the door face-on. From yaw 160 the slab reads edge-on. Leave it.
- `publish_harness()` in `_.monads/_.livedesk-taskbar/ops/palettes_manager.c`. It still hardcodes four centroid rows and was not rebuilt. The menu that works is `pal_side list` writing `board/state/pal.txt`.
- Killing or reopening the board. The khtpm that was open (chrome pid 3682305, started Fri Nov 6 02:16, argv ending in `pchq-board.xhtpm`) parsed the xhtpm once. It will not show the new Harness labels, or pass `tbhar.key`, until that window is opened again. `pal.txt` already has the seven rows. Do not kill it to force a reload unless the owner asks.
- Sidebar Harness. Do not add it.
- Harold. There is no such control. Do not invent one.
- Speech-dispatcher. Do not kill it.
- `/tmp/grok-merge-main`, a merge to the old main, or a previous camera restore.
- Committing `pal.txt`, `cells.marker`, or anything under `xyzfs/users`. The live cells and events match the tree already. They were not a new commit in `79faddefd`.

`centroid/test.pal` is still the court check. The menu does not exec it for the visible rows. `centroid/all.pal` still runs the book, parts, and test case pals if something calls that pal. The menu key `all` is a different path (`apply all`).

Session copies of `bv_render_3d.c` and `bv_menu_input.c` under the session tree are stale. The session binary is what runs. Renderer recipe, when you next change it: `gcc -Wall -Wextra -O3 -march=native -funroll-loops -DBV_HAVE_GPU -fopenmp -o /tmp/bv_render_3d.+x ops/bv_render_3d.c ops/bv_gpu_raymarch.c -lm -lEGL -lGLESv2`, then copy to `board-viewer/ops/+x/` and the session `ops/+x/`. Do that through `cpu_gate`. `--stop-engines` is after the label and before `--`, and it stops only this house's `bv_render_3d.+x` by exact pid. `cpu_gate .` fails. The house argument is the absolute `44.xyz.01.00` path.

Installed this session, through `cpu_gate`, exit 0: `pal_side.+x` (one pre-existing misleading-indent warning at line 78) and `centroid_parts_op.+x`.

## 4. What to do next

1. Read this file and `16.game/CENTROID-VISUAL-BOOK-2026-11-06.md` sections 4 and 4b. The owner reopens the board when they want the new Harness labels. Do not kill the open window for them.
2. Confirm the pane still shows the room, the cap, the sky gap, and the one bridge. Nudge with key 126 only. If the pal loop is dead, restart it only with an empty relay.
3. Gate stair, then one tower, then the second bridge as the same span on another side. Each one is a part row plus a harness expect, same shape as floor/wall/door/roof/bridge. A removed bridge cell must fail the split check.
4. Leave slot 36 until there is a real second rock. Leave the camera. Leave clear alone.
5. `publish_harness` should follow the open page the way `pal_side` does, or it should stop publishing a second list. One authority. Log the choice if you pick a shape.
6. `map.png` should be regenerated from the 33-row cells when you next touch the paint.
7. Pets and the manager stay behind the prebuilt campus until the owner asks.

## 5. Processes and files to leave up

Do not `pkill -f`. Do not kill a shared `khtpm_core_render`. Observed together with the stamp: button.sh pid 3682232, renderer `./system/renderer` pid 3682268, parser pid 3682269, pal loop pid 4018612. Chrome pid 3682305 was the board window from 02:16. Re-check pids before you trust this list. They go stale.

State file: `&.widgits/board-viewer/pieces/sessions/1793960181-3682232/pieces/system/bv_state.txt`. Overlay 886×641 RGBA at `pieces/display/rgb_frame_3d_overlay.raw`.

## 6. External references

- This file: `#.#.calendar-dox/!.HQ-IQ-BOOK/08-roadmap/design-docs/GROK-HANDOFF-TO-CLAUDE-CENTROID-2026-11-06.md`
- Owner copy: `/home/no/Desktop/github/work/XO/31.Nov/nov-6/0.grok-handoff-to-claude-centroid-2026-11-06.md`
- Visual book: `#.#.calendar-dox/!.HQ-IQ-BOOK/16.game/CENTROID-VISUAL-BOOK-2026-11-06.md`
- Test and live note: `#.#.calendar-dox/!.HQ-IQ-BOOK/16.game/CENTROID-TEST-AND-LIVE-2026-11-06.md`
- Decisions: `#.#.calendar-dox/!.HQ-IQ-BOOK/02-architecture/DESIGN-CHOICE-TEST-AND-DECISION-LOG.md`
- Parts op: `44.xyz.01.00/@.apps/centroid/ops/centroid_parts_op.c`
- Menu: `44.xyz.01.00/@.apps/rpg-edit-hq/ops/pal_side.c` and `44.xyz.01.00/@.apps/rpg-edit-hq/board/pchq-board.xhtpm`
- Harness: `44.xyz.01.00/&.widgits/_shared-lib/harness/centroid/parts.pal`, `cases/centroid_parts.pdl`, verdict `results/centroid_parts.txt.verdict.txt`
- Live page: `44.xyz.01.00/@.apps/centroid/centroid-pchq/pieces/system/maps/centroid/test/`
- Earlier handoff, still the roadmap and lore: `/home/no/Desktop/github/work/XO/31.Nov/nov-5/0.grok-handoff-final-2026-11-05.md`
- Owner picture this session used: the stone multi-story campus attached in chat on 2026-11-06. It is not a file in the repo.
