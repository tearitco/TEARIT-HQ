# Terumon: evolution trees, shapeshifting, levelling (design, 2026-10-27)

Written after the owner said: terumon can "shapeshift through the evolutionary cycles of different creatures' evolution trees at will (it uses MP), and they can level up, evolve, etc.; document that stuff." Labels used throughout: **BUILT** = code exists and a harness has run it (named); **EXISTS** = in the repo today, not new; **PROPOSED** = my design, nothing written; **OPEN** = needs an owner decision. Companion: `TECH-TREES-AND-RENDER-RESEARCH-PIPELINE-DESIGN.md` (how a terumon learns a new form), `GROK-HANDOFF-2026-10-27-VISUALS-AND-ORGANIC-TRACK.md` (who builds what).

## 1. What a terumon is today (EXISTS, read 2026-10-27)
* Four terumon pals live in the owner's desk data (`pals/terumon_001_ember`, `_002_glacine`, `_003_murmur`, `_004_solvent`); each has `stats.pdl` (LEVEL init_lv / exp / exp_to_next, HP, STAT atk def mat mdf agi luk, DB link to an actor row), `meta.pdl`, `pal.pdl`, an inventory with a phone and a word bank.
* Levelling is the pet-trainer skill book (`@.apps/pet-trainer/skillbook.pdl`): every care action is a **skill** that costs **MP**, gives **EXP**, and EXP raises the **level** (first-guess numbers: `exp_per_level=8`, `base_mp=6`, `mp_per_level=2`, `max_level=12`). The op `entity_grade` applies use / rest.
* Looks change with level and habits through `@.apps/pet-trainer/evolution.pdl`: `STAGE | baby/child/adult/elder | level=N | scale= ear_len= body_rgb=` and `TRAIT | counter=play_total | per=3 | pin=ear_len | add=5 | cap=20`. This changes **proportions and colour of ONE body plan** (op `pet_gen`). It never changes the **kind of creature**. That gap is this document.
* Not found (searched `terumon`, `tech tree`, `evolution` across .md/.txt/.pdl on 2026-10-27): any "form", "shapeshift" or "morph" concept, and any 3D model per pet. The word `terumon` appears only in desk data.

## 2. The model in one paragraph (PROPOSED)
A terumon is **one entity with persistent identity** (id, owner, wallet, level, EXP, skills, memories) that is currently **wearing one form**. A **form** is a node in an **evolution tree**: it names a body plan (an anatomy part tree), stat multipliers, a model (voxels), and the rules for reaching it. Two different verbs move a terumon around the trees, and keeping them apart is the whole design:
* **Evolve** = *unlock* a form permanently. Slow, earned, rule-checked (level, exam passed, item, research done). Costs EXP/time, never MP. One-way: the form joins the terumon's **unlocked set**.
* **Shapeshift** = *wear* a form from the unlocked set, **at will**, paying **MP**. Fast, reversible, temporary. Costs MP to enter and MP per interval to hold; falling to 0 MP reverts to the **base form**.
"Different creatures' evolution trees" = several **trees** (canine, avian, aquatic, insect, mineral, machine...). Unlocking a form from another tree is a **research** item (see the tech-tree doc), so a terumon can eventually hold forms from many trees and switch among them.

## 3. Laws / choices / state (the house rule from `VISION-CLARIFIED` section 2)
| Kind | What | Where |
|---|---|---|
| **Laws** (hand-written, deterministic, never weighted) | shift cost arithmetic, "only unlocked forms can be worn", mass scales with volume (L^3), MP floors at 0 and reverts, a form's model must pass the hull consistency gate, a tree must pass `tree_lint` | ops + harness |
| **Choices** (weights in the concept bank, promoted by a person) | which form a wild terumon prefers in which situation, how a form's proportions are tuned, form names and words | concept bank, candidate edits |
| **State** (per entity) | `current_form`, `unlocked` list, MP, hold timer, base form, per-form EXP (optional) | `weights_<id>.txt`-style entity file (decision Q1), ledger rows for every change |

## 4. Data (PROPOSED; the row syntax is the one `tree_lint` already checks)
`forms.pdl` (one per game or world; trees are just roots):
```
NODE | egg      | Egg      | cost=0  | tier=0 | kind=base
NODE | pup      | Pup      | parent=egg | cost=5  | tier=1 | unlock_level=2
NODE | wolf     | Wolf     | parent=pup | cost=12 | tier=2 | unlock_level=6 | needs=pack_lore | model=forms/wolf/model.txt | hp_pm=1150 | atk_pm=1250 | agi_pm=1100
NODE | glider   | Glider   | parent=pup_air | cost=14 | tier=2 | needs=wing_lore
```
* `parent=` = the form it evolves from (a tree; one parent). `needs=` = research/knowledge prerequisites from the tech tree (a DAG; may cross trees). `cost=` = **MP to enter the form** (also the unit `closure_cost` sums). `tier=` = depth class; `--tier-monotone` makes a child outrank its parent and prerequisites. `*_pm` = per-mille multipliers on the entity's base stats (so a form never replaces stats, it scales them; layers multiply like the scope chain). `model=` = the voxel model.
* Per-entity file `state/form_<id>.txt`: `current=wolf`, `unlocked=egg,pup,wolf`, `mp=…`, `hold_since=…`.
* Rules as `SHIFT` rows so a world can tune them (scope chain, defaults in range): `SHIFT | hold_mp_per_min=1 | revert_cooldown_s=30 | min_mp_to_enter=cost | max_forms_unlocked=12`. **OPEN:** the numbers are guesses to tune, as in the skill book.

