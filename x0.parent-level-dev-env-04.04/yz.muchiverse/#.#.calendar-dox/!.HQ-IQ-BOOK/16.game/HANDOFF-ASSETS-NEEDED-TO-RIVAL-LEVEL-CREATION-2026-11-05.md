# Handoff: what `good/` still needs so our level creation can rival RPG Maker (2026-11-05)

Labels: **SAID** owner; **OBS** measured or opened this session; **DOC** my reasoning or estimate (not run). Written after the owner said: "we still dont have enough in good to rival level creation". Read first: `GENERATOR-STATE-AND-NEXT-STEPS-2026-11-05.md` (sections 1 to 14), decision log D26 to D32, `generator-hq/good/APPROVED.md` (with its CORRECTION) and `good-new/APPROVED.md`.

## 1. Where we are (OBS, counted 2026-11-05)
| Group | Original RPG Maker files | In `good/` (drop-in) | Of those, genuinely new drawings |
|---|---|---|---|
| tilesets | 31 | 5 (Outside_A2, World_A2, Inside_A2, Dungeon_A2, Outside_B) | **0**: all five are recolours or ramp builds of the same drawings (owner: "we didn't want to just change the color") |
| characters | 26 | 4 (Actor1, Evil, Monster, People1) | 4 |
| faces | 10 | 1 (Actor1) | 1 |
| battlebacks1 / 2 | 13 / 11 | 1 / 0 | 0 (tone only) |
| parallaxes, sv_actors, sv_enemies, system, animations, titles | 9, 3, 22, 13, 11, 2 | 0 | 0 |
| **Total** | **151** | **11** | **5 (3%)** |
New art with no original, approved into `good-new/` (8 files): buildings, items, 3 cast sheets, 3 face sheets. In `interim/` and not yet `good`: our new Outside_B (60 cells, covers only part of the sheet, so not a safe same-name drop-in), A5 floors, voxel characters (small previews), 8 theme packs and ~90 recolour sheets (**not approvable**: same drawings repainted; the Eden pack was opened and is one green-yellow ramp).
**Honest verdict:** a level made today can use new people and new props, but its ground, walls, roofs, water, interiors and UI are still RPG Maker's. Not measured: which tiles the 98 imported maps really use (a grep for tileset ids found nothing, so the map format needs a proper reader).

## 2. What "rival level creation" needs (DOC)
A level author must be able to build an outdoor town, an indoor room, a dungeon and a world map without touching an RPG Maker pixel. That is, per theme (Outside, Inside, Dungeon, World): **A1** animated water, **A2** ground autotiles (48 shapes per block), **A3** building roofs/walls, **A4** wall autotiles, **A5** plain floors, **B/C/D/E** objects; plus **characters and faces** for a cast, **IconSet and window system** images for UI, **battlebacks and parallaxes**, **animations**, and a way to **try a sheet in a real level** before approving it.

## 3. Priority order (DOC; effort S/M/L are guesses, not measured)
| # | Work | Why first | Effort |
|---|---|---|---|
| 0 | **Map-use scan**: a small C op that reads the imported maps and counts which sheet and cell each map uses; output a ranked list | stops us drawing cells nobody places; the grep failed, so this needs the real map format | S |
| 1 | **Outside ground from scratch**: A2 blocks drawn from our own palette into the existing 48-shape table (`events/gen_restyle`), not repainted originals; then A1 water, A3 roofs, A4 walls, A5 floors | ground is most of every screen; the 48-shape verb is built, the art is not | L |
| 2 | **Rest of Outside_B and Outside_C**: plants, trees, snow items, tents, lamps, ladders (slice 3 of D31); the big windows and doors skipped in slice 2 | continue `gen_objects/outside_b.pdl` (60 cells done); finish the sheet so it can become a drop-in | M |
| 3 | **Inside, then Dungeon, then World** sets in the same order as 1 and 2 | interiors and dungeons are the other half of an RPG | L each |
| 4 | **People**: the remaining character sheets (26 originals, 4 done) and faces (10, 1 done), then sv_actors/sv_enemies | cast is the IP; the flat cast op does this | M |
| 5 | **IconSet.png and the system folder (13)** | every item, skill and window uses them; IconSet could be built from the 16 voxel items plus new icons | M |
| 6 | Battlebacks (24), parallaxes (9), titles (2), animations (11) | needed for battles, skies and polish; recolours are not acceptable here either | M |
| 7 | **Real-level test of every file before approval**: scratch house with the extra-root preview (`x:` in `generator_level_op level`) and the rpg-pet binary | the old levels have never loaded these files except the yard and the rpg-pet scene | S per sheet |

