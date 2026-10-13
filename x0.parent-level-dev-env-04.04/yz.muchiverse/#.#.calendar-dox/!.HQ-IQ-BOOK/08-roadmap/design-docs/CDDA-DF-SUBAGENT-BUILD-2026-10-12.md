# cdda-hq and df-hq: subagent build slices (2026-10-12)

Status: **PLAN ONLY.** This is the build handoff the earlier doc left open. Parent: `CDDA-HQ-AND-DF-HQ-HANDOFF-2026-10-12.md` (section 0: gta-hq is the later Toys goal; cdda-hq and df-hq may land first). Nothing in the slices below is built.

A subagent gets **one slice**, the checklist in that slice, and the house rules in section 1. It does not get "understand CDDA, then design the game." Research that is already done is in section 2. A number in that section was counted on 2026-10-12; re-count before treating it as still true.

## 1. House standard every slice inherits

Read before editing, do not skim a summary of these:

- `.claude/skills/khtpm-house-standards/SKILL.md` at the repo root.
- `02-architecture/CENTROID_GOLD_STD.md` and `02-architecture/PRISC-OPS-ARCHITECTURE.md`.
- `03-pitfalls/OPERATIONAL-LANDMINES.md` and `AGENTS.md` at the repo root.
- The launcher you are about to copy, in full: `@.apps/dsr-rpg/button.sh` and `@.apps/dsr-rpg/board/open_board.sh`.

Rules that have already been broken in this house:

- No new C in `khtpm_core_render.c`. No new `layout_*` branch, no new `g_is_<project>` flag. The window is a `.xhtpm` plus CSS. The shared renderer draws it.
- Game logic is a compiled C op under `ops/`, launched by the project's `.pal` or by `button.sh`, same shape as `dsr_rpg.c`. `.pal` is RISC-V (`exec`, `sleep`, `j`), not Tcl and not shell. Do not put shell scripts in `ops/` as the game.
- Copy the project. Do not symlink. Do not text-include another game's C unless two consumers need one pure function and the house rule for that include is already how dsr-rpg does it.
- Own names: binary, registry verbs, state directory, clock, pc-hq instance (`PCHQ_PKG`). Event pages must not exec rpg-pet or dsr-rpg.
- Commit on branch `grok` with an explicit pathspec. Never `git add -A`. Never `xyzfs/users`. Never commit another agent's staged files. The live checkout may be `claude`; do the commit from the `grok` worktree, not by switching the dirty live tree.
- Do not kill a running `khtpm_core_render.+x` without the owner. Drive a window through `#.desktop/entity_menu_history/<pid>.txt`, then read the app's state file. A screenshot is evidence only after the state file changed.
- Do not report done from a clean compile. Gate is: build, run beside dsr-rpg, one daemon, and a state file or frame that shows the slice's claim.
- Marker growth, not mtime. Do not publish a clock label faster than once per 5 seconds (`03-pitfalls/hq-cpu-safety.md` section 5).
- Copy the **current** `dsr_rpg.c`, not an older daemon. The control flag (`state/flag_ctl`) has flipped by itself; cause not proven. Do not "fix" arrows by reading `state/interact_relay.txt`. Keys for the board go to the engine session (`bv_h1` / `bv_h2`). Enter is read from that session's `pieces/keyboard/history.txt`.

Out of every slice: brixels, image concept banks, backtick-as-emoji, the backslash art cycle, gta-hq, and blueprintz editing. Those are section 0 of the parent doc. Do not start them while building cdda-hq.

## 2. What was read on 2026-10-12 (do not re-guess this)

**Copy source.** `@.apps/dsr-rpg/`. One C program `ops/dsr_rpg.c`, built by `ops/build_dsr_rpg.sh` to `ops/+x/dsr_rpg.+x`. Data: `rooms.pdl`, `party.pdl`, `catalog.pdl`, `time.pdl`, `tuning.pdl`, `voices.pdl`, `keybinds.pdl`. Events under `events/`. Window `dsr-rpg.xhtpm`. Board launcher `board/open_board.sh`. Own pc-hq data tree `dsr-rpg-pchq/`. Toys entry `toy.pdl` plus `@.apps/dsr-rpg-board/`. The recipe that already worked is commit `baf634b0e` (rpg-pet to dsr-rpg): copy tracked source only, not `state/`, not `+x`, not docs; rename the project in the C file, the xhtpm, `button.sh`, the pdl files, and event `cmd_*.sh`; new registry commands; one resident daemon.

**Tiles in dsr-rpg today.** `blit()` reads RPG Maker sheets, 48 px (`S48`). Autotiles are `draw_auto()` (kind times 48, plus a shape from 8 neighbours). 3D is an export into `pieces/system/maps/<id>/` on the pc-hq instance. `0` is already 2D/3D (`render_mode_toggle`, key 48 in `piececraft-hq/pieces/system/keybinds.pdl`). Backtick (key 96) is the 2D ASCII/Chinese view, not emoji mode.

