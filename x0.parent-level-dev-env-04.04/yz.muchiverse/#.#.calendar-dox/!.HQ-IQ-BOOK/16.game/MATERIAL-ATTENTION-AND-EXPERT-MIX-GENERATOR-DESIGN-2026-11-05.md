# Material-driven generation: attention over concept weights, an expert mix, periodic-element colour (design, 2026-11-05)

Labels: **SAID** owner; **READ** in a named file; **OBS** observed today; **DOC** my reasoning, not run. **Nothing in sections 3 to 9 is built.** This is the big one, so it is written down before any code.

## 1. What the owner said (SAID, 2026-11-05)
"I want to make sure the generator uses the attentions and weights to generate this stuff based on **material type** (using our **coloring algorithm for periodic elements**, see that? they can also have **concept weights** and on **gradient and attn scores** related etc based on **corpuses**, and **MoE**? get it? doc this design, it's a big deal)."

**My reading (DOC, for the owner to correct):** a tile is not a hand-picked recipe. It is a *material* (a brick wall is clay and iron oxide, a hull plate is an alloy, thatch is cellulose). The material's chemistry gives its base colour (the periodic-element Drude colour). Concept weights (warm, wet, wear, grain, edge, round, metal, big, worn) describe how that material looks. An **attention** step scores the words describing a tile (its role, its place in the Centroid, the owner's lore) against the materials and concepts, using statistics counted from **corpora**. A **mixture of experts** (the pattern generators) is chosen and blended by those scores. Gradients let one knob slide smoothly across a sheet.

## 2. What exists today (OBS and READ)
| Piece | Where | State |
|---|---|---|
| Periodic-element colour, 118 elements | `&.widgits/palettes/ops/elements_palette_manager.c` `drude_color(z)` (READ): plasma frequency from valence electrons per nucleon, reflectivity `refl = wp/(wp+1)`, a heavy-element blue-absorption term, Cu and Au named, light nonmetals tinted cool. Spec `#.ref/menu/palletes/chem-viz.txt` section 3 ("do not hardcode elemental colors") | **Lives inside one palette window.** The generator cannot call it. The file says itself: "a physics-shaped heuristic, not a solid-state solver. Tune later." |
| Element and compound data | `&.widgits/concept-bank/data/chem/elements.pdl` (118 rows, hash-locked), `compounds_chem.pdl` and `compounds_mcat.pdl` (formula, elements, state, density, hint) | Data only, no colour |
| Concept Bank masters and spokes | `concept-bank/data/masters/{ground_warm,wet,wear,grain,edge,look_*}.pdl`, `data/spokes/ground_<recipe>.pdl` (slots `POINTS_TO` a master with `WEIGHT`) | The 5 ground knobs and 5 object knobs are read by the generators (`bank=`). Weights per recipe, hand-set |
| Generators | `generator_ground_op`, `generator_voxel_op` | **Recipe = hand-typed hue, saturation, lightness, one pattern name, a seed** (25 ground recipes). Nothing derives a colour from chemistry. **No attention, no mixing of patterns, no corpus.** |
| Rule for models | `WORD-BANK-TO-IMAGE.md` section 3 (READ) | A model drafts; it never sets a weight, never promotes a bank row, never approves its own output. A human promotes |

**So the honest gap:** the recipes are the router (a BLOCK row names one recipe), the pattern generators are the experts, and a human is the gating network. Everything in this doc replaces the human gate with a measurable one, step by step, without letting a model write weights.

