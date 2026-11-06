# 🧠🎨 AI report for the owner: how our words become tiles, raw text-to-image, and edits to existing tilesets

*Written 2026-11-05 (Claude Sonnet 5.5) for the **nov-6** folder. Personal, goals / calls / decisions first, details after, external references at the bottom so another AI agent can pick this up cold.*
*Labels: **SAID** = you told an agent · **READ** = in a named file · **OBS** = I ran or opened it today · **DOC** = my reasoning, not run.*

---

## 1. 🎯 The short version (read this if nothing else)

You asked three things: (a) make the generator use **attention + weights by material**, with our **periodic-element colouring**, concept weights, gradients, corpora and a **mixture of experts**; (b) check **tomom / attrition** for anything to reuse; (c) explain **how these words will be used for later raw text-to-image and for editing existing tilesets**.

**What I found:** most of the machinery you describe already exists in pieces, scattered across four projects. They have never been wired together. The wiring is the work.

| The ideal you described | What the house already has (OBS/READ) | Gap |
|---|---|---|
| Periodic-element colour | `drude_color` in the elements palette window, 118 colours | It was trapped inside one window. **Fixed today (M0)**: lifted into a shared include, all 118 colours identical (0 differing lines, OBS) |
| Words → weights from a corpus | `hidden_layer` (hand-trained word table: mean vector of reviewed phrases, stop words, exact / words / none) | Only 3 dimensions (energy, force, motion). Needs the 9 look dimensions |
| A word bank for pictures | `img_bank` / `img_prompt` / `img_edit` / the **img-hai** window: words with kind + weight, prompt builder, candidate edits, promotion by a *different* person | Only 24 words and 5 parts. Nothing connects it to the tile generators |
| Attention | `tomom` `attention.c` (softmax, causal mask), `chatbot_moe_v1.c` (temperature gate over several models) | Dormant, one known bug, not trained. Too heavy for a tile; useful as the *shape* and later as a *learner* |
| Mixture of experts | pattern generators in `generator_ground_op` (noise, cobble, plank, blades, plate, grating, circuit, brick, shingle) | A recipe names exactly one. A human is the router |
| Attrition loop (propose → validate → human review → promote) | `concept_edit_validate`, `img_edit`, the candidate folders, `AUTO-PROMOTION-RULE.md` (a stub) | Works for bank weights; not yet for tiles |

**Decision I made (and why):** build it in the order **M0 → M1 → M2 …** (design doc D35), each step with a proof that can fail, and **reuse `hidden_layer` + `img_bank` + the attrition loop rather than writing a new text model**. The studio's attention gate is a small deterministic scoring rule; `tomom` is kept as the later *learner* of the gate's bias, not as a dependency.

---

## 2. 🧱 What I built today (OBS)

1. **M0 — the element colour is now shared.** `_shared-lib/material_colour.c` (text-include, pure). `elements_palette_manager.c` now includes it. Proof: the old palette's published 118 colours vs the new maths = **0 differing lines**; the new manager binary run in a scratch folder also publishes the identical 118; a pin file + a deliberately wrong pin (`bad/elements_wrong.pin`) show the check can fail.
2. **M1 — materials from chemistry.** `events/gen_material/materials.pdl`: 16 pigments (every one marked **DOC = my estimate, not a measurement**) and 25 materials (compositions such as `Fe2O3:0.60,SiO2:0.40`). `material_colour_op` mixes components **by fraction in linear light** and prints the colour. Metals use the periodic-element Drude colour; everything else needs a pigment row.
3. **The honest calibration** (derived colour vs the hand-typed recipe, 21 recipes that have a material):
   - **hue** within 10° for **13 of the 17** recipes that actually have colour (the other 4 are grey, where hue is meaningless);
   - **saturation** within 10 points for only **9 of 21** (red ceramics come out too grey because silica washes them out);
   - **lightness** within 10 points for only **10 of 21**: the derived value is a *surface* colour, bright, while a recipe's number is the *mid-tone of a shade ramp*. This is a systematic offset, so the next step is a **light-to-ramp mapping**, not "chemistry is wrong".
   - Pure metals: gold comes out warm gold (hsl 32,83,62), iron grey (30,2,82), **copper comes out gold-ish (37,81,69), not copper-red**, and **silver gets a warm tint (24% saturation)** from the heavy-element blue-absorption term. Those two are properties of the *existing* algorithm, not of my change; I did not touch them. The header comment in the file says the same ("tune later").
   - So: chemistry gives the right **family** of colour for warm natural materials, and is **not yet a replacement** for hand recipes. It is a measured starting point.

