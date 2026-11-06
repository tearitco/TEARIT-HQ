# good/: approved Tear-it files (2026-11-05)

**Approval.** The owner said: "u can approve to good folder if u think they are acceptable, and we can test them in our old rpgmaker dependant levels." (SAID). claude approved the 11 files below under that delegation. The owner can veto any of them by deleting the file: nothing depends on this folder being complete.

**Criteria used (a file needs all of them):**
1. Same name, same width and height as the RPG Maker file it replaces, so the old levels load it unchanged: `generator_tile_op.+x verify-root <rmmv-www-img> good` (OBS: files=11, problems=0).
2. For a recoloured or ramp-built sheet: transparency identical to the original and something changed (`tone-run` and `verify-png`, both ok). A person sheet is a different drawing, so only 1 applies.
3. Deterministic: the same command and inputs give the same bytes (harness cases).
4. **claude opened the picture and judged it readable at 1x with no obvious defect** (flat fills, banding, missing parts, wrong layout).
5. Not a copy of the original pixels.
6. Drawn from the IP where the lore says what it looks like (the cast rows cite READ and DOC); a DOC colour is a proposal, not canon.

**Approved (opened and judged by claude):**
| File | What it is |
|---|---|
| tilesets/Outside_B.png | objects, recoloured (tone: hue -10, saturation 112, 6 levels, ink outline) |
| tilesets/Outside_A2.png | **calm version (2026-11-05 late)**: grass blocks use `tearit-green-calm` (narrow ramp, no speckle) via `events/gen_restyle/blocks-calm.pdl`; the first version was far noisier than the original grass (seen in the test yard) |
| tilesets/World_A2.png, Inside_A2.png, Dungeon_A2.png | the other A2 ground sheets, built by block with the ramps (`sheet-png` + `blocks.pdl`) |
| battlebacks1/Grassland.png | a painted background (tone, 14 levels) |
| characters/Actor1.png | Asa, Ava, Harold, Lucky, Astra, Gem, Loona, Kael |
| characters/Evil.png | Nihilator, two clowns, Time Police, Carcazod, Planet X hound, Angler, shark-kin |
| characters/Monster.png | Garden Mind, Jelly Fish God, puffer-folk, lion-fish noble, Chronos, caretaker, Murmur, Solvent |
| characters/People1.png | eight generic NPCs |
| faces/Actor1.png | the faces of the Actor1 cast |

**Known flaws in approved files:** `Outside_A2` block 24 (a flat white block in the original) is a dull flat purple square; the A2 sheets use three or more hand-chosen ramps per sheet, so materials are told apart by shape and lightness more than by hue; Harold's and the pets' looks are placeholders, not canon.

**Not approved** (generated and verified by the tools, but claude has NOT opened them): the other ~90 recoloured sheets, the other six people sheets and nine face sheets, the themed world packs, and everything for which no RPG Maker slot exists yet (side-view actors, side-view enemies, big monsters, vehicles).

**Hashes** (`APPROVED.sha256`, run `sha256sum -c APPROVED.sha256` in this folder). Rebuild commands: see `../README.md` and `../interim/*/README.md`.
