# Tech trees and the render-research pipeline (design, 2026-10-27)

The owner's idea: entities **research tech trees** by asking an API / Claude for **.txt / .png renders of a thing from different angles**, then do **image distillation and mathematics** that relates those pictures to a **baseline, empty, customizable game engine**. This document turns that into a pipeline whose every stage has a contract and a test. Labels: **BUILT** (code + harness run), **EXISTS** (in the repo, not new), **PROPOSED** (nothing written), **OPEN** (owner decision). Companions: `TERUMON-SHAPESHIFT-AND-EVOLUTION-TREES-DESIGN.md`, `GROK-HANDOFF-2026-10-27-VISUALS-AND-ORGANIC-TRACK.md`.

## 1. The loop in one picture
```
tech tree node ──► ASK (fixed prompt, budgeted) ──► candidate pictures (3 views) ──► DISTIL ──► candidate model
 (techtree.pdl,        provider ladder:               front/side/top as .txt grid     views_to_voxel     (model.txt)
  tree_lint)           Mac Gemma → free APIs          or .png                         hull + consistency      │
        ▲              img-hai for pictures                                                                 ▼
        │                                                                       REVIEW (a person promotes; nothing auto-kept)
        └──────── unlocks the node, the form, the curriculum words ◄──────────── PROMOTE into forms/<id>/model.txt
```
Everything left of PROMOTE is a **candidate**. This is the house's attrition loop (watch → describe in a fixed line → code decides → validate → review → a person promotes). The model never decides; the code and the gate do.

## 2. Tech trees (EXISTS as a design, one piece BUILT)
* Format already specified in `@.apps/pet-trainer/TOWN-ECONOMY-AND-LEARNING-DESIGN.md` section 4: `NODE | id | title | needs=<ids> | domain | ask="<fixed prompt>"`. A node unlocks when its `needs` are passed in the entity's curriculum (`entity_grade`).
* **BUILT:** `tree_lint` (harness 53/0) already validates exactly this shape: unknown or cyclic `needs`, duplicate ids, bad numbers, `--require ask,domain`, `--tier-monotone`, and `--closure <id>` answers "what must be researched first and what does it cost in total". Run it on every tree file in CI/gates before a game ships it.
* **PROPOSED additions to the row:** `render=front,side,top` (which views the node needs), `max_dim=32,32,16` (engine slot), `style=` (a concept-bank key for palette/prompt words), `cost=` (research points or MP).
* Tech tree vs evolution tree: the **evolution tree** says *what a thing becomes* (parent=); the **tech tree** says *what must be known first* (needs=). A form usually has both: `parent=pup`, `needs=pack_lore`.

## 3. ASK: how an entity requests renders (PROPOSED op `research_ask`)
* Once per entity per day per node family, **costs MP** like a skill (existing skill-book path), picks an unlocked node by weight.
* **Provider ladder** (existing pattern, `^.hai-horn/ops/horn_chat_backend.c`, `ladder_pick.c`): Gemma on the Mac (`gemma_lan_url` from `ai_backend.pdl`, **never** local Ollama) → free providers the owner holds keys for (Groq / OpenRouter `:free`) → stop. **Nothing paid.** Per-provider daily budget counter in state; a 429 skips that provider for the day. **Standing rule (owner 2026-10-27): never loop to a 429; read headers from one call.**
* **Two request styles, because they fail differently:**
  1. **Text grids** (`.txt`): a text model is asked for three ASCII silhouettes of the thing (front, side, top) with a legend of colours. Cheap, no GPU, a free text model can do it. Weakness: models are bad at keeping three grids mutually consistent, which is exactly what the gate measures.
  2. **Pictures** (`.png`): the existing `img-hai` pipeline (EXISTS: text-to-image on the Mac or debil by ssh, proven 12/12 unattended jobs, pixel-art reduction in the pipeline; quality "a wheel is solid, others usable in some seeds", the owner has not yet judged any picture). Weakness: **an image model does not return true orthographic views**; three separate prompts give three drawings of "a wolf", not three projections of one wolf. So the pictures path needs either a render-from-model step (section 6, the generator side) or a loose consistency threshold plus human review.
* The prompt is **fixed per node** and asks for **one parseable block** (`VIEW front` … `VIEW side` … `VIEW top` rows, a size line). Anything that does not parse is rejected and logged, never partly applied (same rule as the lesson-card design).
* Where the answer lands: `research/pending/<entity>/<node>/<n>/front.txt|png, side…, top…, source.txt` (model, date, prompt id, lane). `status=candidate`. It is **not** in any bank or form.