---

## 3. 🔤 What a "word" is in this system

Today (READ `img_bank.c`) a word is a row: `WORD | id | text | kind | weight`, kinds `subject material colour angle style size negative`, weight 0..1, words under a cutoff (0.30) are left out of the prompt. 24 rows exist (OBS).

For tiles a word needs more than a weight. The plan (DOC) adds, **as data, not code**:

| Field | Meaning | Example |
|---|---|---|
| `kind` | gains `role` (roof, wall, floor, prop, door), `place` (inside, outside, dungeon, Centroid hall), `pattern` (brick, shingle, plank), `lore` (tag), `concept` | `role: roof` |
| **concept vector (9)** | warm, wet, wear, grain, edge, round, metal, big, worn, each −1..+1 | `weathered` → wear +0.7, worn +0.6, warm −0.1 |
| **material affinity** | which materials the word pulls toward, 0..1 | `terracotta` → roof_red 0.9, roof_orange 0.6 |
| `weight` | how loud the word is (as today) | 0.8 |
| `stop` | the word carries no signal (as `hidden_layer` already marks "the", "a") | `stop=1` |

**How the numbers get there (READ `hidden_layer.c`, reused, not reinvented):** a person (or a free model *drafting*) writes short phrases; a reviewer accepts / adjusts / rejects each with its 9 numbers; `train` then sets every word's vector to the **mean of the approved phrases that contain it**. It is deterministic counting, no random start, no gradient: "hand-trained". A word in more than a fifth of the phrases becomes a stop word. `forward` returns `source=exact|words|none` so you always know whether a number came from the phrase itself, from its words, or from nothing. **Retool needed:** `hidden_layer` is fixed at 3 dimensions (energy, force, motion). The retool is a `keys` list instead of three hard-coded names. Its harness (`hidden_layer.pal`) must keep passing unchanged for the 3-key case.

⚠️ **Honest caveat (OBS):** the 119 approved rows that exist today (`phrase_assoc`, `chem_assoc`) are signed `by=claude-manager`: an agent reviewed them, not you. They teach the *mechanism*; they are not your taste yet. Anything that decides how your art looks should be reviewed by **you**.

---

## 4. 🛤️ Path A: words → our own generator (no image model, milliseconds)

This is what the studio will do. Everything is small arithmetic, deterministic, runs in C on a click.

