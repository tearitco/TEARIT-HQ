# Tutorial harness, onboarding gamification, and the near-term drive/animation/possession fixes (2026-11-26)

Written 2026-11-26 from the owner's message of the same day. Branch `claude`, nothing pushed. Labels: **BUILT** = a harness name or a dated OBS backs it, **SEEN** = looked at on screen, **CHECKED TODAY** = I read the code today, **DESIGN** = nothing built.
Companion docs: `RMMC-RECREATION-PLAN-ASA-ON-THE-ISLAND-VOX-EVENTS-AND-THE-FRAME-FOR-FRAME-VIDEO-2026-11-25.md` (the island, the video pipeline, backup/alpha), `RMMC-PLAY-ON-THE-VOX-LEDGER-2026-11-25.md`, `02-architecture/STANDARD-RULES-AS-OPS-FOUNDATION-DESIGN-2026-11-18.md`, `17.ai/META-STRATEGY-ATTRITION-HARNESS-TOOL-USE-AND-CHEAP-BUILDING-2026-11-22.md`.

## 0. The owner's idea in one paragraph (his words, kept)

Every tutorial is done twice: first the harness (an FSM) drives the entity while on-screen words say what is happening, exactly like the video; then it hands over and the human does the same steps. The engine scores the human's input against the same steps; the tutorial is "checked" when done; a badge is stored in the user's account and can be viewed. Tutorials live as a list in the rpg-edit book/pages, are reusable events/layouts, accumulate like lessons in an online school, and are done at the user's own pace. If the human has trouble, the session is shown to me afterwards. "That is what stops this from being an abstract house." Because I present and then read the human's input, the tutorials double as UI/UX testing and debugging.

## 1. What exists today (observed, with where)

| Thing | State | Evidence |
|---|---|---|
| The island, vox ledger fold, verbs (walk, face, jump, mine, place, craft, give, bag, seat, eat, sleep...) | BUILT | harness `vox` 361/118/27, `rmmc_verbs` 51, `rmmc_seat` 47, `rmmc_project` 64, `rmmc_biomes` 45 |
| Asa as a skinned voxel rig, hands, trees as events, break cracks and chips | BUILT; skin SEEN; hand colour NOT seen live; chips NOT seen on screen | commits `be0e403b4`, `281b31af8` |
| Hotbar number keys 1..9, page flip 0 | BUILT (`rmmc_pick_op __slot`, `keys.pdl`) | commit `f858c54ef` |
| POV keys 1..4, alias keys `p`/`o` for first/third person | BUILT, verified with real keys | commit `13153dfbc` |
| Possession: Enter on an entity's cell (`key_possess_commit` 13), key `9` | BUILT (`bv_menu_input.c` 606-607) | CHECKED TODAY |
| **Shift switches possession between entities** | **NOT FOUND.** `bv_menu_input.c` reads Shift only for the camera keys W/A/S/D (line 966). I grepped `cycle`, `next`, `party_next`, `poss_next`: nothing. The owner may remember the `m` move/reorder mode (`HOW-TO-BUILD-M-MOVE-MODE-AND-REORDER-HANDOFF-2026-11-24.md`) or Esc releasing possession. So Shift-cycling is a NEW feature, not a regression | CHECKED TODAY |
| Release possession | Esc releases possession first, then leaves In (memory: project-esc-backs-out-one-level) | memory only; re-verify |
| Ava in the level / on the minimap | **NOT PRESENT.** `seed/events.txt` holds only Asa and 5 trees; `rigs/` has `rig_asa*.pdl`, no `rig_ava` | CHECKED TODAY |
| Animation | Two poses. `bv_rig.h` header: "Boxes cannot rotate in this raymarcher, so a swing is NOT an angle: the limb box is OFFSET along the facing axis, stepped". Walk = `(frame + (pattern==2)) & 1`, frame = wall clock / 140 ms; patterns 0 idle, 1/2 walk, 3 jump, 4 mine, 5 place | CHECKED TODAY |
| Tutorial videos (two languages), frame-for-frame director, narration as `.pdl` | BUILT, SEEN | `XO/31.Nov/nov-25/9.rmmc-user-tutorial-both-languages/` |
| Harness-drives-entity (FSM) | BUILT in pieces: `rmmc_driver`, `beats.pdl`, `shots*.pdl`, `rmmc_director` | harness `rmmc_beats` 39 |
| On-screen instruction text in the game, tutorial list in the book, scoring of human input, badges | **NOT BUILT** | DESIGN below |

