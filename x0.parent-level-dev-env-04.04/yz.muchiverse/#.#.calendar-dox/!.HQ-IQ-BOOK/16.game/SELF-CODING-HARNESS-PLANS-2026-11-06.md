# Self-coding harnesses that learn to make things from the word banks

2026-11-06. Six plans. Decision D46. Nothing here is built except Plan 1's first five phrases and the gate they already call (`campus_look.pal`, 9/0). A learner writes data. The ops we have run it. A model does not edit C, does not promote its own row, and does not train a diffusion network on this machine.

Owner copy: `/home/no/Desktop/github/work/XO/31.Nov/nov-6/0.self-coding-harness-plans-2026-11-06.md`

## Shared contract

One line a learner may emit. Anything else is a failed expect.

```
PROPOSE | kind=assoc|part|phase|cells|molecule | id=<token> | body=<one row of that kind>
```

| kind | body is | who runs it |
|---|---|---|
| assoc | nine look numbers in [-1, 1] and a phrase | `hidden_layer apply` then `train` |
| part | a named stamp already in `centroid_parts_op` (`floor`, `wall`, `door`, `roof`, `bridge`, later `stair`, `tower`) | `apply <part>` |
| phase | one phase-table row the playthrough op already understands | `playthrough_op` |
| cells | a `cells.txt` no wider than 8 and no taller than 8, tokens `floor,wall,depth` | the maker reload, then a count |
| molecule | a formula plus a placement of element ids in that same 8×8 | the molecule check below |

A local model, when one is used, may only fill that line. Gemma describes. It does not classify, and it does not type the weight (`HORN-HALO-IRL-IMPLEMENTATION.md`: DESCRIBE never CLASSIFY). Code parses the line. A weight outside [-1, 1], an unknown kind, an element id outside 1..118, or a grid over 8 is exit 2 before any stamp.

The loop, every plan:

1. Proposal lands in a candidate file. The tracked bank is not opened for write.
2. Scratch house. Never the live centroid page unless a person asked for that click.
3. The existing op runs.
4. A judge that can fail writes PASS or FAIL.
5. `harness_bank_op` adds that row to reward or punish and sets `WEIGHT = (reward+1)/(reward+punish+2)`.
6. Promotion is a different person from the proposer. `find` ranks. It does not run the winner (D45).

Local machine, what "ML" means here:

- Word vector: mean of the accepted phrases that contain the word (`hidden_layer train`). Nine look keys when `HL_KEYS=warm,wet,wear,grain,edge,round,metal,big,worn`. The 3-key `energy,force,motion` table stays byte-identical and is not this learner.
- Attention: `s = q·k / sqrt(9) + bias + 0.5 * word affinity`, softmax at temperature 0.15, top 2 (`gate_op.c`). Deterministic. Word order does not change a digit.
- Optional later fit: M5 in `MATERIAL-ATTENTION-AND-EXPERT-MIX-GENERATOR-DESIGN-2026-11-05.md`, gradient descent in C on a few hundred bias numbers, output is a candidate file. Held-out approved rows must rank above a shuffle.
- Not on this machine: torch, a trained diffusion net, a CNN, tomom linked into the studio, an FSM whose only job is to score a picture. tomom's `attention.c` is the shape of a later small net (IRL bootstrap layer 4), run on a curriculum a cheaper router already picked.

In-house vision, in order, cheapest first. A neural net is not step 1.

1. Counts: width, height, number of non-air cells, adjacency (4-neighbour).
2. Identity: element id, material name, door bit, bridge cells that are `1,0,0` over `0,0,0`.
3. Colour: mean RGB of a cell against the Drude colour of its element (`elements` palette, 118 colours). A distance over a pinned epsilon fails.
4. Period: `tile_period_op` on a sheet. A wrong period fails.
5. A frame, only when the claim is about the pane: the overlay raw, a pixel bound, a hash. Not "newest file on disk".