1. **Words in.** A tile has words: from its sheet and role (`Outside_A3`, `roof`), from a typed phrase in the studio, or from a lore tag. Stop words dropped, unknown words counted (and shown, so a typo is visible).
2. **Query vector `q`.** Mean of the known words' concept vectors, scaled by each word's weight (exactly what `hidden_layer forward` does, with 9 numbers instead of 3).
3. **Attention over materials.** Each material has a key `k_m` (its 9 concept weights from its spoke) and a prior `b_m` (how often the corpus joins those words with that material). Score `s_m = q·k_m / √9 + b_m`. Gate `g = softmax(s / T)`, with T a temperature (the same idea as `tomom`'s `softmax(scores, size, temperature)`).
4. **Top-2 experts.** Each material lists pattern experts with an affinity. The router takes the two best `g · affinity`, renormalises, and **blends the per-pixel shade value** of the two patterns. Every pattern is 24 px periodic, a weighted sum of 24 px periodic fields is 24 px periodic, so seamlessness is kept **by construction** and `tile_period_op` proves it (`checked=576 mismatches=0`).
5. **Colour.** The material's derived colour (M1) drives the ramp; the concept weights go through the 0.6 response curve as today; **gradients** add a per-block offset across a sheet (wear rising down a wall); `variant` and `scramble` act last, so everything you already like keeps working.
6. **Everything printable.** The studio can show, per tile: the words used, the gate numbers, the two experts and their weights, the derived colour next to the original. A wrong look is traceable to a row of data.

**Cost:** microseconds for the gate; a mixed block costs two patterns instead of one; no new always-running process.

---

## 5. 🖼️ Path B: the same words → a RAW text-to-image model (later)

A real image model (diffusion) is a different animal: it paints pixels from a prompt. We already have the plumbing (img-hai, W0/W1, 2026-10-13); what is missing is **using it for tiles safely**.

**5.1 Words → prompt (READ `img_prompt.c`, reused as is).** Order: subject, material, colour, angle, style, size; within a kind by weight, high first; words under the cutoff dropped; negative words go to the negative prompt. For a tile the prompt builder gets the gate's output as extra words: any material with gate ≥ 0.3 contributes its `words=` text ("terracotta roof tile"), the concept vector contributes its strongest words ("weathered, mossy"), and fixed tile words are always present: *tileable seamless texture, flat top-down, pixel art, no perspective*. Negative: *text, border, perspective, shadow cast, watermark*.

**5.2 Weight → model settings (READ `sd_map.h`).** The bank weight becomes the model's *guidance* (`7.5 × (0.5 + colour weight)`) and *steps* (`20 × (0.5 + detail weight)`). A weight of 0.5 is the default (7.5 / 20). So tuning a word's weight is tuning the picture, and it is recorded as a bank edit, not a hidden slider.

**5.3 What came out last time (OBS, 2026-10-13, `WORD-BANK-TO-IMAGE.md` 2c):** on debil (CPU only), a 512 px image took **about 2 minutes** (124–142 s over four runs) and the tiny models **did not follow our "pixel / top-down / part" words**; one result was a dark-red grid texture, the others whole red cars. So: *the pipeline works end to end, the quality is not there*, and I will not claim more.

**5.4 So what do I recommend for tiles (DOC)?** Use a raw image model as an **inspiration and measurement source, not as the pixel source**:
1. Generate a few 128–256 px reference pictures per material on debil / the Mac (niced, one at a time, never while a game runs, free models only).
2. **Measure** each reference the way `floormap` measures an original: mean colour, contrast, dominant pattern scale, pattern period guess. Those numbers become **candidate recipe rows / candidate concept weights**, not pixels.
3. Our generator then makes the real, **24 px periodic**, alpha-correct, palette-correct tile from those numbers. The model's picture never ships; its *numbers* do, after a human approves them.
4. If a model's pixels are wanted anyway (an object, a sign, a creature), they go through a **tile-forcing and palette step**: crop to 24 or 48 px, blend opposite edges so it repeats, snap colours to the material's derived ramp, then run the same proofs (`tile_period_op`, `sheet_dups_op`). A tile that fails a proof is rejected before anyone looks at it.

**5.5 Why not train our own diffusion model?** (READ `WORD-BANK-TO-IMAGE.md` section 1): no GPU on any machine we own, no model weights on disk, and torch/diffusers only on CPU. The `tiny-ai.goal` idea (a small student distilled from a big teacher, "KD") stays the long road; the **banks first, hand-tuned words** route is the one that gives results now. If you ever want the student, **every accepted tile + its words + its gate numbers is training data**, already being recorded by the studio's save files.

---

## 6. ✏️ Path C: using words to EDIT existing tilesets

There are four edit modes, from safest to riskiest. All of them keep the rules the generators already enforce: **alpha untouched, 24 px periodic where the original is, one block at a time, every edit replayable from a record.**

**C1. Knob edit on a selection (works today, no AI).** Studio Select tab + warm / wet / wear / grain / edge / scramble. Saved as a `.studio.pdl` record that replays exactly.

**C2. Words → knob edit (next).** You type "make the Outside roofs older and a bit teal". Steps (this is the attrition loop, READ `NIGHT 30` / `concept_edit_validate.c` / `img_edit.c`):
1. A model (Gemma on the Mac, `10.0.0.144:11434`, free, never the local Ollama) is asked to **DESCRIBE in one fixed line**: `TARGET: <block set or selection> | MATERIAL: <name> | CONCEPT: <name>:<strength high|medium|low> | REASON: …`. It **never** outputs a number and never classifies.
2. Real code maps `high|medium|low` to a delta through a fixed table (for example 0.30 / 0.15 / 0.05), applies the 0.6 response curve, and writes a **candidate `EDIT`** line (a new type `tile_concept_delta`, next to the existing `spoke_weight_delta` and `img_word_weight_delta`).
3. A validator checks: target exists, concept exists, delta in bounds, the result *renders* and still passes `tile_period_op` and the identical-tile scan. One bad line rejects the whole candidate (existing rule).
4. A review file is written. **A human promotes** (`img_edit promote` already refuses promoter == proposer). A ledger row records who, when, what.
5. Only then does the studio replay the edit onto the sheet.

**C3. Words → material swap on chosen blocks.** "Turn these walls from brick to plank." The gate picks the new material and experts for the selected BLOCK rows; unselected blocks keep their old bytes (the same select-limited rule as the knobs). This is cheap because BLOCK rows are per block: an edit is a data change, not a repaint of the sheet.

**C4. Pixel edits by a model (img2img / inpainting), last and optional.** A model edits one chosen cell with a mask. Guardrails: it works on a **copy**; alpha is restored from the original afterwards; the tile-forcing and palette step from 5.4 runs; the proofs run; the result is a *candidate* image in `candidates/`, never written into `good/` and **never written into the original RPG Maker image folder** (your rule: new levels only, never replace).

**C5. The reverse direction: describing what already exists.** `floormap` already measures an original sheet (mean colour per cell/block) and proposes a recipe. With materials this becomes "this A3 block is roughly `roof_red` with gate 0.7 / `roof_orange` 0.3". Those are candidate rows for you to accept, and they are also **the first real corpus**: 31 source sheets measured, in your own data format.

---

## 7. 📚 Corpora: where the counts come from

All **in-house**. Your lore is unpublished IP, so nothing goes to an outside service. Sizes below are what is on disk now (OBS):

| Corpus | Size now | Used for |
|---|---|---|
| Chemistry: compounds (`compounds_chem` 50, `compounds_mcat` 36), phrases (86), element names zh (118), `words.txt` (187) | ~480 rows | material descriptions, word → material prior |
| Reviewed phrase → concept rows (`phrase_assoc` 46, `chem_assoc` 73) | 119 rows (agent-reviewed) | the learned word table |
| Image word bank (`img/words.pdl` 24, `parts.pdl` 5) | 29 rows | prompts |
| Concept bank masters 14, spokes 67 | 81 files | the 9 look dimensions and per-recipe weights |
| 31 source tileset sheets (RPG Maker) | 31 sheets | measured by `floormap`; **never copied, only measured** |
| Studio save records | grows with use | what you accepted; the gate prior learns from these |
| Lore tags (Soul Pen canon) | local only | optional biases; the decision is yours (see 9) |

Counting is plain arithmetic: word ↔ material co-occurrence with a smoothing term, written to a capped `.pdl` by a C op. A model may **suggest** rows (free models only); a reviewer promotes.

---

## 8. 🧩 tomom and attrition: what to reuse, what to leave

**Reuse now**
- ✅ **`hidden_layer`**: the word table. Retool to N keys.
- ✅ **`img_bank` / `img_prompt` / `img_edit` / img-hai**: the word bank, prompt builder, candidate-edit grammar, "promoter ≠ proposer" rule, the ledger.
- ✅ **`concept_edit_validate` + the candidates folders**: the validator pattern; we add one new edit type.
- ✅ **The attrition loop** (Watch → one-line DESCRIBE → code maps strength to delta → validate → human review): this *is* Path C2.
- ✅ **Studio records** are Watch records: each `.studio.pdl` says which knobs produced an accepted sheet.

**Reuse later**
- 🕰️ **`tomom`**: the right shape (`attention_forward`, softmax with temperature, `chatbot_moe_v1` gating) and a corpus/curriculum scaffold. Its own `goals_report.md` says MoE multi-expert, distillation and the emoji renderer are **not implemented**, and one causal-attention path is hard-coded to 0. So it is **not on the critical path**. Its job later (M5): learn the gate's bias `b_m` from approved tiles, as a **candidate file a human promotes**, never writing weights itself.
- 🕰️ **`tiny-ai.goal`**: the "student distilled from a teacher" road for a real in-house image model. Needs a GPU you do not have.

**Do not do**
- ❌ Link `tomom` into the studio (house rule: no header+link split; use a text-include or op + IPC).
- ❌ Any auto-promotion (`AUTO-PROMOTION-RULE.md` is a stub on purpose).
- ❌ Let a model set a weight, approve its own output, or write into `good/`.

---

## 9. ❓ Decisions I need from you (none blocks the next step)

1. **Who reviews the word → concept numbers?** The 119 existing rows were agent-reviewed. For tile looks I recommend **you** review a first batch of ~30 phrases (a table: phrase + 9 sliders), then I count the rest.
2. **Lore words as biases?** E.g. "Centroid" nudges stone/marble/clean lines, "Metal Sun" nudges gold. Local data only. Yes / no / later.
3. **Pigment table sources.** My 16 pigments are estimates. OK to keep them as editable data, or must they come only from house chem data?
4. **Raw image models: go?** Per your free-only and offload rules the first trial would be debil (CPU) with small models, one image at a time, niced. I recommend a **measurement-only** trial (Path B 5.4 steps 1–3) before any pixels are used.
5. **Chinese labels and the 2D/3D toggle** (your earlier asks) remain queued; Chinese needs a CJK-complete font in the studio stylesheet (the renderer currently falls back to a Traditional-only font and shows empty boxes for some Simplified characters, OBS).

---

## 10. 🗺️ Order of work (DOC, in the order that makes sense to me)

1. ✅ **M0** shared element colour (done, proven).
2. ✅ **M1** materials + chemistry colour (done; calibration above; light-to-ramp mapping is the next fix).
3. ➡️ **Retool `hidden_layer` to N keys** + a first 30-phrase review sheet for you.
4. **M2** expert mix (two patterns blended, periodic proof, mix weight 0 = old bytes).
5. **M3** attention gate op (`gate_op`: words + materials in, numbers out; fixed words give fixed scores).
6. **M4** gradients; **M6** a Material tab in the studio.
7. **Edit path C2** (`tile_concept_delta` candidate edits through the existing validator).
8. In parallel with 3–7: the remaining sheets (Inside, Dungeon, World, sci-fi B and C, A1 water + Play), written with hand recipes first and re-expressed as materials at M1 with a byte-identical proof.

---

## 11. 🔗 External references (repo paths and outside URLs)

**Repo (house root `x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00`, BOOK = `…/#.#.calendar-dox/!.HQ-IQ-BOOK`)**
- This design: `BOOK/16.game/MATERIAL-ATTENTION-AND-EXPERT-MIX-GENERATOR-DESIGN-2026-11-05.md` + decision `D35` in `BOOK/02-architecture/DESIGN-CHOICE-TEST-AND-DECISION-LOG.md`
- Studio: `BOOK/16.game/PALETTE-STUDIO-WINDOW-AND-USER-CONCEPTS-DESIGN-2026-11-05.md` (sections 8–17)
- Shared element colour: `&.widgits/_shared-lib/material_colour.c`; spec `#.ref/menu/palletes/chem-viz.txt` section 3; palette `&.widgits/palettes/ops/elements_palette_manager.c`
- Materials + op: `@.apps/generator-hq/events/gen_material/materials.pdl`, `@.apps/generator-hq/ops/material_colour_op.c`
- Generators: `@.apps/generator-hq/ops/{generator_ground_op,generator_voxel_op,generator_cast_op,palette_studio_op,palette_studio_ui_op,tile_period_op,sheet_dups_op}.c`; recipes `events/gen_ground/ground.pdl`
- Word / concept banks: `&.widgits/concept-bank/` (`data/masters`, `data/spokes`, `data/img/words.pdl`, `ops/hidden_layer.c`, `ops/img_bank.c`, `ops/img_prompt.c`, `ops/img_edit.c`, `ops/sd_map.h`, `ops/concept_edit_validate.c`, `AUTO-PROMOTION-RULE.md`)
- Image studio window: `&.hq-apps/img-hai/docs/README.md` (+ `HANDOFF.md`)
- Plan for word-bank → image: `BOOK/16.game/WORD-BANK-TO-IMAGE.md`
- tomom: `#.Z.HUMAN_LLM/3.stage.llm.tomom@qroq.fame]921🐋️/` (`attention.c`, `chatbot_moe_v1.c`, `dox/goals_report.md`, `dox/tiny-ai.goal🧬🔥]9.17.23.md`)
- Attrition: `#.#.calendar-dox/1-1.HARNECIENT.SMOL/NIGHT_30_THE_ATTRITION_MODEL.txt`, `A-TEARIT-IS-ALL-YOU-NEED.md`
- Harness: `&.widgits/_shared-lib/harness/` (`README.md`, `cases/palette_studio.pdl`, `cases/hidden_layer.pdl`)

**Outside (for other AI agents)**
- Drude model: https://en.wikipedia.org/wiki/Drude_model
- "Attention Is All You Need": https://arxiv.org/abs/1706.03762
- Sparsely-gated mixture of experts: https://arxiv.org/abs/1701.06538 · Switch Transformers: https://arxiv.org/abs/2101.03961
- Knowledge distillation: https://arxiv.org/abs/1503.02531
- Diffusers (image models on CPU): https://huggingface.co/docs/diffusers

*Nothing in this report was sent to an outside service. The lore stays local.* 💛