## 2. Super near term (this sprint, in this order; the owner proves each before I move on)

**Gate rule (owner): do everything possible in the tutorial video, then STOP and let him do the steps. I do not start the next item until he says he has done the current one.**

1. **Human can drive the player (the proof tutorial).** Needs: one key to take control of Asa (today: park the cursor on Asa's cell and press Enter, or `9`; untested as a one-key macro), WASD/arrows walk, space jump, mouse/click mine and place, `1..4` POV, `p`/`o` first/third person, number keys pick a slot, Esc releases. Video `T01-drive-asa` shows the keys on screen; the human repeats it. Accept: the human's own session writes the same ledger rows as the video (section 5 scoring).
2. **Animation more like Minecraft ("under-rated now").** The cause is a hard limit: boxes do not rotate, so a limb can only slide in two poses. Options, best first (design-choice test, section 6):
   - **A. Data-driven pose table, more poses (recommended first).** A `rig_anim.pdl` of `POSE | pattern | step | part | dx | dy | dz` rows, 6 to 8 steps per walk cycle, arm and leg offsets following a sine, a body bob, head turn toward the target, an idle breathing bob, a swing with a wind-up and follow-through. No engine C change beyond reading the table; every number tunable as data, like the skin keys. Cheap, reversible, looks like stepped Minecraft limbs.
   - **B. True rotation: oriented boxes in the raymarcher.** One yaw/pitch per box, transform the ray into box space. Real swing angles, but a change in the shared GPU path (`BV_GPU_MAX_BOX 128` budget, shader cost on a weak CPU/GPU machine). Do only if A looks wrong after the owner sees it.
   - **C. Sheared limb stacks** (a limb is two or three boxes at offsets that fake an angle). Middle path if A is not enough and B is too costly.
   Accept: a walk of 8 steps in the harness (`rmmc_project` rows) plus a side-by-side video clip the owner approves. Existing mutant fixture `BV_RIG_MUTANT_LEGS_IN_PHASE` must still turn the walk check red.
3. **Add Ava to the level and the minimap, so the owner can find and possess her.** Ava is a woman, Asa a man (memory: project-asa-male-ava-female). Needs: `rig_ava.pdl` + `rig_ava.skin.csv` from her real sprite through `rig_from_charset_op`, a `rig:ava` row in the seed `events.txt` (on `rmmc-alpha` first, not the stable page), a minimap marker (the minimap click already honours `de_possessible`). Accept: she appears on screen and a click on the minimap possesses her.
4. **Release and switch possession.** Esc releases (verify). **Shift (and Tab as a second key) cycles to the next possessable entity** by the order of the book's party/entity list, one new KEY row `possess_next`, same shape as the alias rows I added (`KEY | possess_next | <code>`), read in `bv_menu_input.c`, writing `possessed_id` through the existing xelector path, never a second authority. Ask the owner before taking Shift (it is the camera's W/A/S/D modifier today).
5. **Tutorial 02 `T02-release-and-switch`** (release, find Ava on the minimap, possess her, Shift to cycle back). The owner does it; I read his session.

## 3. Mid term: the tutorial engine (one reusable system, not an rmmc feature)

### 3.1 Parts (all data + ops, no per-game C; D92)
| Part | Shape | Reuse |
|---|---|---|
| **Tutorial = a page** in a book `tutorials` (rpg-edit book/pages), made of ordered **STEP rows** | `.pdl` | any game, any app: a step names an op/verb, not a game |
| **STEP row** | `STEP | n | say | expect | hint | timeout` where `say` is the on-screen words (en + ja columns, same as `tutorial_narration.pdl`), `expect` is a ledger/state predicate, `hint` the nudge after a pause | the video and the in-game text read the SAME rows (one authority) |
| **Presenter** (the FSM that drives the entity) | an FSM page that issues the same verbs a human would (`vox_op walk ...`), with `say` shown as an on-screen caption entity | `rmmc_driver` already does this for Asa; generalise to "drive actor X by STEP rows" |
| **Hand-over** | one ledger row `TUTORIAL | id | HANDOVER | actor` then the human's input flows | possession/ledger already exist |
| **Checker op** `tutorial_check_op <steps.pdl> <ledger> <cursor>` | pure op: reads the ledger rows after the hand-over and answers `STEP n PASS/FAIL/WAIT` | a harness, an FSM node, an AI tool call and the engine can all call it (scorable reuse) |
| **Score and badge** | an append-only row in the user's own ledger `BADGE | tutorial_id | score | date | session` | viewable list |
| **Session recording** | the human's key/ledger stream is already files (`entity_menu_history`, ledger): keep it as the "show me the session" file, capped (RUNTIME-FILE-GROWTH-STANDARD) | I read it after |
| **Tutorial list UI** | a page in the rpg-edit book listing tutorials with done/not done from the BADGE rows, level order via `REQUIRES` rows (accumulates like school lessons) | same page type as any list |

