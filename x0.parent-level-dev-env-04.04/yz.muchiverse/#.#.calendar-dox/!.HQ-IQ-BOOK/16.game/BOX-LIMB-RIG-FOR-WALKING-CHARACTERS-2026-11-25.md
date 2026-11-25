# Box-limb rig for walking characters (2026-11-25)

Owner: "why isn't Harold animated like the Minecraft guy yet?" Reference: `XO/31.Nov/nov-25/3.rmc-expected/1-behind-the-body.jpg`, `2-face.jpg` (head, torso, two arms, two legs as separate boxes).

STATUS: code built and unit-proved by harness `bv_rig` (35 checks). The LIVE 3D view was NOT verified (needs a board relaunch; the engine only reaches an open board through a new session).

## What it is
An event row of `events.txt` (`x y r g b charset index direction pattern`, read by `maker_read_events` in `&.widgits/board-viewer/ops/bv_render_3d.c`) whose **charset name starts with `rig:`** (e.g. `rig:asa`) is drawn as 6 axis-aligned boxes instead of one extruded sprite / one coloured box. Every other row is untouched (`bv_rig_is` is false, the old `ADDBOX` runs).

Pure functions in `&.widgits/board-viewer/ops/bv_rig.h` (text-include, like `mk_side_place.h`; the harness compiles it alone through `ops/bv_rig_op.c`):
- `int bv_rig_boxes(int part_set, int direction, int pattern, int frame, float out[6][6])`: 0 boxes for a non-rig, else 6: head, torso, right arm, left arm, right leg, left leg (the FIGURE's right). Each box `{x0,y0,z0,x1,y1,z1}` in blocks, x/z relative to the cell centre, y up from the feet.
- `bv_rig_is(charset)`, `bv_rig_facing`, `bv_rig_parse(path, col)`, `bv_rig_colors(dir, charset, fallback, col)` (cached once a second).

Sizes are Minecraft's in 1/16: head 8x8x8, torso 8x12x4, arms 4x12x4, legs 4x12x4. 32/16 = 2 blocks, scaled by 0.9, so the idle figure is **1.8 blocks** tall.

## Row format
`x y r g b rig:asa index direction pattern` (9 fields; the 15-field form with a box is not used by a rig). r g b is the fallback colour for any part the RIG file does not name.
- direction: `2` south, `4` west, `6` east, `8` north (numpad as RPG Maker); `0` south, `1` west, `3` north are also read (2 is south in both readings).
- pattern: `0` idle; `1`, `2` walk (pose = (frame + (pattern==2)) & 1: legs one way, arms the other, the body bobs 1/16 on the second pose); `3` jump (whole body 4/16 up, legs together, arms raised beside the head); `4` mine / swing (right arm forward and up, two poses by frame); `5` place (both arms forward); any other value is idle.
- frame: the renderer uses the monotonic clock in 140 ms steps (`ms / 140`), so a walk is about 3.5 poses a second. The pattern field is the writer's state; the frame is not stored anywhere (no ghost data).

## RIG file
`rig_<name>.pdl` beside the desk (`rig:asa` reads `rig_asa.pdl` in the shown desk's folder, `g_mk.key`). Rows:
```
RIG | head  | 224 172 130
RIG | torso | 60 90 200
RIG | arm   | 224 172 130
RIG | leg   | 40 40 90
RIG | face  | 20 20 20
```
`face` is parsed but NOT drawn yet. A part with no row takes the event row's r g b. The file is re-read at most once a second.

## Limits (read before promising anything)
- **Boxes cannot rotate in this raymarcher.** A swing is an OFFSET of the limb box along the facing axis, stepped (two poses), not an angle. Arms "up" are the arm box moved up beside the head.
- The scene has `BV_GPU_MAX_BOX` = 128 boxes; ADDBOX drops silently past that. A rig costs 6, so about 20 rigs fit on a busy board. Raise the constant in `bv_gpu_raymarch.h` (owner decision, GPU buffer sizes) before many walkers.
- The idle frame is only redrawn when the board re-renders; a standing rig on an idle board shows its pose of the last frame.
- Not built: skins / textures on limbs, the face plate, mining crack stages, a sneak or sleep pose, a hand-held item box, any per-part animation file (the poses are code).

## Harness
`&.widgits/_shared-lib/harness/bv_rig.pal` + `cases/bv_rig.pdl`, op `board-viewer/ops/bv_rig_op.c`: 6 boxes always; idle height 1.8 +- 0.1; walk legs on opposite signs; periodic; jump lifts the torso; mine/place arm forward of the torso; facing north/east swaps the axes; a non-rig charset gives 0 boxes; renderer wiring greps; NEGATIVE: header compiled with `-DBV_RIG_MUTANT_LEGS_IN_PHASE` reads `pos` where the real one reads `neg`. Run: `/tmp/prisc bv_rig.pal` from the harness folder (README there).