**CDDA art on disk.** `#.NNEST_ASSETS/cdda-tilesets/gfx/UltimateCataclysm`. Counted 14,441 PNGs. CC-BY-SA 3.0, recorded in `44.xyz.01.00/shared/CDDA-ASSET-SOURCE-LOCATION.pdl`.

What is **not** there: `tile_config.json` and `tiles.png`. `tileset.txt` (10 lines) still names those packed files. They were not in the folder.

What **is** there:

- `tile_info.json`: 22 rows of **packed-sheet** sizes (`normal` 32x32, `tall` 32x64 with `sprite_offset_y` -32, `small` 20x20, `fallback.png`). Useful as the size table. The sheets it names are not in this tree.
- One JSON beside each sprite id, under `pngs_<bucket>_<w>x<h>/<kind>/<id>/<id>.json`, plus the PNGs that JSON names.
- Example, `pngs_normal_32x32/terrain/t_wall_wood/t_wall_wood.json`: `"multitile": true`, default fg `t_wall_wood_unconnected`, and `additional_tiles` whose ids are `center`, `corner`, `t_connection`, `edge`, `end_piece`. The fg names are the PNG basenames (`t_wall_wood_corner_nw`, `t_wall_wood_edge_ns`, `t_wall_wood_t_connection_n`, `t_wall_wood_end_piece_e`).
- Example, `t_grass.json`: fg is a weighted list of `t_grass_center`, `t_grass_center2`, `t_grass_center3`, `t_grass_center4`. Seasons are separate ids (`t_grass_season_summer`), not a renderer feature to invent.
- `layering.json` (139 lines) is **not** the global draw order. It is item-on-furniture variants (flags on a wall, flags on a flagpole) with a `layer` number. Do not treat it as "terrain then furniture then monster."

Draw order for the first port, until a later slice reads CDDA's real layer stack: terrain, then furniture, then the entity. Skip items, fields, and traps.

Connected terrain for the first port: if the id's JSON has `multitile: true`, pick the `additional_tiles` entry from the 4-neighbour mask of the **same terrain id**, using the suffix the JSON already lists. If the PNG for that suffix is missing, use the JSON's default `fg`. Do not port RPG Maker's 48-shape autotile table.

Index to build once, as data, not at frame time: one pdl or text row per sprite id, `id | png path | w | h | offset_x | offset_y`. Walk the `pngs_*` tree. 14k files is a cache, written by a one-shot C op. The game reads the cache.

**Dwarf Fortress today.** `44.xyz.01.00/201.dwarf-fortress/ARCHITECTURE.md`. Freeglut, one z-level, 48 by 48. Designations, jobs, stockpiles, BFS to an adjacent stand tile, saves as `saves/<name>/{meta,map,items,dwarves}.txt`. Jobs are rebuilt from designations on load. It is not a khtpm window. Do not link it. Copy the ideas into a house project after cdda-hq has z-levels. Bitlands was not found. df-hq does not pick a substitute tileset.

## 3. Slice A — index the tileset (one subagent)

Owns: a new one-shot op and its output file under a new folder `@.apps/cdda-hq/` is **not** required yet. Write the op and the cache beside the asset note, or under `44.xyz.01.00/shared/` if that is where `CDDA-ASSET-SOURCE-LOCATION.pdl` already lives. Do not create the game in this slice.

Does:

1. Walk `#.NNEST_ASSETS/cdda-tilesets/gfx/UltimateCataclysm/pngs_*`.
2. For each `<id>.json`, emit one row for the id and one row for every fg sprite basename that file names.
3. Record width and height from the directory name (`32x32`, `32x64`, `64x64`, `96x96`, `20x20`, `32x36`, `64x96`). Record offset from `tile_info.json` only when the bucket matches (tall offset_y -32, small offset_x 6). If the bucket is not in `tile_info.json`, offset 0,0 and say so in the row.
4. Print the row count and confirm it is in the same thousands as the PNG count. A missing output file is not a count of 0.

Does not: draw, copy dsr-rpg, or invent a multitile algorithm beyond copying the JSON's `additional_tiles` ids into the cache as extra columns.

Gate: the cache exists, `t_grass` and `t_wall_wood` rows resolve to real PNG paths, and `t_wall_wood` lists `unconnected`, `corner`, `edge`, `t_connection`, `end_piece`.

## 4. Slice B — copy dsr-rpg to cdda-hq (one subagent, after or beside A, no shared files)

Owns only:

- `@.apps/cdda-hq/**` created by the copy
- `@.apps/cdda-hq-board/**` if dsr has a twin toy
- the new registry command rows that the copy recipe says must be unique
- `toy.pdl` inside the new app

Does not touch `@.apps/dsr-rpg/`, `@.apps/rpg-pet/`, `khtpm_core_render.c`, `bv_menu_input.c`, or `network_browser_manager.c`.

