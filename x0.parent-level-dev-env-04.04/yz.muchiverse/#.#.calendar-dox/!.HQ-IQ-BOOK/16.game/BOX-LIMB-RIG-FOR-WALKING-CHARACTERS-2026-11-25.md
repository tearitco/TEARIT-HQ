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

## Extra parts and the RPG Maker converter (2026-11-25, owner priority 1: a Minecraft-style Asa, and any RPG Maker MV sprite the same way)
STATUS: built and unit-proved by harness `rig_convert` (51 checks; `bv_rig` still 35). NOT seen on screen: the installed `bv_render_3d.+x` was not rebuilt (the edited file compiles to /tmp only), so nothing here has been looked at in a live board.

### New RIG rows (all optional; an old file with only head/torso/arm/leg/face still draws exactly 6 boxes)
```
RIG | hair   | r g b     hair cap on the top 2/16 of the head + a slab behind the head (direction-aware, follows the pose)     +2 boxes
RIG | sleeve | r g b     the upper half of each arm, a hair wider than the arm                                              +2 boxes
RIG | shoe   | r g b     the bottom quarter of each leg                                                                     +2 boxes
```
Maximum 12 boxes. `bv_rig.h`: `bv_rig_boxes_ex(part_set, mask, direction, pattern, frame, out[12][6], ci[12])` returns the count and a colour index per box (0 head, 1 torso, 2 arm, 3 leg, 4 face, 5 hair, 6 sleeve, 7 shoe); mask bits 1 hair, 2 sleeve, 4 shoe come from `bv_rig_parse(path, col[8][3], &mask)` / `bv_rig_colors(..., &mask)` (the rows present). `bv_rig_boxes` is the old 6-box call. The renderer's rig block loops over the returned count. `bv_rig_op parts <d> <p> <f> <mask>`, `boxes ... <charset> <mask>` (prints `ciN`) and `parsefile <file>` expose it.

### Converter: `board-viewer/ops/rig_from_charset_op <charset.png> <char_index 0..7> <out_rig.pdl> [convert.pdl]`
Reads the down-facing standing frame (middle column, first row of the character's block). Normal sheet: 12x8 frames, 4 characters across and 2 down; frame = w/12 x h/8. A file name starting with `$`, or a sheet shaped 3:4 (like the generator-hq cast sheets), is ONE character (frame = w/3 x h/4) and the index is ignored. Settings are data in `board-viewer/data/rig_convert.pdl` (`CONVERT | key | value`: head/torso/leg fractions, hair, skin and eye bands, sleeve side fraction, shoe fraction, alpha and outline thresholds), found at `ops/+x/../../data` or given as the 4th argument. Colour = the mode of a 5-bit histogram over opaque, non-outline pixels (a region with nothing else falls back to the dark pixels, so a black suit is black). Exit 0 written, 2 usage, 3 no opaque pixel in the block, 4 PNG unreadable, 5 settings missing, 6 not written (written via `.tmp` + rename).
Test helper: `rig_sheet_make_op` makes the synthetic sheets of harness `rig_convert`.
Converted files live in `@.apps/rmmc/rigs/` (`rig_asa.pdl`, `rig_actor1_0.pdl`, `rig_actor2_1.pdl`, `rig_evil_0.pdl`); the lead copies the chosen one next to the desk as `rig_<name>.pdl`.

### What it cannot do
- Colours only: no textures, no face plate (a `face` row is the commonest non-skin eye-band colour and is still not drawn).
- ONE set of fractions for every sprite. RPG Maker figures are chibi and vary (head 45 to 65 percent of the height): Asa (cast sheet) converts well (tan head, red mohawk, black suit); Actor1/Actor2/Evil come out muddy (the face skin lands in the torso band, hair shadow in the head). Tune the fractions per sprite via the 4th argument.
- Two-tone clothing, capes, accessories and the sprite's own pose are lost; the sleeve is the torso's side-column colour (bare arms: skin arm, shirt-coloured sleeve).
- Asa has no RPG Maker sheet of her own: `rig_asa.pdl` was converted from the generator-hq cast sheet `#.NNEST_ASSETS/generator-hq/interim/cast/asa.png` (144x192, from the `CAST | asa` row of `@.apps/generator-hq/events/gen_cast/cast.pdl`); `tsots-characters` holds only a frame `.rgba`.

## Update 2026-11-25 (late): chibi-proof converter and the face (RAN, SAW live)

- **Profile mode** (`CONVERT | profile_mode | 1` in `board-viewer/data/rig_convert.pdl`, with the skin ratio keys): the face is the band of skin-hued pixels that holds the MOST skin pixels in the top 75% of the figure (the first band is wrong on sprites whose hair highlights are skin-hued: Harold), the head ends under that band's last row (the chin), torso and legs split the rest of the body by `body_torso_frac`, and skin-hued pixels never count as clothing. A file without those keys keeps the old fixed fractions, so the old harness checks still hold.
- **Harold (`Actor1.png` index 0, the hero of rpg-pet, RPG Maker default Actor 1):** head 249 193 157, hair 235 133 95, torso 91 26 40, sleeve/arm 64 72 67, leg 90 90 90, shoe 63 37 16, eyes 15 208 249 (RAN). Installed as `rmmc/rigs/rig_harold.pdl` and as Asa's rig (owner: "u can do with harold sheet"); the old cast-art Asa is `rigs/rig_asa_cast.pdl`.
- **The face:** a RIG row `eyes | r g b` turns on 7 more thin boxes on the FRONT of the head: sclera + iris per eye, brows in the hair colour, a mouth in the skin tone darkened 40%. `BV_RIG_MAX` is now 20 (a full rig is 19 boxes: 6 + hair cap and back 2 + sleeves 2 + shoes 2 + face 7); the scene holds 128 boxes, so about 6 full rigs at once. Colour slots 0..10 (8 iris, 9 white, 10 mouth are derived/added).
- **Harness:** `bv_rig` 46 PASS (new case `face`: for directions 2, 4, 6, 8 the 5 sclera/iris/mouth parts are all in front of the head; mask 7 = 12 parts, mask 0 = 6, mask 15 = 19; NEGATIVE: a header built with `BV_RIG_MUTANT_FACE_BEHIND` reads `in_front 0`); `rig_convert` 51 PASS (one expected value changed on purpose: the converted file has 8 rows and mask 15 because of the new `eyes` row).
- **SAW live** (`XO/31.Nov/nov-25/7.rig-face-live/`): Harold's rig on the island with blue eyes, white sclera, brows and a mouth, red hair cap and back slab. The camera in that shot is steep and close, so the body looks stretched: that is perspective.
- **Not done:** textures (the hair is a flat cap, not a fringe), `Actor1`-style sprites with a hood or helmet have no skin band and fall back to the fixed fractions, and the preview op (original sprite beside the rig) was not built (the live engine was used instead).