## 3. The model, in house terms (DOC)
```
 words (role, place, lore)  ──┐
 corpus counts (co-occurrence)├─► ATTENTION scores ─► gate over materials ─► material mix
 composition (Z:fraction)  ───┘                                              │
                                                                             ▼
 Drude colour (per element) ─► material base colour + reflectance      concept weights (spoke slots)
                                                                             │
                              expert gate (top-k pattern generators) ◄───────┘
                                       │
                               24 px periodic tile ─► sheet (A1..C) ─► studio knobs and gradients on top
```
1. **Material** = a row of data: `MATERIAL | name | class | composition | state | density | notes`, e.g. `brick_red | ceramic | Fe2O3:0.08,SiO2:0.55,Al2O3:0.30,... `. Composition is a list of formulas or element ratios from the chem data, not a free colour.
2. **Base colour from chemistry.** Metals (class `metal` or `alloy`): the existing Drude function per element, mixed by atom fraction in *linear* light (average reflectance, not average hex). **Honest limit (DOC):** the Drude model only explains metals. Clay, wood, grass, ice, snow, cloth get their colour from pigments and absorption, not from free electrons. For those classes the table carries a small **pigment term** (iron oxide red, chlorophyll green, cellulose tan, water-ice blue) keyed by element or compound from the chem data. Both routes end in one function `material_colour(material) -> (hue, sat, lightness, reflectance)`, so a recipe's hand-typed HSL becomes the *fallback*, not the source.
3. **Concept weights.** Each material has a spoke (as today) whose slots point at the masters (warm, wet, wear, grain, edge, round, metal, big, worn). Defaults come from the class (metals: high `metal`, low `wet`), authored once as data. Existing spokes keep working unchanged.
4. **Attention.** A query vector `q` is built from the tile's words (role: "roof", "wall", "floor"; place: "Centroid hall", "inside"; lore tags). Keys are the material concept-weight vectors `k_m` (the same 9 numbers). Score `s_m = (q · k_m) / sqrt(d) + b_m`, where `b_m` is a corpus prior (section 5). Gate `g = softmax(s / T)`. All of it is small fixed-size arithmetic over at most a few hundred materials, so it runs in C in microseconds and is **deterministic** (no random number outside the seed).
5. **Mixture of experts.** The experts are the existing pattern generators (noise, cobble, plank, blades, plate, grating, circuit, brick, shingle) plus later ones. Each material lists `EXPERT | pattern | affinity`. The router picks **top-k (k = 2)** experts by `g · affinity`, renormalises, and blends **the per-pixel shade value** of the experts (not the finished picture). **Periodicity is preserved by construction:** every expert is 24 px periodic, a weighted sum of 24 px periodic fields is 24 px periodic, and `tile_period_op` already proves it. Capacity: k = 2 per tile, so cost is at most 2 experts per pixel.
6. **Gradients.** A *gradient* is a knob that varies across a region instead of being constant: `wear` rising toward the bottom of a wall, `warm` rising toward a sunlit side, a blend that fades from stone to moss over a sheet row. Data form: `GRADIENT | knob | axis x|y|radial | from | to | curve`. It is a per-block offset added to the weight before the 0.6 response curve, so it never breaks the 24 px rule inside a block (the whole block takes one value; the next block takes the next). A second meaning of "gradient" (training a gate by gradient descent on scores from human picks) is in section 6, step M5, and is optional.
7. **Reseed and scramble stay.** `variant=N` and `scramble=S` act after the mix, so every look the owner already likes keeps working.

## 4. Why attention at all (DOC, the deciding reasons)
- A new sheet (Dungeon B, an inside shelf) needs no new hand-typed recipe: describe it in words, the gate picks materials, the mix picks patterns. This is what makes "the 10th consumer" cheap.
- The score is a visible number per tile (the gate), so a wrong look can be traced: "the roof took 70% slate and 30% clay because the word `Centroid` raised slate" is a row of data, not a mystery. This follows the house rule "a number shown needs a live writer" and "never report a number you did not observe".
- The same scores later rank candidate tiles for the concept bank (the `tomom` / attrition track): a candidate is accepted when its gate matches what a human approved.

## 5. Corpora (DOC)
What counts as a corpus here, **in-house only** (owner: "keep it in-house"; the lore is unpublished IP, so no outside services, no public repos):
| Corpus | Gives | Source |
|---|---|---|
| Material descriptions | word to material co-occurrence (the prior `b_m`) | the chem data hints (`compounds_*.pdl` hint column), the concept-bank `zh` word tables, hand-written material notes |
| The approved tiles | which materials/patterns the owner already accepted per role | `good/` folders and saved studio records (`studio_saves/*.studio.pdl` carry the knobs) |
| Lore and place words | tags such as Centroid, Spiral Alley, Sky Port, Metal Sun | the Soul Pen canon (labelled SAID / READ), used only as local data |
| The RPG Maker originals | per sheet: mean colour, pattern period, size | `floormap` already measures mean colour; a `statmap` op would store them as data |
**Counting is plain arithmetic** (word to material counts, normalised with a smoothing term), written to a `.pdl` file by a C op from the corpus text. A language model may *suggest* corpus rows (free models only, through the Mac), and the rule from `WORD-BANK-TO-IMAGE.md` section 3 holds: it never sets a weight; a candidate row is validated and a human promotes it.