## 4. DISTIL: pictures → model (BUILT: `views_to_voxel`)
`&.widgits/_shared-lib/ops/views_to_voxel.c`, build `build_views_to_voxel.sh`, harness `views_to_voxel.pal` (**60 checks, 0 failing**). Usage: `views_to_voxel <out_model.txt> <front> <side> <top> [--solid] [--max-vox N] [--min-consistency F] [--bg R,G,B]`; each view `.png` or `.txt`.
* **The mathematics (a law, deterministic, never weighted):** a voxel (x,y,z) is kept iff it lies inside **all three silhouettes**. This is the **visual hull**. Axes follow the engine's own `model.txt`: front = X across, Y up; side = Z across, Y up; top = X across, Z down the rows.
* **The consistency number** is the gate. The hull's own projection can never exceed a silhouette, so *the share of each picture's filled pixels that the hull's projection reproduces* is ≤ 1, and **= 1 exactly when the three pictures describe one solid thing**. `consistency = min(front, side, top)`. Default threshold 1.0: a set that disagrees (a wing in the side view the front view cannot see) exits 4 and **no model file is left behind**; an empty hull exits 4 too; `--min-consistency 0.9` lets a person accept a looser set knowingly. Exit codes: 0 ok, 1 usage, 2 unreadable or mismatched sizes, 3 over `--max-vox`, 4 inconsistent/empty.
* **Output:** `model.txt` in the format `rpg_space.c` asteroids and `pieces/veh_N/model.txt` use. Surface voxels only by default (`written_voxels`), `--solid` keeps the interior. Colour: the front picture where the voxel is front-most in its column, else the top picture where top-most, else the side picture where right-most, else front. The report prints `dim_x/y/z`, `hull_voxels`, `written_voxels`, per-view and overall consistency, and `fits_engine` (1 when X≤32, Y≤32, Z≤16: the GPU model slot).
* **Evidence:** expected values come from an **independent Python oracle** (a second implementation of the same maths: a random sparse solid's three projections, a 5×5 disc, an L-shape on a white background, a cube, a transparent-background set) not from the C program's own output. 8 mutations each fail it (union instead of intersection, surface test off, consistency forced to 1, side picture mirrored, `--max-vox` ignored, slot limit wrong, top colour never used, consistency off by one). Asymmetric inputs were needed: my first random solid had a symmetric side view and a mirror bug passed; I replaced it. A flat-colour PNG with no `--bg` is empty on purpose (the top-left pixel is the background).
* **Limits (honest):** it reconstructs only what the three silhouettes allow. The hull is the **largest** solid consistent with the pictures (concavities that no silhouette shows are filled: a mug's inside is lost, a donut seen from the side is solid). Colours come from three flat pictures, so the back and underside are guesses (front colour). Pictures larger than 96×96 are refused. It does not read lighting, perspective or an AI's idea of "from a slight angle": inputs must be true orthographic silhouettes.

## 5. The "mathematics relating to a baseline engine" (what is concrete, what is not)
"Baseline, empty, customizable engine" = the engine with no game content: the voxel grid, `model.txt`, the part/piece system, palettes, the 2D/3D projections, the rows (`TUNE`, `POLICY`, …), nothing authored. Distillation turns a picture into **only things that baseline can hold**. The maths, in order of how real it is today:
| Step | Maths | Status |
|---|---|---|
| Silhouette → solid | visual hull = intersection of three cylinders | **BUILT** |
| Do the pictures agree? | projection-reproduction ratio (= silhouette IoU on the one side where it is exact) per view, min of three | **BUILT** |
| Fit the engine slot | dimension limits, voxel budget (`--max-vox`) | **BUILT** |
| Colour budget | quantise to the palette the game uses (palettes folder / concept-bank style key): k-means or nearest-palette | PROPOSED, small |
| Compare to a prior model | 3D IoU of two voxel sets, "is this candidate better than what we have" for refinement rounds | PROPOSED, small (the oracle code in the harness already computes hulls) |
| Symmetry detection | mirror-IoU about the X axis; enforce symmetry when ≥ threshold (animals, ships); halves research cost | PROPOSED |
| Scale laws | mass ∝ volume (L³), strength ∝ cross-section (L²), a form's real-world size class from the scale ladder in `RPG-DUST-DESIGN.md`; the count of voxels sets a *derived* mass, not an invented one | PROPOSED, law not weight |
| Part decomposition | connected components / colour regions → candidate parts (limb, head), checked by `tree_lint` as an anatomy part tree | PROPOSED, needs the anatomy v0 spec |
| Lighting / normals | not needed: the GPU raymarcher shades the voxels | n/a |
The point of the list: **only the first three exist**; the rest are small, testable, and each should arrive with an oracle and mutations like the first three.

## 6. The other direction: generating the views from a model (PROPOSED, the missing piece for pictures)
Because image models cannot give orthographic views, the reliable way to get a **consistent picture set** is to produce them from a 3D thing we already have, then let a model *restyle* them:
1. The engine renders a model from front/side/top (one-shot `bv_render_3d` with a fixed camera; also useful as 2D parity frames).
2. An image model restyles or details them (img-hai), with the silhouette as a mask so geometry is kept (control by mask / inpainting); or a text model proposes a refined grid.
3. `views_to_voxel` distils back; the consistency number says whether the restyle drifted.
This is the **closed loop** the owner described (renders from different angles → distillation → mathematics against the baseline). It also gives training data for the in-house model: (prompt, seed, views, consistency, human verdict) rows. **The nuanced visuals and the image generation/training are Grok's lane** (handoff).

## 7. Review and promotion (the safety rail)
* A candidate model is shown in a window next to the node (Knowledge HQ is the place for knowledge domains). A **person** promotes (`candidate → forms/<id>/model.txt`); **the proposer cannot promote its own** (img-hai's two-judge rule is the precedent: promotion refuses the proposer).
* Auto-gate before a person sees it: parses, `consistency ≥ threshold`, `fits_engine=1`, voxel budget, no external paths, palette within budget.
* Models can be wrong: tag `unverified`; a fact-like claim (a lesson card about an animal) is promoted only on owner approval, or two providers agreeing AND the entity passing the card's CHECK question in an exam (existing "teach me" rule).
* **Lanes:** every run counts in the **dev** lane (how to build) or the **live** lane (the game's own entities researching); only live-lane evidence trains weights.

## 8. Footprint and budget (owner rules since 2026-10-27)
* Research is an **event**, not a loop: nothing runs between asks; idle cost ≈ 0. Distillation is milliseconds on a 32×32×16 grid (not yet measured on this machine; **measure before claiming**).
* Anything heavy (image generation, training) goes to **debil / the Mac by ssh**, niced, one at a time, never to this CPU. Free models only; per-provider daily budgets; no probing for limits.
* Keys: never in a pet's folder or a chain folder (same rule as signing keys, design doc section 28).

## 9. Build order, each with a test that can fail
1. **DONE** `tree_lint`, `views_to_voxel` (+ harnesses, 53/0 and 60/0, 15 mutations). 2. `research_ask` op with a **fake provider** fixture (good block, malformed block, 429, budget stop; nothing reaches `forms/` without promotion), harness `research_ask.pal`. 3. `model_compare` (3D IoU) + `model_palette` (quantise) + symmetry check, small ops with oracles. 4. `render_views` (engine renders three fixed views of a model: the generator side; checks `views_to_voxel(render(M))` reproduces `M` for convex and for L-shaped models, the **round-trip law**). 5. A Knowledge-HQ-style review window. 6. First real round trip on the Mac (one call, headers read, no looping) and a human verdict logged.

## 10. Open decisions for the owner
1. Which request style first: **text grids from Gemma** (cheap, free, weak consistency) or **restyled renders via img-hai** (needs step 4, better looking)? My recommendation: text grids + the round-trip law first, because they exercise the whole loop without a GPU.
2. The consistency threshold for auto-accept (default 1.0 is strict; real model output may need ~0.9 with human review).
3. Where research results live for a purchased/owned terumon (the chain/store side): in the entity's data, in a shared Knowledge pack, or both.

## 11. Not verified
No provider has been called for a view set; no model from this pipeline has been drawn in the live engine; the `fits_engine` limits match `bv_gpu_raymarch.h` (`BV_GPU_MDL_DIM` 32 for x and y, `BV_GPU_MDL_DEPTH` 16 for z; read 2026-10-27), but the CPU path may allow larger models and was not checked. Sections 3, 5 (below the first three rows), 6, 7 and 9 are plans.