### 3.2 Where the badge lives (ask the owner)
The owner said "stored in your user account". `xyzfs/users` is live data and I never touch it; a badge row needs a verb that writes it, not my hand-edit. DESIGN question for the owner, with my recommendation: **a `BADGE` row in the book's own ledger keyed by user id, mirrored later to the account by the existing save-data path**, so one authority per fact and no private data in code history.

### 3.3 Harness levels (so "we will know there are no bugs")
1. Pure op case: `tutorial_check_op` against a hand-written ledger (PASS/FAIL/WAIT), with a sabotage case that must turn it red.
2. FSM presenter case: the presenter completes the steps and the checker passes (the robot is the first student).
3. Live on-screen case: assert the caption text IS on screen (`relay_drive see`), open the PNG.
4. Human round: the owner does it; I read the session file and write down every deviation as a bug or a missing hint. This is the UX test.

## 4. Long term: what the owner asked to plan (reusable for other games)
- The tutorial engine above takes a **target app** as one row (`APP | rmmc`), so Teropoly, Teario and the editor get their own tutorial books without code.
- **Gamification** is only ledger rows: `BADGE`, `XP`, `LEVEL`, `STREAK` derived by a fold (never stored as a counter in memory; no-ghost-data rule).
- **Self-paced curriculum:** `REQUIRES | tutorial | other` rows give the school-like tree; a "next" button picks the first unlocked, not done tutorial.
- **Survival basics roadmap** (day/night, planting, animals, mobs, shelter, doors, lights, ores, furnace, recipe popup, break difficulty, health/hunger/death, chests, beds, water, fall damage) comes after the drive/possession tutorials: each feature gets its own tutorial in this engine, so every feature ships with its instruction video AND its in-game lesson. That roadmap's design note is a separate document (to write next) and builds on `rmmc-alpha`.
- Videos stay generated from the same STEP rows (Mac pipeline), so words in the game, the video and the test never drift.

## 5. How a human's input is scored
The checker reads **ledger rows**, not key presses: "walk to the tree" passes when a `MOVE`/`WALK` row puts Asa within 1 cell; "mine a log" when a `MINE` row for log exists after the hand-over. Key presses are kept only in the session file for my review. A step that needs a UI action with no ledger row (open a menu, change POV) uses a **preference or state marker row** written by the same op that did it (a verb row, `UI | pov | 1`), never a hand-edited file.

## 6. Design-choice test (five questions; also logged as D139)
Question: where does the tutorial/onboarding system live, and how does a presenter teach?
- **(A) Per-game scripted demo in the rmmc driver** (what exists).
- **(B) One generic tutorial engine: STEP rows + presenter FSM + checker op + BADGE rows, per-app only as data (recommended).**
1. One authority per fact: B (steps are one file read by video, caption and checker; the ledger decides pass/fail). A duplicates the steps in shots, narration and tests.
2. Modular: B (an op and data; no per-project C). A is a per-game script.
3. Scalable (the 10th consumer): B is one `APP` row. A is a new script per game.
4. Long term: B is where the roadmap points (harness verdict as the loss, an FSM drives, human-reviewed promotion).
5. House shapes: B is `.pdl` + op + event page + harness case + capped session file.
Cost and risk of B: a new op and a hand-over rule; first slice is small (3 STEP rows for T01).
**Recommendation: B, first slice = T01 `drive-asa` on `rmmc-alpha`.**

## 7. Risks and what is NOT verified
- Shift for possession cycling is unverified and may collide with camera Shift+W/A/S/D.
- Hand colours do not show live; chips never seen on screen.
- `rmmc-alpha` shares the vox rules/blocks/heightmap with the live page until the data directory is overridable (do that before any rule experiment).
- Oriented boxes (animation B) would touch the shared GPU path; not started.
- No harness case yet for skin keys, number keys or alias keys.