## 6. Build order, smallest provable step first (DOC)
| Step | What | Proof that can fail |
|---|---|---|
| M0 | **Lift `drude_color` into a pure shared text-include** (`_shared-lib/material_colour.c`), used by the palette window and the generator. House shape for pure logic with 2+ consumers (memory: "no header-link split"). The palette window's 118 colours must not change | harness: all 118 hex values equal the old ones (pinned); a deliberately wrong value fails |
| M1 | `materials.pdl` (about 40 materials for walls, roofs, floors, sci-fi, with class and composition) and the op `material_colour` printing HSL. Recipes gain an optional `MATERIAL` field; the old HSL stays as fallback | cases: Cu and Au come out warm, Fe and Ag grey; an unknown element is refused (exit 2); recipes without MATERIAL give byte-identical output (A2 v0 hash `e035291e2c218d5b` stays) |
| M2 | **Expert mix**: a recipe may list two patterns with weights; blend the shade value. A mix is on or off per recipe | `tile_period_op` `checked=576 mismatches=0` on a mixed tile; a wrong period fails; mix weight 0 equals the single-pattern bytes |
| M3 | **Attention gate**: `gate_op` reads words + materials + corpus counts, prints scores and the top-k with their numbers | fixed words give fixed scores (pinned); permuting the words does not change the result; an unknown word is ignored and counted |
| M4 | **Gradients** in sheet data and a Gradient control in the studio (Knobs panel) | the block at gradient start and end differ by the stated amount; inside one block the tile still repeats |
| M5 (optional) | Fit the gate bias from approved tiles (gradient descent on a few hundred numbers, in C) | held-out approved tiles rank higher than shuffled ones; trained weights land only as a *candidate* file |
| M6 | Studio: a Material tab (pick material, see its gate scores as numbers, slide a concept weight and see the change live) | frames opened and counted; relay-driven |

## 7. Costs and limits (DOC, to be measured before a commit claims them)
- **CPU:** a sheet is rendered by one short process on a click (about 0.1 to 0.5 s today). The gate adds microseconds. Mixing doubles the per-pixel work of mixed blocks only. No new always-running process. Heavy runs go through `cpu_gate`.
- **Files:** a corpus count file (a few KB per corpus, rewritten whole, capped); gradient rows are data. A row goes into the runtime growth standard for any new writer.
- **Drude is not the whole story** (section 3 point 2); the pigment table is hand-authored and will be wrong in places. It is data, so it is editable, and the studio shows the derived colour next to the original so a human can see the difference.
- **Attention here is a scoring rule, not a trained transformer.** It borrows the shape (query, key, softmax, top-k gate) because that shape is auditable at this size. If the owner wants a learned model later (the `tomom` pipeline), this is the data contract it would plug into.
- **Not decided (owner):** how many materials to start with; whether lore words may bias materials (for example sci-fi hull stays blue-grey); whether the pigment table may use outside references (colour of iron oxide) or must come from the house chem data only.

## 8. Design-choice test (AGENTS.md rule, answered in order)
1. *One authority per fact?* Yes: colour from `material_colour` (one function), weights in spokes, corpus counts in one data file, recipes only name materials.
2. *Modular?* Yes: data rows plus one small shared pure function; no per-project code in shared files.
3. *Scalable (the 10th consumer)?* Yes: a new sheet is words + a few rows; nothing new in C.
4. *Long term?* Points at the concept-bank, attrition and `tomom` direction and the in-memory-DB end state.
5. *House shapes?* Text-include (pure shared function), `.pdl` data, a C op for counting, a harness that can fail, capped runtime files.
6. *Cost and risk:* medium. Risk is false precision (a physics colour looks authoritative but the pigment table is hand-made), so every derived colour ships next to the original and with a fallback.
Alternative rejected: hand-tune more recipes (cheap now, but the human stays the router for every new sheet) and "train a diffusion model" (no GPU, no weights on disk, section 2 of `WORD-BANK-TO-IMAGE.md`).

## 9. Relation to the studio work
Sections 8 to 17 of `PALETTE-STUDIO-WINDOW-AND-USER-CONCEPTS-DESIGN-2026-11-05.md` are the window and the sheets this feeds. The remaining sheets (Inside, Dungeon, World and sci-fi B and C, A1 water) can be built first with hand recipes, as the owner asked, and are *re-expressed* as materials at M1 without changing their output (the byte-identical proof above).