## 5. Verbs and events (PROPOSED)
Registry commands wrapping one op, `form_op`, so any event page or the player menu can call them (house pattern, `ev_chain` style):
* `form unlock <entity> <form>`: checks parent unlocked, `unlock_level`, `needs` satisfied (tech tree), then appends `FORM|unlock|entity|form|ts` and adds to the unlocked set. Refusals say which rule.
* `form shift <entity> <form>`: checks unlocked, MP ≥ cost, then spends MP (through the same skill-book path as every other MP use, so rest refills it), sets `current`, appends `FORM|shift|…`. Pieces and the 3D scene read `current` and draw that form's model (see section 8).
* `form revert <entity>` / automatic revert at MP 0.
* `form tick`: one slow op, like `life_op`: charges hold cost, reverts the broke. **Footprint rule:** idle cost ≈ 0 (a counter and a row), complex work only for the entity in focus (decision Q2).
Every change is a ledger row; nothing is auto-promoted; the harness is the judge.

## 6. Levelling and evolving (how this joins what exists)
* Level and EXP stay in the skill book (decision Q3: training happens only by use; deliberate practice is a costed skill use). New skills `shift_enter`, `shift_hold` give EXP like `play` does, so shapeshifting is itself trainable and the **MP pool grows with level** (existing `mp_per_level`).
* `evolution.pdl` STAGE/TRAIT stays as **within-form growth** (baby→elder proportions of the current form). A form may carry its own STAGE rows later; until then the global ones apply.
* **Unlock gates** reuse exams: `unlock_level` from the skill book; `needs=` nodes are passed in the pet's curriculum (`entity_grade` evidence), exactly as the "teach me" design in `@.apps/pet-trainer/TOWN-ECONOMY-AND-LEARNING-DESIGN.md` section 4 already defines for the tech tree.

## 7. Tree validation (BUILT)
`&.widgits/_shared-lib/ops/tree_lint.c` (build `build_tree_lint.sh`; harness `tree_lint.pal`, **53 checks, 0 failing**) lints any file of `NODE` rows: duplicate or malformed ids, unknown or self parents and needs, parent cycles, needs cycles, no/many roots (`--single-root`), depth (`--max-depth`), required keys (`--require cost,tier`), tier order (`--tier-monotone`), non-numeric cost/tier; every problem is listed in one run; `--path <id>` prints the ancestor chain and `--closure <id>` prints everything needed to reach a form plus its summed cost ("what does it take to be a wolf"). One linter for evolution trees, tech trees and anatomy part trees (this is the generic form of the roadmap's `part_tree_lint`). **Evidence:** 15 mutations of the source each fail the harness (duplicate ids allowed, both cycle checks off, tier rule weakened, closure cost not summed, depth not tracked, edges not counted, …). **Limit:** it checks structure, not balance; it cannot tell that a wolf is too strong.

## 8. Bodies and visuals (PROPOSED, depends on the other two docs)
* A form's **model** is the engine's voxel `model.txt` (`dim X Y Z` then `x y z r g b`), the same file the rpg-space asteroid seam and the `veh_N` pieces already use. Limits the GPU raymarcher imposes: X ≤ 32, Y ≤ 32, Z ≤ 16 (`views_to_voxel` reports `fits_engine`).
* Models come from **pictures or text grids of three views**, distilled by `views_to_voxel` (BUILT, 60/0 harness) and gated by its consistency number. How a terumon *obtains* those pictures is the research loop in the tech-tree doc.
* 2D parity (owner 2026-10-27): one scene description, two projections. The same form needs a 2D frame set (sprite sheet or a side view rendered from the model); the projection is the engine's job, not a second hand-made asset path.
* A **body plan** (anatomy part tree: bones, muscle, lungs, brain …) is what makes a form behave differently, e.g. wings enable flight. That is the `part_tree_lint` / anatomy v0 item on the roadmap; `tree_lint` is its linter. Until then a form changes **stats and look only** (cheap fidelity, the default).

## 9. Cheap and deep fidelity (decision Q2, applies here)
Default (cheap): a form is a stat multiplier set and a model; shapeshift is a row. Opt-in (deep): the form also swaps the anatomy part tree, and needs (air, food) follow the new parts. The world rule `fidelity_max` sets the ceiling; the saved truth is the per-entity weights and ledger, so switching fidelity loses nothing.

## 10. Build order, each step with a test that can fail
1. **DONE** `tree_lint` (structure of any tree). 2. **DONE** `views_to_voxel` (pictures → model, consistency gate). 3. `forms.pdl` + `form_op` (`unlock`, `shift`, `revert`, `tick`) with a pal harness: refusals (locked form, no MP, bad parent), MP arithmetic, ledger rows, revert at 0, idempotent tick, mutations. 4. Registry commands + one event page; a terumon in rpg-dust wearing a placeholder cube per form (proves the draw path before any art). 5. Research loop writes candidate models (tech-tree doc). 6. Anatomy-backed forms (deep fidelity). 7. A promotion path for tuned `*_pm` weights through the concept bank.

## 11. Open decisions for the owner
1. Is shapeshifting **temporary** (MP upkeep, revert at 0; my proposal) or a **one-time permanent change**? The design above supports both by setting `hold_mp_per_min=0`.
2. Can a **wild or purchased** terumon start with forms already unlocked (store items, chain-owned NFTs) or must every form be earned by that individual? Affects the Store and the pet chain economy.
3. May forms from **other players'** terumon be learned by observation (research by watching)? Parked until the research loop exists.
4. Cap on unlocked forms per terumon (`max_forms_unlocked`, guess 12).

## 12. Not verified
Nothing in sections 4-6 and 8-9 has been built or run. The only measured facts here are the repo facts in section 1 and the two BUILT items with their harness counts. No form has been drawn in a live window.