## 8. Order of work and gates
1. Doc, D139 row, index row (this commit).
2. T01 drive: confirm the one-key take-control, write STEP rows, make the video, **wait for the owner to do it**.
3. Animation option A as `rig_anim.pdl`, side-by-side clip, **owner approves**.
4. Ava on `rmmc-alpha` + minimap, **owner finds and possesses her**.
5. `possess_next` key (after asking about Shift), tutorial T02, **owner does it**.
6. `tutorial_check_op` + harness case; in-game caption; BADGE row; list page.
7. Survival-basics roadmap note and its tutorials.

## 9. Owner decisions 2026-11-26 (later the same day) and the robust part/animation data structure

**Possession key: Tab was NOT free, checked in code.** Tab (raw code 9) is already two things: (a) the engine's side-scroll POV 5 (`key_pov_5 = 9`, `bv_menu_input.c` line 630) and (b) the editor row `KEY | control_switch | 9  # Tab: INT drives the selected pet <-> the player` (`@.apps/rpg-edit-hq/keybinds.pdl:9`). So Tab already means "switch who INT drives" in the editor; the new `possess_next` must either REUSE `control_switch` (preferred: one authority, one meaning) or take another key. Decision pending the owner; Shift stays the camera modifier.

**Owner ruling: rotation and all animation live in `.pdl`, and the data structure must not paint us into a voxel-only corner** ("we get bit when we get far in game dev of voxel games and want other shapes the voxels cannot do; scaled voxels can represent spheres depending on resolution"). So the animation work (section 2, item 2) is no longer "more offset poses": it is a **node tree with transforms**, versioned, with the box slide as only the first renderer of it.

### 9.1 The model: PART nodes (a scene graph in rows)
`rig_<name>.pdl` schema 2 (a `SCHEMA | rig | 2` row; a file without it loads as today, every new key optional with the old behaviour, same rule as `rig_convert.pdl`):
```
PART | name | parent | shape | res | pivot_x pivot_y pivot_z | pos_x pos_y pos_z | rot_x rot_y rot_z | scale_x scale_y scale_z | alpha | material
```
- **parent**: relative position and rotation compose down the tree (head on body, hand on forearm, held item on hand); `-` = the actor root.
- **pos** in block units RELATIVE to the parent's pivot, **rot** in degrees (euler order fixed and written in the schema), **scale** per axis (squash and stretch, growth, "bigger Asa"), **pivot** so a limb swings from the shoulder, not its centre.
- **shape** is a name, not a hard-coded box: `box`, `voxel` (a model grid, the skin we have), `sphere`, `capsule`, `cylinder`. Analytic shapes are tiny in the raymarcher (a distance function each); `voxel` carries `res` = grid size per axis (8, 16, 32): **a sphere is `voxel` at `res` N with a filled-radius rule, or `sphere` exact; the same row either way**, so quality is a number (resolution) and a pdl edit, never a code change.
- **alpha** 0..255 per part (ghost, glass, fade-in on spawn, water). `material` is a name looked up in a small `MATERIAL` table (emissive for torches/lava, flat, leaf-dither), so lighting needs no new column later.
- Reserve columns now, even before the renderer reads them: a part whose row has fields the renderer does not know yet is **kept and ignored, never an error** (forward compatibility), and a `PART_EXTRA | name | key | value` row carries anything later.

### 9.2 Animation: tracks of keyframes over those channels
```
ANIM | name | length_ms | loop
TRACK | anim | part | channel | t_ms | value | ease
```
- channels: `pos_x/y/z`, `rot_x/y/z`, `scale_x/y/z`, `alpha`. Eases: `linear`, `sine`, `step`, `ease_in`, `ease_out`.
- A **state row** `STATE | actor | pattern | anim | blend_ms` maps the existing patterns (0 idle, 1/2 walk, 3 jump, 4 mine, 5 place, plus later eat, sleep, hurt, swim) to animations, so verbs and the ledger do not change. Blending two anims is a weighted sum of channels (data, not code).
- Minecraft feel = walk cycle `rot_x` on arms and legs as a sine of the walk phase, a head that looks at its target, a body bob, a swing with wind-up and follow-through: all of it TRACK rows.
- Procedural helpers (look-at, walk phase from speed) are ops that WRITE rows or per-frame channels from a rule row; they are not baked into the renderer.