## Plan 1. Hand tune the look table until the gate names the thing

Status: started. Five phrases. Stone, roof_red, grass, timber, stone. Wear on the wall moves the second material from brick_brown to rust.

Next rows, still hand-set, still the same case shape: stair stone, tower stone, wood table, wheat stalk, dog, a person's coat. Each new `TOP2` line is pinned. A silent flip fails.

Scale: the tenth phrase is another `ASSOC` and another expect. No new op. This plan does not place a voxel.

## Plan 2. The gate names a part, and the part op places the voxels

The event page, or the harness, runs `gate_op` on the phrase and writes `TOP2` to a one-line file. A new verb `centroid_parts_op apply-named <material> <cells>` is not a second stamp. It calls the stamp that already exists for that part, and it checks the material id is one of floor, wall, door, roof, bridge.

Proof: two phrases, one desk in scratch.

- `weathered stone wall` must leave a wall cell whose material file says stone.
- `wood door` must leave a door event and a timber line.
- Swap the phrases and the old material line must be absent. That is the negative.

The live centroid page is not the scratch desk. Wall slot 36 stays until this scratch proof is green and a person asks to point the live page at it.

Scale: stair and tower are new part names only after each has a `PART` row and a harness. The gate does not invent a part the op cannot stamp.

## Plan 3. Self-coding means a proposed row, compiled by the harness

The coder is the loop, not a model with a text editor.

A candidate file holds one or many `PROPOSE` lines. A C op, `propose_check`, is the only new code in this plan, and it is written by us once:

- Reject a bad line (exit 2).
- Copy accepted bodies into a scratch `cells.txt`, `phases.pdl`, or `assoc` draft.
- Exec the op named by `kind`.
- Diff the stdout against the expect file that shipped with the task, not against an expect the proposal wrote. A learner that writes its own expect can pass by changing the test.

Reward: PASS adds 1. FAIL adds 1 to punish. Twenty failures on one id and the weight falls under 0.2. `find` stops offering it. The row stays in the candidate file until a person deletes it.

What this is allowed to "code":

- A phase: `mine stone`, `craft door`, `place door`, using verbs the playthrough table already has.
- A cells patch inside 8×8.
- A molecule placement (Plan 5).

What this is not allowed to code: `bv_render_3d.c`, the camera, `keybinds.pdl`, `generator_tile_op` internals, a new `.py`, a new `.sh` that holds logic. A later plan may emit a unified diff as a candidate. `cpu_lint` and `spawn_lint` must not gain hits. A person applies it. That plan is not this one.

## Plan 4. Vision checks a small object without a network

Each small object has a judge row, not a picture model.

```
JUDGE | id=wood_door | cells=8x8 | door=1 | material=timber | mean_rgb=~timber | air_under_bridge=0
```

The op paints or stamps, then measures. Failures are separate expects so a red run names which fact broke. Palette snap: if the mean colour is farther than epsilon from the material colour, FAIL, and the proposal is not retried with a looser epsilon inside the same run. Epsilon is data, changed by a person.

A diffusion draft is a second judge, off by default. Same phrase, two weight sets, the pictures must differ when the weights differ, and both must pass the count judge. Host is the img-hai Draft already measured at about two minutes a frame on CPU (2026-10-13). One draft at a time, not while a board engine runs. The PNG goes to a candidate folder. `0.good/` stays empty.

## Plan 5. Molecules and other tiny voxel objects

A cell is one voxel. `floor,wall,depth` is the maker record. For a molecule the wall token is the element number 1..118. Depth 0. The colour is the Drude colour of that element, the same 118 colours the elements palette already shows. Bonds are 4-neighbour contacts. There is no molecular-dynamics step and no force field. The check is the formula and the contact list.

Seed table, hand-written, then a learner may only propose another placement of the same formula:

| id | formula | cells | contacts that must exist | a placement that must fail |
|---|---|---|---|---|
| water | H2O | 3 | each H touches O, the two H do not need to touch each other | three H in a row |
| salt | NaCl | 2 | Na touches Cl | two Na |
| methane | CH4 | 5 | each H touches C | a bond between two H and no bond to C |
| quartz | SiO2 | 3 | each O touches Si | Si alone |

The harness counts element ids, checks contacts, and checks mean colour against Drude within epsilon. A proposal that adds a fourth element fails even if the picture looks right.

The same 8×8 budget covers the other small items, with a material id instead of an element id:

- a blade of grass: one column, depth 1, material grass
- a brick: one cell, material brick_red or brick_brown
- a stool: four cells, timber
- a cup: a ring of cells, air in the middle
- a short stair: three cells of rising depth, stone
- a door slab: the door event we already stamp, inside the 8×8, not a new mesh

A pet later gathers by the formula: the phase says `need H 2, O 1`, the bank weight of that phase rises when the molecule check passes. The pet does not invent the element.

## Plan 6. The event page is the actuator, and the bank learns which page worked

One new page, `events/gen_material/make.pal`, three execs:

1. `gate_op gate` on the phrase stored in the page's `words.txt`.
2. `centroid_parts_op apply` for the part the top material maps to, scratch cells only.
3. `propose_check` or the existing parts harness expects.

The page is the same shape as `events/gen_characters/event.pal`: exec, then halt. No plugin list.

Learning across pages uses the behavior bank, not a new attention net. Each page has `KEYWORDS` and a slot weight (aboutness, edited only by a person) and `COUNTS` (reliability, edited only by the ledger). After real runs, `find make door` should rank the door page above the roof page. The hand tune now is those keyword rows, the same recommendation as `WEIGHTS-ATTENTION-HARNESSES-AND-EVENTS-2026-11-06.md`. Inverse reinforcement learning, when a person keeps or throws a result, reuses the promote ledger and the next-line correction in `IRL-BOOTSTRAP-RECURSION-SPEC.md`. It does not get its own learner in this plan.

## Order

1. Plan 1 phrases for stair, tower, plant, animal, person. Pin each `TOP2`.
2. Plan 5 seed table for water, salt, and one brick, with failing negatives. No learner yet. This proves the judge.
3. Plan 3 `propose_check` on those seeds. A bad line exits 2. A good water placement passes. A three-H placement fails.
4. Plan 2 scratch stamp driven by the gate, one wall and one door.
5. Plan 6 one event page that calls 2 and 3.
6. Plan 4 colour epsilon, then, only if a person asks, one diffusion pair.

Stop conditions: a plan that needs a new neural runtime, a write into `0.good/`, a camera write, or a second copy of the autotile table is the wrong plan. Remove it rather than grow it.

## External references

- This file.
- D46 in `02-architecture/DESIGN-CHOICE-TEST-AND-DECISION-LOG.md`.
- D45, D35, and the material design `MATERIAL-ATTENTION-AND-EXPERT-MIX-GENERATOR-DESIGN-2026-11-05.md`.
- Gate and parts: `44.xyz.01.00/@.apps/generator-hq/ops/gate_op.c`, `44.xyz.01.00/@.apps/centroid/ops/centroid_parts_op.c`.
- Look data: `44.xyz.01.00/&.widgits/concept-bank/data/hidden/look_assoc.campus.*`.
- Harness weights: `08-roadmap/design-docs/HARNESS-BEHAVIOR-BANK-DESIGN.md`.
- IRL: `08-roadmap/design-docs/IRL-BOOTSTRAP-RECURSION-SPEC.md`, `HORN-HALO-IRL-IMPLEMENTATION.md`.
- Where weights already are: `16.game/WEIGHTS-ATTENTION-HARNESSES-AND-EVENTS-2026-11-06.md`.
- Elements palette: `08-roadmap/TILESETS-EVENTS-AND-GAME-CLONES.md` row `elements`.
