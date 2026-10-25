# rpg-space vehicle entities (ship + land) and direct flight

Written 2026-10-25. Branch `claude`. Skill for driving it: `.claude/skills/rpg-space-board-driving/SKILL.md`. Older history of the slide-the-planet attempts: `HELM-MODES-HISTORY.md` (this folder).

## Owner direction (2026-10-24, kept verbatim)
- "WAS ALWAYS MEANT TO BE AN entity. even the land is meant to be its own entity. technically map should be just blank parallax."
- "nothing has to change for direct, just redirect arrow keys into a vehicle move relay and move the entity. yes the pets are passengers on the entity."
- "nothing should change size just cuz its piloted. the image changed completely for direct pilot. it shouldn't change at all."
- "when in doubt always default to what is more realistic."
- Chosen: ship fixed on screen, land scrolls; camera follows ship; popup Galaxy / Direct / No; planet unchanged.

## What exists (engine: `&.widgits/board-viewer/ops/bv_render_3d.c`, GPU path)
Vehicle pieces: any of `pieces/ship_01`, `pieces/land_0..8` with
- `state.txt`: `pos_x pos_y pos_z size_x size_y size_h`, optional `desk=<id>` (drawn only on that desk; `desk=none` = never drawn), `selflit=1` (no night multiply);
- `model.txt`: `dim X Y Z` then rows `lx ly lz r g b` (up to 4096 voxels, Y up to 32, Z up to 16 - shader depth was raised 8 -> 16).
Drawn as one model box each. Moving = editing `pos_x/pos_y`, no book rewrite.

## Direct flight as built (skin: `@.apps/rpg-space/ops/rpg_space.c`)
- `helm direct` (popup row 2): mode file `state/helm_mode.txt`=direct, flag_helm=1. The bridge desk, real hull, crew and camera are untouched.
- The baked planet is dropped from the desks (one reload) and the planet becomes nine land pieces (`land_0..8`, the page and its 8 neighbour pages, 16 x 16 voxel models, `desk=bridge`).
- `pieces/ship_01/state.txt` (`desk=none`) holds the ship's WORLD position; `state/direct.txt` is the same offset; arrows -> `helm move dx dy` -> both change, the land pieces are re-placed (`sk_land_write(0)`), no reload.
- `tuning.pdl`: `direct_max` (16), `land_z` (-9), `ship_alt`, `ship_start_x/y` (only used by the older small-hull version).
- Esc / `helm off`: land pieces removed, full rebuild (`sk_rebuild`) puts the planet back into the book.

## Evidence (live, harness `flight_run.sh`, 2026-10-25)
Popup with three choices; Direct click; real Right x6, Down x3: `direct.txt` and `ship_01` read 1 0 .. 6 0, 6 1 .. 6 3 after each key; frames differ from each other (harness rejects identical frames); 10 more moves to x=16, the next refused ("the edge of the map"). Video + frames + script: media archive `October-24/rpg-space-direct-flight-final-20261025/`, XO copy in `XO/30.rpg-dust-work/`.

## Known problems (not fixed)
1. After Esc the planet does not show again in the 3D view although the book and the minimap have it (cells.txt rewritten, minimap blue patches back). Cause unknown. In the PARKED view the planet outside the hull has never rendered in 3D (only the block under the hull); a stale-copy bug (`sk_fs3` made once at the full build) was fixed in `sk_build_ship` today but the picture did not change in my run - needs a look at the engine side (second solid run / extrusion of the sunk cells, camera level -7 after a reload).
2. After every book reload the engine puts the xelector/camera level at -7; raise it with real `x` keys (8) before a camera preset. `board_ctl camera` does this for the preset's level.
3. Map limit marker: not drawn (a wireframe box via `pieces/system/limit_box.txt` is read by the engine, the skin's writer was removed with the painted wall; not verified).
4. Pets/crew are not passengers on a moving entity: in this version the ship does not move on screen, the land does, so they are carried by construction. The owner's door-from-hull idea (hull = meeting spot, go through a door) is not built.
5. Galaxy mode is still the old space desk (book rewrite per move, ~5 s).
6. The helm only works with In: on (it releases itself otherwise). `helm direct` must not be sent while the popup flag is stale (it clears it).
7. The engine session/window/daemons died several times; one was a whole-machine reboot (2026-10-24 23:04, uptime 1 min), the others unexplained. `board_ctl recover` (checks for a running launch first) relaunches; window size is now 1296x789, use `BOARD_CTL_SIZE=1296x789`; the Direct popup row is at about (648,447).

## Files
Engine: `bv_render_3d.c` (vehicle pieces, limit_box), `bv_gpu_raymarch.{c,h}` (model depth 16, self-lit models). Skin: `rpg_space.c`, `tuning.pdl`, `events/helm/choice.pdl`. Harness: `&.widgits/board-viewer/tools/board_ctl.c`, `camera_presets.pdl`, `flight_run.sh` (in the media folder and here).