### 9.3 How the renderer gets there without a rewrite (honest limits)
Today `bv_rig_boxes_ex` returns axis-aligned boxes and `ADDBOX` takes min/max corners; the raymarcher can scale a voxel grid to a box but cannot rotate a box (`bv_rig.h` header). Three stages, each shippable:
1. **Stage 1 (data first, no engine change):** the node tree, tracks and states are parsed and **evaluated on the CPU** to world transforms; rotation is approximated for the existing renderer by a chain of 2 to 3 offset boxes (option C of section 2); scale and alpha-as-threshold work today. The data is already final.
2. **Stage 2 (engine, shared path, owner approves first):** add an optional per-box orientation (a 3x3 or a quaternion) to the GPU box record; the raymarcher transforms the ray into the box's local frame (a box and a voxel-model grid both work the same way). `BV_GPU_MAX_BOX 128` is the budget: a rig of 12 parts x a few actors must be counted and measured on the weak machine, never guessed.
3. **Stage 3:** analytic `sphere` / `capsule` / `cylinder` primitives and real per-part alpha blending (alpha needs a second pass or an ordered march; costly, so it is behind a rule and off by default).
Harness: an op `rig_pose_op <rig> <anim> <t_ms>` prints the evaluated world transforms; cases check a known pose (arm at 45 degrees at t = quarter cycle), a parent move carries its children, schema-1 files load unchanged, and the old mutant (legs in phase) still turns the walk check red.

### 9.4 Why this shape (design-choice test, folded into D139)
One authority: the rig file is the only place a part's geometry is stated, the skin CSV only fills a `voxel` part's grid. Modular: transforms, tracks and states are data; the renderer only reads the evaluated list. Scalable: the 10th creature (sheep, creeper, spider) is a new rig file and animations, no C. Long term: sphere/capsule/alpha/rotation arrive as stages without a file-format break because the columns exist from day one. Cost: a parser, an evaluator op and a harness case first; the engine orientation change is gated on the owner seeing stage 1.

## 10. Owner answers 2026-11-26 (asked in chat)
- **Next-entity possession keys:** `[` and `]` (previous / next), as `KEY | possess_prev | 91` and `KEY | possess_next | 93` rows. Tab keeps its meaning (editor `control_switch`, engine POV 5).
- **Take control:** keep Enter on the entity's cell, and ADD "Enter anywhere" (possess the selected/nearest entity). Which one is active is a `.pdl` KEY/RULE row, not code.
- **Release:** both Esc and a dedicated key for now; the owner will pick. Note (READ, not run): `9` is the engine's release key (`key_possess`), but rmmc's `GAMEKEY | slot9 | 57` is the same code and GAMEKEYs are consulted before the release handler, so with Asa possessed `9` most likely picks hotbar slot 9 and does NOT release. Verify with a real key before relying on it.
- **Badge:** a `BADGE` row in the book ledger, mirrored to the account later.
- **Captions:** English by default with a switch (Japanese one setting away); both live in the same STEP rows.
- **Ava while unpossessed:** a small, customizable FSM (data-defined) that will also carry reinforcement-learning features later (rows in, harness verdict as the score; promotion human-reviewed per the meta strategy).
- **Animation order: OPEN.** The owner wants to SEE the `.pdl` and understand how real rotation aligns with or deviates from the engine's principles, and whether anything hinders making more AAA-like games, including physics-style scaled simulation. Answer given in chat: the rows stay data and ops stay the rule owners; real rotation adds one orientation to the shared GPU box record (a bounded change to a shared path, needs owner OK and a measured frame cost); voxel/primitive scenes scale to Minecraft/Teardown class bounded by the box budget; mesh/skinned characters would need a different render path; physics fits as an op that WRITES pose channels (pos/rot/scale/alpha) into the same rows, so the renderer never needs to know.

## 11. DECISION 2026-11-26: animation goes to real rotation (option B); stage 1 is only a stepping stone (D140)

Owner: "b sounds like the option since this should evolve into a robust game engine, however does it respect our in-file truth and house design?" then "then that's what we must do now. explain all of that in the doc before we continue." This section is that explanation. Nothing below is built yet.

### 11.1 The two options in plain words
- **How the picture is made today:** a GPU raymarcher shoots a ray per pixel and asks which box it hits first. A box is two corners (min, max), so it is always lined up with the world axes. A skinned limb is a small grid of coloured cubes stretched to fit one box. There is no angle anywhere in a box description, which is why Asa's arm cannot tilt.
- **A, stepped poses (the stepping stone):** a limb stays upright and moves in 6 to 8 small steps. Looks like a toy soldier; cheap; no shared-engine change. A tilt can be faked with 2 to 3 offset boxes per limb, but that eats the 128-box budget (`BV_GPU_MAX_BOX 128`), so A has a hard ceiling. A cannot do a head that tilts toward a target, a sword arc, a creeper lean, spider legs, an opening door.
- **B, real rotation (chosen):** each box gets an orientation; the raymarcher rotates the ray into the box's local frame and tests the same axis-aligned box there. A voxel skin works the same way, so the grid rotates with the limb. Looks like Minecraft; costs more maths per box per pixel (NOT measured on this machine; the live GPU path was about 17 fps, so a slowdown would show there); touches a shared render path.
- **Same data either way:** `rot_x/y/z` are in the `PART` rows from day one. In stage 1 the evaluator turns a 35 degree arm into the nearest offset boxes; in stage 2 the same row becomes a real rotation. Nothing is thrown away. The one thing that would break this is storing positions instead of angles in the file, which we do not do.

