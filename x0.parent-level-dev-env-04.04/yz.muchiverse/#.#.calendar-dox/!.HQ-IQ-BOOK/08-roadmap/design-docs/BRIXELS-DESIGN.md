# brixels: the in-house tile vocabulary for gta-hq (design, 2026-10-12)

Status: **DESIGN + a seed data file.** Nothing reads it yet. Written by claude for the owner's gta-hq goal (`CDDA-HQ-AND-DF-HQ-HANDOFF-2026-10-12.md` section 0), to be filled in by Grok. Facts marked *verified* were read from disk today; the rest is proposal. Seed data: `44.xyz.01.00/shared/brixels.pdl`. Companion tileset files made the same day: `shared/cdda-tiles.pdl`, `shared/gta-tiles.pdl`, `shared/emoji-gta.pdl`.

## 1. What the owner said (verbatim source: handoff doc section 0)
Brixels are the in-house art, not a downloaded tileset: "evolving shape-clay used as Lego-like pieces". We write our own vocabulary for our own 2D/3D tile sets. An **image concept bank** (images associated by pixel maps) says what a piece looks like and what it does. Pieces **socket together**: arms + legs + head = a human; wheels + engine + windshield = a car. It is the third entry of the backslash cycle (RPG Maker tiles, CDDA tiles, **brixels**), and the 2D/3D toggle (`0`) must work in it. **No brixels folder existed** before today's seed (*verified*); do not substitute UltiCa or emoji for this mode.

## 2. Why gta-hq needs it (verified against the CDDA tileset, 2026-10-12)
`shared/gta-tiles.pdl` maps 37 GTA roles to CDDA ids that exist, and lists 7 `GAP` rows of things the loose UltiCa tree does **not** provide by name: no car frame, engine or roof (`vp_frame*`, `vp_engine*`, `vp_roof*` have 0 rows in `cdda-tiles.pdl`), no base person sprite (only clothing/mutation/effect overlays under `pngs_human_body_*`), no crosswalk/curb/road-marking ids. UltiCa does have door, windshield, seat, trunk, headlight, cargo and wheel parts (`vp_door_full_left`, `vp_windshield_full_face[NESW]`, `vp_seat_leather_left`, `vp_trunk`, `vp_headlight_ne/nw`, `vp_cargo_space`, `vp_wheel`). That is the gap brixels fills: **the missing car body and the person are exactly "pieces that socket together"**. The emoji side (`shared/emoji-gta.pdl`): of 31 GTA roles, 12 have a 3D voxel model in `&.widgits/board-viewer/pieces/registry/emoji_assets/` (25 models exist in total, 16 voxels per side) and 19 do not, including the car (1F697).

## 3. Proposed model (Grok: confirm or replace)
- **Brixel** = one piece with a size in voxels (same 16-grid as the emoji voxel models, `voxels_16.csv`), a kind (body, wheel, glass, limb ...), a 2D pixel map and a 3D voxel set derived from the same shape. One shape, two views: the 2D sprite is the top-down projection of the voxels, so `0` (2D/3D) never needs a second asset.
- **Socket / plug** = typed connectors with a position and facing. A piece plugs only into a socket of the same `type` (`wheel_mount`, `door_hinge`, `neck`, `shoulder`...). Typed sockets are what make "wheels + engine + windshield = a car" checkable instead of free-form.
- **Assembly** = a recipe (`ASSEMBLY` rows): which pieces on which sockets, with a `role` (car, person). An assembled thing is one entity for the engine (one mover), with parts that can have their own state (door open, windshield broken).
- **Concept bank** = per piece: an image (a path, or a CDDA id `cdda:vp_door_full_left`, or `emoji:1F9CD`) associated with the piece by a **pixel map** (which pixels of the image belong to which part of the piece), plus one sentence of what the piece **does**. "Does" is a hook into events-hq/blueprintz (section 4), not code in the engine.
- **Evolving** ("shape-clay"): a piece's voxels can change (damage, growth, upgrades) while its sockets stay. Proposed rule: sockets are the stable interface, voxels are the clay. How pieces evolve (by events? by the learner/RL systems in the notes?) is open.

## 4. Where it plugs into the rest (verified connections)
- **Engine (gta-hq):** a mover (car, person) is one entity whose draw routine walks its assembly. Today dsr-rpg draws every character from one RPG Maker frame (`blit`), and its 3D side is an export into `pieces/system/maps/<id>/`; a brixel draw path is a new tile source next to RPG Maker and CDDA (handoff section 3, P1 "tile source abstraction").
- **Events (blueprintz):** each piece's "does" sentence becomes an events-hq event on that piece (`event.ir.pdl`, compiled to `event.pal`), so a door piece has `door_open`, a windshield has `broken`. blueprintz shows them as the node graph (section 5 of `BLUEPRINTZ-DESIGN.md`).
- **Registry and 3D:** the existing emoji voxel registry already links three CDDA-named models (`t_grass`, `t_tree`, `t_water_dp`) to the 3D side; brixel voxels should live in the same registry shape so the 3D renderer needs no new reader.

## 5. First slices (each ends with a screenshot, per the evidence rule)
1. **Read the seed**: a tiny op that loads `brixels.pdl` and prints the car and person assemblies as a tree; check every plug type has a socket of that type. A one-off check script already ran on the seed (59 rows): every plug type has a socket of that type, every assembly row names a real piece and a real socket whose type matches the child's plug, and the car's socket coordinates all lie inside the 8x4x2 frame box; 0 errors, and a deliberately broken type was caught (negative test). That checks the file's consistency only, not that the shapes look like a car.
2. **Draw a car in 2D** from the CDDA parts that exist plus placeholder boxes for the missing frame/engine/roof (colour-coded by `kind`), in a gta-hq room.
3. **The same car in 3D** from the same pieces as voxels (`0` toggles).
4. **A person** from torso/head/arms/legs; walk it (INT arrows) so the rule "every cast member is playable" holds for brixel people too.
5. **Evolve**: change one piece's voxels at runtime (a dented door) without touching its sockets.

## 6. Open questions for Grok / the owner
1. Voxel grid: stay 16 per side (matches the emoji voxels), or finer for cars?
2. The pixel map format: a PNG mask beside the image, or rows in a `.pdl`? (The house prefers `.pdl`; a mask is a picture.)
3. Where do brixels live: `shared/` (one vocabulary for every game) or per game (`@.apps/gta-hq/brixels/`)? Proposed: shared seed, per-game overrides.
4. Who evolves pieces and when (events, learning, the player)?
5. Is the car body a CDDA sprite re-used, or fully brixel? (UltiCa has no body, so at least the frame is new work.)

## 7. Not verified / not done
The seed file's numbers (sizes, socket positions) are placeholders: consistency was checked (section 5, slice 1) but not that four wheels at those sockets look right under an 8x4 frame, or that a person's limbs line up. No code reads the seed. The cdda/emoji ids in the `CONCEPT` rows are real (each was looked up) but the `pixelmap` column is `(none yet)` for all of them.