Steps are the `baf634b0e` recipe with these names: folder `cdda-hq`, binary `cdda_hq.+x`, verbs that do not collide with `dsr_rpg_verb` or `dsr_pet_says`, pc-hq instance `cdda-hq-pchq`, `PCHQ_PKG` set to that instance, `PCHQ_HOST_ID` unique. `pieces/registry` must exist or the HUD is black on black.

Art stays RPG Maker in this slice. The point is a second game that opens beside dsr-rpg.

Gate: both windows open, `ps` shows one `cdda_hq` daemon and the old dsr daemon still there, and a state file under `cdda-hq` changes when the new daemon is sent a verb. dsr-rpg's state file does not change.

## 5. Slice C — draw one room with CDDA ids (one subagent, after A and B)

Owns: `cdda-hq` ops and the pdl files inside `@.apps/cdda-hq/` that name tiles. Reads the slice A cache. Does not edit the cache format; if the format is wrong, stop and write that in the slice note.

First room only, one z, no scrolling yet. Replace `blit()`'s 48 px sheet assumption for this project with: cell size 32, sprite looked up by id, tall sprites drawn with their offset so they hang into the cell above. Mapping for this room, fixed in a pdl table the op reads (change the table, not the C, to retarget):

| dsr kind | CDDA id | notes |
|---|---|---|
| grass | `t_grass` | weighted center sprites from its JSON |
| floor | `t_floor` | confirm the id is in the cache; if absent, stop and name the nearest `t_floor*` row, do not guess a PNG |
| wall | `t_wall_wood` | multitile from its JSON |
| door | `t_door_c` or the cache's closed wood door | confirm before coding |
| water | `t_water_dp` if present, else stop | same rule |

Leave trees, sand, ocean, path, and furniture on the RPG Maker path until this one room is proven. A mixed room is the goal of this slice, not a full reskin.

Gate: a state or frame of that room where a wall corner uses `t_wall_wood_corner_*` and a grass cell uses a `t_grass_center*` PNG. Neighbour a wall with a different id and show it falls back to `unconnected`.

## 6. Slice D — one map, two z levels (one subagent, after C)

This is the engine change. Do it only after C's gate is met. Do not also port the city.

Model, small on purpose:

- Two layers, `z=0` outside and `z=1` one floor, same x,y size as the slice C room.
- A cell is terrain id, furniture id, flags. Stored as text, one file per z, under the cdda-hq data dir.
- Stairs are a terrain id that changes the mover's z (`t_stairs_up` / `t_stairs_down` if the cache has them; if not, a house id `stair_up` that still uses a visible CDDA sprite you name in the report).
- The camera shows the selected entity's z. `0` still toggles 2D/3D. Render the current z only.
- Pathing stays the existing per-room walk, plus a stair edge. No viewport scroll in this slice.
- Read dsr's existing state field `z=` and write down what it already means before storing a second meaning. If it is already the floor, use it. Do not add a parallel z.

Out: roofs that hide, cellars, chunk files, the full island, NPC schedules.

Gate: a character walks across z=0, steps on the stair, and the state file's z changes to 1, in 2D and after a `0` toggle to 3D. The 3D export path may stay one floor if the pc-hq map format has no z; say that explicitly and keep 2D correct rather than inventing a second 3D format in this slice.

## 7. Slice E — df-hq (one subagent, after D, and only if bitlands is still blocked)

Do not start this in parallel with D. Same copy recipe from cdda-hq (so z-levels already exist), new names `df-hq`, `df_hq.+x`, own instance.

From `201.dwarf-fortress/ARCHITECTURE.md`, bring across as data, not by compiling freeglut in:

- A 48 by 48 map, one z at first (D already has two; use z=0 only until a stair is needed).
- Designation and job rows: dig, cut, stockpile, build. Idempotent per cell and kind.
- A few movers. BFS has a budget per tick (the sandbox doc already says the per-room BFS will not carry hundreds of movers).
- Save shape like `meta` / `map` / `items` / `units` text files. Jobs rebuild from designations on load.

Art: RPG Maker or the CDDA ids already wired in C, labeled temporary in the slice note. **No bitlands substitute.** If the owner names a path, a later slice swaps the ids.

Do not edit `201.dwarf-fortress/` except to read it. Leave that program runnable as it is.

Gate: one designation painted on a cell becomes a job row, one mover walks to the adjacent stand tile, the map file shows the dug or built cell, and the freeglut binary was not launched and not modified.

## 8. Order and what a parent agent must not parallelize

A then B may run together only because they write different trees. C waits on both. D waits on C. E waits on D. Two subagents must not edit `@.apps/cdda-hq/ops/` at once.

Each subagent writes its gate evidence (command, state path, row that matched) at the bottom of its own note under `08-roadmap/design-docs/` and commits that note plus its paths on `grok`. It does not mark a later slice done.