### 11.2 Does B respect "in file truth" and the house design? (checked against the rules, a design conclusion, nothing run)
A rotation angle is not a game fact. It is a pure function of three things: the rig rows, which animation the ledger says is playing, and the clock. A pure function of file truth is a **projection**, not ghost data: the same status as today's 140 ms wall-clock step.
| House rule | B |
|---|---|
| One authority per fact | Yes if the rig `.pdl` is the only place a limb's geometry and angles are stated; the skin CSV only fills a `voxel` part's grid |
| Ledger is the only state authority | Yes: the ledger and event rows say WHICH animation plays (`pat`, `STATE`); the angle at time t is derived, never stored as state; the rules hash is untouched |
| No ghost data | Yes for time-driven animation. CAUTION physics: see 11.4 |
| Every rule is an op, nothing hard-coded in the engine (D92) | Mostly: the evaluator is pure maths (rows in, transforms out); the renderer only draws what it is handed and holds no game rule |
| No per-project code in shared files | Yes: orientation is generic; an unrotated box stays byte-identical |
| Sharing C: text-include for pure code, never header plus link | Needs condition 1 below |
| Marker-file change detection, capped runtime files | Yes: B adds no runtime file and no polling |
| A harness that can fail | Yes if built as 11.3 |

### 11.3 The four conditions (B is only accepted with all four)
1. **One evaluator, not two.** The renderer and the test op `rig_pose_op` use the SAME pure C file, text-included into both (the house pattern for pure shared code). Two implementations would be two authorities and a test could pass while the screen drifts.
2. **Identity by default.** A box with no orientation row renders exactly as before. A harness case compares frames against the current engine to prove it; the existing mutant `BV_RIG_MUTANT_LEGS_IN_PHASE` must still turn the walk check red.
3. **Schema versioning, forward compatible.** `SCHEMA | rig | 2`; unknown columns are kept and ignored, so stage 3 (sphere, capsule, alpha) never breaks old files.
4. **Physics writes through rows** (11.4).

### 11.4 Cosmetic versus state (physics, ragdolls, vehicles)
- **State** = anything that changes WHERE a thing is or what it can do (a pushed crate, a thrown item, a vehicle, a ragdoll that ends up on a cell). It must go through ledger rows, or be a deterministic fold of them, otherwise replay cannot reproduce it: that would be ghost data.
- **Cosmetic** = purely visual (a swaying branch, a cape, a tail, breathing). It may stay a projection computed from rows plus the clock, never written back.
- A scaled-simulation op may write pose channels into the same `TRACK`/channel format; the renderer never needs to know it is a simulation. That is why the data format matters more than the renderer.

### 11.5 What B changes, honestly
- The shared GPU box record gets an orientation field (a change to a shared path). Other boards use that path.
- Open board sessions keep their old `bv_render_3d` copy until relaunched (known quirk), so an engine fix reaches an open window only through a NEW session.
- Cost on this weak machine must be MEASURED, offline first (`bv_gpu_seam_test`-style tooling) before any live window; heavy builds go through `cpu_gate` and one at a time with the temperature checked.
- Alpha (per-part transparency) is a separate, costlier job and stays off by default.
- The 128-box budget is shared with trees, items, cracks and chips. Rotation adds no boxes, but more actors do: count and measure, never guess.
- Not in this plan: skinned meshes/skeletal deformation (would be a different render path). Voxel and primitive scenes scale to Minecraft/Teardown class, bounded by the box budget.