## 4. How the work is done (all exist and are tested)
- Props, signs, walls, buildings, items: write recipes in `@.apps/generator-hq/events/gen_voxel/*.pdl`, place them with `CELL` rows in `events/gen_objects/<sheet>.pdl`, render with `generator_voxel_op objsheet <out.png> <cells.pdl> <recipes.pdl>... [bank=<concept-bank dir>]`. Weights (warmth, wear, round, big, metal) live in `&.widgits/concept-bank/data/spokes/prop_<name>.pdl` (D32).
- Ground: `generator_tile_op restyle|sheet|sheet-png|tone-run`, `generator_level_op`; verify with `verify-png`, `verify-root`, and **`compare-png`** (the new-art gate: luminance correlation under about 0.5 and a person judging it readable).
- People: `generator_cast_op` (flat 2D, the finisher) with `events/gen_cast/cast.pdl`; voxel route for icons and previews only (the 2D quality gate failed).
- Harness: pal cases in `&.widgits/_shared-lib/harness/` (`generator_voxel.pal` 110/0, `generator_sheets` 60/0, `generator_level` 11/0, `generator_tiles` 37, `generator_restyle` 44, `generator_cast` 44, `generator_char` 12). Run through `cpu_gate`; never bypass exit 3.

## 5. Rules that must not be forgotten
No Python and no new `.sh`. Nothing written into `#.NNEST_ASSETS/rmmv-www-img` (test overlays only in a scratch house). A file goes into `good/` only after the picture is opened and `verify-root` still says problems=0; new art with no original goes in `good-new/` (verify-root walks `good/` and fails on a file with no same-name original). A same-name drop-in must cover every cell its original covers, or old levels lose objects. Say how many pictures were opened. Lore is the owner's unpublished IP: label READ/DOC, never invent canon. Commit by explicit pathspec on branch `claude` with `commit_scoped`; heavy jobs through `cpu_gate`.

## 6. Open owner decisions
1. Is the **0.5 correlation + readable** gate the right bar for ground tiles, or should the owner pick sheets by eye? 2. May a same-name drop-in cover only the cells we drew (the rest blank), or must a sheet be complete before it enters `good/`? (Recommended: complete.) 3. Licence stays parked: until a person reads the terms, nothing leaves the house. 4. Written approval of the pattern is still awaited.

## 7. Added after the owner's question ("make sure we have generators when possible, that's the whole point, give them weights by hand; did u explain that?") 
The first version of this doc named the generators only in passing (section 4) and mentioned weights for props only. **The principle, stated plainly (SAID by the owner, 2026-11-05 and earlier: "details should be read or seeded from some kind of seed + optional args... like a sound synthesizer has knobs"):**
- **Every asset kind is a generator, not a folder of pictures.** Its inputs are data: a recipe or table (`.pdl`), a seed, and knobs. A drawn file in `good/` is the *output* of a generator run; the generator, its inputs and a harness case that reproduces the bytes are what we keep and improve. Nothing is "done" if it only exists as a hand-painted PNG.
- **Knobs are weights by hand.** Concept Bank spokes (`&.widgits/concept-bank/data/spokes/prop_<name>.pdl`, slots `look_warm`, `look_worn`, `look_round`, `look_big`, `look_metal`, each 0.00 = as drawn) are the model: one authority per fact, edits bounded and checkable with `concept_edit_validate`, a person turns the knob. The same shape must be used for ground, walls, people and creatures.
- **Where we already have generators (OBS):** props, signs, walls, buildings, items (`generator_voxel_op` recipes + `objsheet`); characters (`CHAR` rows: seed, presets, knobs build/legs/head/mark/equip/face); the 48-shape ground table and ramps (`generator_tile_op`, `gen_restyle`); the flat cast (`generator_cast_op`). **Where we do not yet (gaps):** ground colours and textures are hand-chosen ramps with no knobs and no bank spokes (Outside_A2 still repaints originals); characters have knobs in `CHAR` rows but no bank spokes; creatures are still C code, not data (D30 step 5); IconSet, battlebacks, parallaxes, animations have no generator at all.
- **Rule for every item in section 3:** before drawing any single cell by hand, ask "which recipe, table or knob makes this?"; add the knob to the bank; only then render. Priority 1 (Outside ground) therefore starts with a **ground generator** (block recipe with weights for hue, wetness, wear, grain, edge softness) feeding the existing 48-shape table, not with repainting.
- A studio to turn these knobs by sheet or by area is proposed as D33 in the decision log (palette-studio).

## 8. Progress after section 7 (2026-11-05, ground)
Priority 1 (Outside ground) has its generator: `generator_ground_op` (decision log D33 step 2): recipes and knobs in the Concept Bank, all four A2 sheets generate with identical transparency and gate correlation 0.11 to 0.24. **Still open for priority 1:** A1 water, A3 roofs, A4 walls, A5 floors; a real-level test; the visible 24-px repeat; per-block recipes instead of the old style map. Then priority 0 (map-use scan) to decide which blocks matter most.