### 11.6 Revised order of work (replaces the animation lines in section 8)
1. Data: `SCHEMA | rig | 2`, `PART`, `ANIM`, `TRACK`, `STATE` parser plus the pure evaluator file (condition 1), and op `rig_pose_op <rig> <anim> <t_ms>`; harness case first (a known pose, parent carries children, schema-1 files load unchanged, a sabotage that must fail).
2. Stage 1 renderer path (offset boxes) so the owner can SEE the data format running.
3. Stage 2: orientation in the GPU box record and the ray transform; offline seam-style test, identity-frame comparison, then a measured frame cost; owner approval before the live engine changes.
4. Stage 3 only on request: analytic sphere/capsule/cylinder, real alpha.
Gates: the owner sees each stage before the next starts. T01 "drive Asa" is not blocked by this; its captions and keys are separate.

## 12. OWNER RULING 2026-11-26: effects tiers, "big machines and smol" (D141)

Owner: "we will optimize for machines depending on their ability and be flexible; that's the one thing I would say when doing intense. Let's give fx to big machines and small. Doc that and go."

**Rule:** no feature may assume a strong machine. Every costly effect (real rotation, many poses, alpha, analytic spheres, voxel resolution, shadows, particles such as break chips, animation step rate, actor count) has a **cheap form** and a **rich form**, chosen by DATA, per machine. The same rig and animation files run on both; only the tier changes how much of them is used. A weak machine must always still work (this one is weak and shares a browser: AGENTS.md "our programs run alongside normal user activity").

**Shape (DESIGN, nothing built):**
- `fx.pdl` rows: `TIER | name | description` and `FX | tier | key | value`. Two tiers to start, `small` and `big`; a third can be added later as rows, never as code. Example keys: `rig_rotation` (0 = stage-1 offset boxes, 1 = real oriented boxes), `rig_anim_steps` (poses per cycle), `voxel_res_max` (cap on a part's `res`: 8 / 16 / 32), `alpha` (0/1), `analytic_shapes` (0/1), `chips` (0/1 and count), `box_budget` (<= `BV_GPU_MAX_BOX`), `anim_hz` (update rate), `view_distance`.
- **Which tier a machine uses is a PREFERENCE, not game state** (it never changes an outcome, so it never touches the ledger or the rules hash): a one-line file `fx_tier.txt` (user override, wins) and, when absent, an `fx_probe_op` that reads cores, load and the measured frame time of the last frames and writes a RECOMMENDATION row; it never changes a tier silently while a window is open and never exceeds the owner's choice. Frame-time based downgrade (a "frame guard") is a later, optional rule.
- **Same truth on every tier:** the ledger, the rules and the replays are identical on `small` and `big`; only the drawing differs. Harness: the same ledger projected on both tiers gives the same event rows, and the tier file is the ONLY difference (a ghost-data check).
- **Defaults:** `small` is the default until a probe or the owner says otherwise (safe on the weak machine); a missing `FX` key means the cheap value.
- **Each stage names its tier behaviour:** stage 1 (offset boxes) is the `small` form of rotation; stage 2 (oriented boxes) is the `big` form and may be allowed on `small` after it is measured cheap. Stage 3 alpha and analytic shapes are `big` only until measured.
- **Measuring:** a tier's cost is measured with a C op and written next to the producer's state (CPU pitfalls 30 to 32), on this machine with a browser open, before any tier is called cheap. No number is quoted that was not observed.
- **Offload:** heavy tasks (video, builds, TTS) still go to the Mac/debil, one job at a time with the temperature checked; the tier system is for the RUNTIME picture, offload is for the production work.

Applies to everything planned here: Ava's FSM and RL features (decision frequency per tier), mobs and day/night (entity count and update rate per tier), tutorial captions (cheap text only), and the survival roadmap.

## 13. Progress log
- **2026-11-26, section 11.6 step 1 BUILT (data + the ONE evaluator + the test op), NOT on screen yet:** `&.widgits/board-viewer/ops/bv_rig_anim.h` (pure parser and evaluator: `SCHEMA`, `PART` tree with pivot / pos / rot / scale / alpha, `ANIM`, `TRACK`, five eases, loop, parent composition, world AABB per part; unknown row types and extra columns ignored), `rig_pose_op.c` (prints the evaluated pose, no writes), harness `rig_anim` (case `cases/rig_anim.pdl`, `rig_anim.pal`): **28 passed, 0 failed** (observed, run through `cpu_gate`). Expected numbers were worked out by hand from the geometry (a 90 degree roll about the shoulder, a one-block lift carried to the child, a sine ease midpoint = 0, a loop wrap, t 1000 in an 800 loop = t 200). Controls: `-DBV_RIG_ANIM_MUTANT_NO_PARENT` compiles and its output LACKS the on-shoulder arm position (so the parent check can fail); an unknown parent, an unknown channel, an unknown animation and a missing file are refused (exit 4, 4, 3, 2); the real schema-1 `rig_asa.pdl` loads as `schema=1 parts=0` (old rigs keep working).
- **Not done, so not claimed:** the renderer does not include the header yet (nothing changes on screen), no real `rig_asa` schema-2 file exists, no stage 1 offset-box path, no stage 2 rotation, no effects tiers file (D141) beyond the doc. The eval is plain C maths, but its cost on this machine is unmeasured.
- **Next:** author `rig_asa.pdl` as schema 2 (body, head, two arms, two legs, with walk, idle, mine and place animations as TRACK rows), then the stage 1 renderer path behind `fx.pdl` tier `small`; owner sees it before stage 2.

## 14. T01 "drive Asa": what is BUILT and SEEN on 2026-11-26, and the steps for the owner (the gate: he does them before I continue)
Owner answers applied: switch `[` and `]`; take control = Enter on the cell AND Enter anywhere (`KEY | possess_enter_anywhere | 1`); release = Esc AND `u` (`possess_release_alt`); badge in the book ledger; English captions with a switch; Ava a small FSM with RL features later (NOT built); animation: real rotation chosen (D140), stage 1 built.
Commits (all local): `1efea67c1` possession keys, `efb58e3e2` renderer (rig2 hook + minimap markers, only my hunks), `309a8f18a` driver / desks / rig data / harness, `52c6524f2` evaluator, `42248d342` D141 tiers.

**SEEN on screen (frames opened):** the alpha page appears in the page list as `rmmc-alpha-iterate` and opens with textured terrain; Asa and Ava both stand on the plain; both have a 5 px minimap marker in their own colour (Asa red-orange, Ava pink); a held walk pose shows legs spreading and closing, arms swinging opposite, torso bobbing across 8 frames (limbs SLIDE, they do not tilt: stage 1).
**RAN with the engine key relay (the same codes the window writes):** `]` and `[` cycle Asa and Ava (possessed_id, cursor, SEAT rows human/harness both ways); `u` releases; space made Asa jump (ledger MOVE rows); code 1001 walked him east; Player > Play started `rmmc_driver` (lock, game bar feed, hotbar with counts); Player > Stop and the `stop` verb set flag_running 0.
**Harness:** rig_anim 41, rmmc_follow 30, rmmc_seat 47, rmmc_beats 39, rmmc_verbs 84, rmmc_project 64.
**NOT verified (do not assume):** a PHYSICAL keyboard press (my synthetic `xdotool key` presses did not reach the board; they went to whatever window had focus: that was my mistake, a few Right and space presses, harmless but wrong); Esc releasing possession; arrow keys through the real window (the director's `xkey:` path worked earlier in this session, the engine relay path worked today, the last hop is yours); hand colours; break chips; the T01 video and in-game captions (not made); `Stop` does not pause `rmmc_driver` (it ends only when the editor window ends or I kill it); the vox rules directory is shared by both pages.
**Things I broke and fixed today:** I killed the running engine with SIGTERM; the board's `loop_guard` read that as the board dying and ended the dispatch loop, so the editor stopped redrawing; I relaunched the board with `@.apps/rpg-edit-hq/button.sh run` (the supported replace). Lesson: never SIGTERM a board engine, let it idle out. The relaunch reset `In:` to off; I turned it back ON, put the page back on rmmc-0001, possession none, game stopped, no camera override.
**Known trap (design):** with the board ENGAGED (`[^]` on the In cell) the Enter key goes to the board, so with `possess_enter_anywhere=1` Enter takes the next token instead of opening a menu. Turn In: off to use a menu with Enter, or click.

### T01 steps for the owner (about 3 minutes)
1. Click the `page:` cell, choose `rmmc-alpha-iterate` (arm it with a click, then Enter). Turn `In:` ON.
2. Player cell > `Play`. The bottom hotbar appears and the left panel says `Player: playing`.
3. Press `]` : Asa is possessed (HUD `poss rig:asa`, the bar says `seat human`). Press `]` again for Ava, `[` to go back.
4. Walk with the arrow keys, jump with Space, mine with `b`, place with `g`, pick a hotbar slot with `1` to `9` (`0` flips the page), craft with `c`.
5. Camera: `p` first person, `o` third person, `1` to `4` as before, `f` resets.
6. Release with `u` (or Esc). Player > Stop when done.
What to look for: the walk swing, Ava's pink marker on the minimap, the seat flipping in the game bar. Tell me which key did nothing.
