# rpg-dust: a pets-like board for a whole planet, from solar system down to molecules (design, 2026-10-23)

Status: **PAPER DESIGN. Nothing in this file is built.** Written by claude (Sonnet 5.5) from the owner's request of 2026-10-23 and from reading the house. Not yet seen by grok.
Labels: **READ** = from a file or git, **RAN** = I ran it and saw the result, **PROPOSED** = my design, **JUDGEMENT** = my opinion, not measured, **OWNER** = said by the owner.

## 0. The ask (OWNER, 2026-10-23, kept close to the words)

A new pets-like board, **rpg-dust**, that simulates an entire earth-like planet; a **tweakable seed** generates different solar systems. Zooming **out**, play-scale buildings shrink into a world; zooming **in** reaches a **molecular** level where the real electromagnetism and chemical reactions can be viewed. Living entities have **anatomy** and are simulated at that scale. At every level the user can open "limitless" generated menus, driven by **size, place and species**, which decide what to generate (bones, skeleton, ...). Entities cook, harvest, create, do material science. The owner wants to start building this **deliberately** and asks: what should be done **by hand**, what with **weights**, what with **events**, how to use the **current boards**, and how the **marathon refactor** simplifies it.

## 1. The short answer (PROPOSED)

1. **A scale ladder is data, not code.** Each level is a row (how big one cell is, who generates it, how it is drawn, how fast it ticks). Adding a level adds a row. "Limitless" = the ladder is open-ended, not that everything exists at once.
2. **The world is a pure function of `(seed, edits)`.** Nothing is stored that can be regenerated. Generated pages are disposable cache files. The only authoritative state is the seed, the ladder rows, and an **append-only overrides ledger** (what the player and entities changed). That is the house rule "if it is not in a file it is a lie" applied to a world too big to store.
3. **Three kinds of decision, kept apart:**
   - **Laws** (physics, chemistry, scaling laws, unit maths): written **by hand**, deterministic, unit-tested, never weighted.
   - **Choices** (which body plan fits this size and place, which proportions, which material for a tissue, which words describe it): **weights in the concept bank**, edited only through candidate edits that a human promotes.
   - **Actions** (cook, harvest, craft, zoom, react, eat, die): **events / blueprint pages** that call small resolver ops, with every outcome appended to a ledger.
4. **rpg-dust is a tenth skin of the pets core**, not a new program. The core gives the daemon, rooms/doors, entities, chat, shop/ledger, 2D/3D export, windows and verb pages (section 6). It needs **one new core seam** (pages made on demand, section 7), which every other board would also benefit from.
5. **Build it as a vertical slice, not a layer at a time:** one seed, two or three adjacent levels, one species, one reaction, each with a test that can fail (section 9).

## 2. What exists that this stands on (all READ unless marked)

| Need | What the house already has | Where |
|---|---|---|
| a game as data + a small C file | the pets core, nine skins (230 to 547 lines each + about 21 data files) | `&.widgits/_shared-lib/petslike_core.c`, `@.apps/<game>/` |
| a skin that paints its own pages | `PET_OWN_EXPORT` hook; rpg-edit-hq paints islands with depth per cell, hung rock, taper, a far island | `@.apps/rpg-edit-hq/ops/rpg_edit_hq.c` sections 72 to 74 |
| per-game sizes | `MAXC MAXR NP FW FH` overrides (rpg-edit-hq: 64 x 24 cells, 16 pets, 1360 x 440 frame) | core top |
| 2D tile and 3D views, z levels | board engine; z/x now moves the xelector and camera on painted maps (**RAN 2026-10-23**: rpg-pet, civ-rpgce, tcg-rpgce, cdda-hq, mcl-hq, gta-hq pass; rpg-edit-hq passes to -8; **df-hq stopped at 19 not 17 (unexplained); dsr-rpg's engine did not start in the test**) | `&.widgits/board-viewer/` |
| scale as a concept, cell spans, levels up to orbit | **Z-cell format v1.1**: `ZWORLD`, `ZLEVEL | world | z | span | alt_m | scale_concept | label`, `span` a power of two, "scale is a concept, not a number in code". Paper design plus a parse check; no game reads it yet | `16.game/Z-CELL-FORMAT.md` |
| parts with sockets, assemblies | **brixels**: `BRIXEL`, `SOCKET`, `PLUG`, `ASSEMBLY` rows (a car, a person) | `shared/brixels.pdl`, `BRIXELS-DESIGN.md` |
| ground-truth chemistry | `elements.pdl` (118 elements, protons, neutrons, hash-locked), chemistry tiles, compound CSV, `chem_label` (21/0 harness), `hidden_layer` (32/0) | `&.widgits/concept-bank/`, `#.ref/menu/palletes/` |
| soft weights with a gate | concept bank: masters and spokes with `WEIGHT`, candidate `EDIT` lines, `concept_edit_validate` (**RAN today**: PASS on a good edit, REJECT on out-of-bounds and on an unknown master), `promotion_ledger` (**RAN today**: replay of 3 observations on a scratch copy gave reward 2 / punish 1 / score 0.6000, identical on a second run, bank files unchanged) | `&.widgits/concept-bank/`, `^.hai-horn/ops/` |
| entity learns skills | `entity_grade` (grades by exams, level/EXP/MP by skills), curriculum and skillbook rows | `concept-bank/ops/entity_grade`, `^.hai-horn/curriculum.pdl` |
| a game-event ledger and resolver | S1: `EVENT | time | actor | verb | target | at | result`, verbs `give_item take walk dig build craft`, replay gives identical state | `S1-EVENT-LEDGER.md`, `_shared-lib/game_events.c` (df, cdda, mcl) |
| a decision ledger | M3: `DEC | ts | stamp | actor | state | action | target | outcome` | `DECISION-LEDGER-AND-DAILY-REVIEW.md`, `game_decisions.c` |
| words to pictures | image word bank (`WORD | id | text | kind | weight`), `img_prompt` (weight change changes the prompt: **RAN 2026-10-13**), img-hai studio with two judges | `concept-bank/data/img/`, `&.hq-apps/img-hai/` |
| events as programs | event pages (IR, pal, wrappers), 52 command types in use (135 registered) incl. `if / loop / variables / switches / call_common_event`; `verbgen` keeps manifest, engine and pages in agreement | `@.apps/<game>/events/`, `verbs.pdl` |
| the vision of a voxel planet with jobs, trade, reproduction | pet-trainer WORLD-DESIGN (layers 1 to 6, society layer) | `@.apps/pet-trainer/WORLD-DESIGN.md` |
| heavy compute off this machine | offload rule: GPU or big jobs go to debil or the Mac | owner rule 2026-10-13 |

**What does not exist (READ, corrected 2026-10-23 after finding the prior art below):** code for scale levels and zoom between them, a seeded generator for a planet or a system, an anatomy generator, a reaction resolver, a molecular view. The Z-cell format is paper. The concept bank has no game, anatomy or reaction nodes yet (2026-10-14 handoff).

### 2b. Prior art I MISSED in the first draft (found 2026-10-23; the first draft said "not found: any planet physics"; that was wrong)
| What | Where | State |
|---|---|---|
| **solar sandbox**: four desks (system hub, Earth, Moon, Mars), emoji bodies, Enter / Leave orbit / Teleport rows | `&.widgits/solar-sandbox/` | built as template files, harness 41/0, not installed in a live session |
| **physics nodes**: gravity, orbit and escape velocity as swappable, linked nodes: rows `CONST BODY NODE LINK TUNABLE OVERRIDE`, resolution **page > planet > system > house default**, op `phys_node_eval` | `&.widgits/physics-nodes/` | built, harness `phys_nodes` 31/0 (Earth g 9.82, Moon 1.62, Mars 3.73; swapping an impl recomputes downstream) |
| **the design behind it**, incl. the owner's 2026-10-07 correction "gravity is a node, not a built-in": derived-never-stored physics, a random planet generator as step 6 | `08-roadmap/design-docs/SOLAR-SANDBOX-AND-PLANET-PHYSICS-DESIGN.md` | design; steps 1 and 2 built, temperature / pressure / travel nodes not |
| **a planet as a ball of voxels from a seed**, lazy chunks, spherical gravity as a node, and **two clocks, one world** (stepped vs real-time) | `08-roadmap/design-docs/REAL-3D-PLANET-EXPLORATION-AND-REALTIME-MODE-DESIGN.md` | design only |
| **formula parser with a locked element table** | `concept-bank/ops/chem_label.c`, harness `chem_label` 21/0 | built; it returns total atoms and mass, not per-element counts |
| **a variable comparison command**: `variable_math` with `lt le gt ge eq ne add sub mul mod`, a C op over `variables.txt` (the worker pilot `q020 var_cmp` duplicated it and was not registered) | `#.ref/menu/event_commands.registry.pdl`, `ring-board-rmmv/ops/var_math.c` | built; `if` itself tests only switches |

**What this changes in this design:**
1. **Laws are nodes, not private code.** Section 6.1's laws (scaling, orbits, climate) should be written the physics-nodes way: a node with a hand-written default `impl`, `TUNABLE` joints and `OVERRIDE` rows by scope. They stay hand-authored and deterministic, never learned weights, but a person can swap or tune them without touching generators. This also answers "is the visual layer real": the owner already decided physics is event pages wired together.
2. **The solar-system generator extends `system.pdl`** (BODY rows, seedable) instead of starting from nothing; the sandbox desks are the first system-level pages.
3. **The planet-from-seed idea is already designed** (voxel ball, lazy chunks); this design adds the levels below and above it and the overrides ledger.
4. **Clocks:** the "two clocks, one world" decision stands (a mode is a clock source, never a second implementation); per-level ticks in section 5 extend it.
5. **Chemistry:** a per-element conservation check is new (built today, section 9, R0b); `chem_label` stays as it is.
6. **Nodes can carry logic.** RAN 2026-10-23 on a scratch copy: a page of two nodes, `change_items` and `control_switch`, with no C verb, was compiled by the events manager and run by the prisc VM (`item_berry=1`, `harvested=1`; three runs gave `item_berry=3`). Branching on item counts goes through `variable_math` plus a switch; a **cook** built from nodes is plausible but **not yet proven**.

## 3. The scale ladder (PROPOSED)

One row per level. Levels are **powers of two in metres per cell** (`log2_m`), matching the Z-cell rule that spans are powers of two. The numbers are examples to be tuned; the rows are the design.

```
SCALE | id       | log2_m | cell is about | parent   | gen op          | view           | tick   | note
SCALE | system   |  +36   | 0.46 AU       | -        | dust_gen_system | 2d-tiles       | day    | star, planets, moons; orbits from a seed
SCALE | globe    |  +20   | 1000 km       | system   | dust_gen_globe  | 2d-tiles       | day    | one planet: oceans, land, ice, climate bands
SCALE | region   |  +14   | 16 km         | globe    | dust_gen_region | 2d-tiles       | hour   | biomes, rivers, mountains
SCALE | area     |   +8   | 256 m         | region   | dust_gen_area   | 3d-voxel       | 10 min | a village, a forest, a coast
SCALE | play     |    0   | 1 m           | area     | dust_gen_play   | 3d-voxel + 2d  | minute | buildings, pets, plants: today's boards
SCALE | body     |   -4   | 6 cm          | play     | dust_gen_body   | 3d-voxel       | second | bones, organs of ONE entity
SCALE | tissue   |  -10   | 1 mm          | body     | dust_gen_tissue | 3d-voxel       | second | bone, muscle, blood vessels
SCALE | cell     |  -17   | 8 um          | tissue   | dust_gen_cell   | 2d-tiles       | ms     | a cell and its organelles
SCALE | molecule |  -30   | 0.9 nm        | cell     | dust_gen_mol    | 2d-tiles + field | us   | molecules, bonds, charge
SCALE | atom     |  -33   | 0.12 nm       | molecule | dust_gen_atom   | 2d-tiles + field | us   | electrons as charge clouds (a MODEL, section 8)
```

- "Zoom" = a **door**: an event (`zoom_in`, `zoom_out`) moves the camera and the focused address from one level to its parent or child. The core already treats doors as events.
- A level has a **view style** (which engine view draws it) and a **tick** (how fast its own clock runs). Only the focused level and a **summary** of its parents run; far levels are not simulated (section 5).
- To add a level (for example `galaxy` above, or `quark` below) add a row and a generator op; nothing else changes.

## 4. The world is a function of seed and edits (PROPOSED)

- `SEED | universe | <64-bit number>` plus a few **tweakable parameter rows** (`PARAM | star_mass | 1.0`, `PARAM | metallicity | 0.02`, `PARAM | ocean_fraction | 0.7`, ...). Different rows give different solar systems.
- **Address** of anything: a path such as `system/p3/globe[12,7]/region[4,9]/area[30,2]/play[10,10]`.
- **Child seed** = `hash64(parent_seed, child_key)`. The hash is one small C op (`dust_seed`), pure and unit-tested.
- Each generator op is **pure**: `dust_gen_<level> <seed> <address> <params> -> cells, entities`. Same inputs, same bytes out.
- Generated pages (`map.txt`, `cells.txt`, `extrusion.pdl`, `events.txt`, the files the board engine reads today) are **cache**: deletable, regenerated on demand, verified by a hash.
- **Edits** (an entity cooks, a tree is cut, a wall is built) are rows in `state/world/overrides.txt`, append-only: `OVR | ts | address | field | value | cause_event`. The page is `generate(seed, address)` plus its overrides, replayed in order. This is the pattern the pets core already uses for `BUY / PLACE / MOVE / REMOVE` (**READ**: `replay()` in the core); here it covers the world instead of the economy.
- **Why this matters:** a planet cannot be stored; a seed and a ledger of changes can. It also makes every claim testable: the same seed twice must give identical files (a gate like the sky gate from the marathon).

## 5. Time, cost and "only simulate what you look at" (PROPOSED)

- **Lazy.** Generate a page only when a window or an entity needs it. Keep a small set of live pages (LRU). Everything else is a **summary row** (for a region: biome mix, population, resource totals), updated at a coarse tick by a cheap op.
- **Summaries are laws, not shortcuts.** A region's summary row is computed from its child rows by the same rules the child level uses (totals, averages). When the player zooms in, the child page is generated **consistent with the summary** (the generator reads it as a constraint), and a gate checks conservation (sections 8 and 9).
- **Multi-rate clocks.** Each level has its own tick (table above). The house clock daemon (`lc_clock`) already runs a game clock; per-level ticks are extra clock rows, started only for focused levels. **Not designed in detail**: this is the least-known part (section 10).
- **Budget rule from the marathon:** a page run (sh + bash + prisc) costs about 100 ms on top of the verb (**RAN 2026-10-23**: `goto` direct 93 ms, through its event page 194 ms, on a loaded machine; the C binary alone takes about 62 ms to start). So per-tick behaviour (pet and creature AI, field updates) must be **C or data**, never a page per tick; pages are for player-meaningful actions.
- **Heavy generation runs off this machine** (debil or the Mac) when a level needs it; results come back as files.

## 6. What to do by hand, with weights, with events, with ops (PROPOSED)

### 6.1 By hand (authored, deterministic, locked or unit-tested, never weighted)
- The **scale ladder rows**, the seed and hash rule, the address grammar, the override ledger format.
- **Physical constants and units**, in one locked file (like `elements.pdl`: derived from a source, hash-locked).
- **Elements** (done), **bond data** (valence, typical bond lengths and energies) from public tables, a small **reaction table** (reactants, products, enthalpy, activation, conditions) for the reactions the game uses first (combustion, water, photosynthesis, a few oxidations).
- **Material properties** (density, melting and boiling points, hardness, conductivity) for the first twenty materials, from public data, locked.
- **Scaling laws** (allometry: how bone cross-section, limb length and metabolic rate scale with mass and gravity), written as **physics-nodes rows** (`NODE` with a hand-written default `impl`, `TUNABLE`, `OVERRIDE`; section 2b) with tests. These are physics, not preferences: a person can swap or tune them, a learned weight never does.
- **Orbit and climate maths** for the system and globe generators (Kepler, insolation by latitude, a simple energy balance). Simple and deterministic; labelled a model.
- The **body-plan skeleton graphs** for the first two or three archetypes (as `ASSEMBLY` rows of bone brixels with sockets). Hand-authored first; later archetypes may be proposed by models as candidates that a human promotes.
- The **first species and biome rows**, and the **first recipe events** (cook, harvest, smelt).
- **Every generator and resolver op**, and **the gates** (section 9). A generator is never a model call.

### 6.2 With weights (concept bank; soft choices; human-promoted; replayable)
- **Which body plan fits** a given size class, gravity, medium (land, water, air) and climate: `species -> plan` spoke weights.
- **Proportion ranges** (how long the limbs are relative to the spine, within the physical bounds the scaling laws allow).
- **Which material a tissue uses**, and **how common** each reaction is in a place (a prior, not a rule).
- **Which words describe a thing** (image word bank), and which **menu entries** a concept shows, in what order.
- **How far a creature's behaviour leans** (cautious, curious): behaviour-bank weights, as the house already plans for pets.
- A weight **never** decides whether a reaction conserves mass or whether a bone can bear a load. If a choice would break a law, the **law** wins and the choice is rejected by the validator.
- **Every change** is a candidate `EDIT` line, checked by `concept_edit_validate` (extended with new target types), promoted by a human (dual verdict: owner and claude), and logged. **RAN today** on the existing validator: PASS and REJECT cases behave as documented. Extending it to image words, anatomy and reactions is **not built** (the image-word note in `WORD-BANK-TO-IMAGE.md` section 4 says the same).

### 6.3 With events / blueprint pages (actions; every outcome in a ledger)
- **Zoom** (`zoom_in`, `zoom_out`, `zoom_to <address>`), **place / dig / build / craft / cook / harvest / eat / rest**, **react** (apply a reaction row at a place), **inspect** (open a generated menu).
- Each is a page whose nodes call a resolver op; the resolver writes an S1 `EVENT` line and, when the world changes, an `OVR` line. The page is **in the execution path** (the verb-page design of 2026-10-20), so a human or blueprintz can change it.
- **Generated menus** are queries, not code: `inspect <concept>` lists the events linked to that concept (`META | concept | <id>` rows on events, the design of 2026-10-14, **not built**). A dragon concept lists `loot, fly`; a farmer lists `plant, harvest`; a bone lists `fracture, grow, weigh`. New concepts add menus with **zero new C**. This is the honest meaning of "limitless menus".
- **Entities learn** by grades (existing `entity_grade`): a pet that has "passed" a material exam can do material-science actions with fewer failures. Skills cost MP, as the owner already specified.

### 6.4 With C ops (pure, seeded, tested)
`dust_seed`, one `dust_gen_<level>` per level, the physics steppers per level (orbit step; fluid-lite or cellular climate; rigid-body-lite at play scale; reaction stepper; a field solver for charge), the resolvers (`craft`, `cook`, `react`, `harvest`), and the exporters that write what the board engine reads. These are the same shape as the pets-core skins and the painter ops the house already has.

### 6.5 With pal / prisc
Only orchestration: tick loops that `exec` ops, and page programs the event compiler already emits. No new logic in pal.

## 7. How to use the current boards (PROPOSED)

1. **Copy a small skin** (`dsr-rpg` is 230 lines; `rpg-edit-hq` is the closest model for painting its own pages) to `@.apps/rpg-dust/`. Fill the data files (`toy.pdl`, `rooms.pdl`, `party.pdl`, `toolbar.pdl`, `verbs.pdl`, ...). Run `petslike_tpl gen` for the board window and the standalone window, and `verbgen gen` for the verb pages. **You get**: daemon, windows, doors, chat, ledger replay, footer pictures, sky phase, CPU gating, verb pages.
2. **Pages are the levels.** A page (core "room") is one generated page of one level. rpg-edit-hq already runs 6 pages (the island 56 x 17, five interiors 24 x 17) with its own painter (**RAN**), so a slice with a handful of pages works with **no core change**.
3. **Painter pattern from rpg-edit-hq.** Its `sk_*` functions (stamp, taper, per-cell depth, own export) become the base of `dust_gen_area` and `dust_gen_play`; a floating island is already "a chunk of ground with depth under it".
4. **Views from the engine.** 3D voxel for area, play, body and tissue; 2D tiles (already fit-the-page with zoom keys) for system, globe, region, cell, molecule. The z fix means depth (underground, inside a body) is visible on painted maps.
5. **Scale concept** from the Z-cell format: `ZLEVEL span` and `scale_concept` become the `SCALE` rows. Implement the Z-cell reader as a small text-include, as that design intends; it is paper today.
6. **Resolvers and ledgers** from df-hq / cdda-hq / mcl-hq: `game_events.c` (S1) for outcomes, `game_decisions.c` (M3) for creature decisions, shared by text-include.
7. **Chemistry** from the concept bank (`elements.pdl`, `chem_label`, `hidden_layer`, `entity_grade`); **anatomy parts** from brixels; **appearance** from the image word bank and img-hai (drafts made off-line, judged by two judges).
8. **Society layer** (jobs, trade, reproduction) from the pet-trainer WORLD-DESIGN, later.
9. **Voices** for species from the voice-fx studio, later.

### The one core change needed (PROPOSED)
The core holds its pages in a fixed array (`rooms[]`, 8 by default, 24 with the override). A level ladder needs pages **created on demand and dropped when far away**. Proposed: a new numbered seam (section 75) that lets a skin answer "give me page N" at run time, with an LRU of live pages. This is a core feature, not an rpg-dust hack: the other nine boards would not change (gated by the same compile-all plus verb-script diff gate as the marathon). **Phase 1 does not need it.**

## 8. Honesty about "real" physics (JUDGEMENT, stated so it is not hidden)

- At every level the game runs **simplified deterministic models** (orbits, a climate balance, rigid and fluid approximations, a charge-field picture, reaction rows), **not first-principles quantum mechanics**. The molecular and atom views are **educational pictures driven by the same rows the game uses**, labelled as models, with their approximations written next to them.
- **Conservation is the credibility test.** At least: atoms of each element are conserved across any reaction event; mass is conserved at play scale; energy bookkeeping uses the reaction row's enthalpy. These are gates (section 9), not hopes.
- **Cross-scale coupling is by summary laws**, not by simulating molecules to run a fire. Zooming in generates a **consistent** molecular scene; it does not **drive** the macro world, except through the reaction rows both levels share.
- Some things will be wrong or crude. The design keeps every law in one reviewable place (section 6.1) so the owner and grok can correct a law without touching generators.

## 9. Build order, each step with a test that can fail (PROPOSED)

| Step | What | Proof that would show it wrong |
|---|---|---|
| R0 | **BUILT 2026-10-23** (ladder, seed and params in `@.apps/rpg-dust/scale.pdl`; `dust_check ladder`; harness `dust_ladder` 61/0). Still paper: the address grammar and the override format. | RAN: the check refuses a duplicate id, an unknown parent, two or no roots, a child not smaller than its parent, bad seed (letters, negative, above 2^64-1), bad view / tick / gen name, duplicate or non-numeric PARAM, an unknown row kind; two deliberate code mutants were caught by the harness (5 and 2 failures) |
| R0b | **BUILT 2026-10-23** atom conservation as a harness case before any reaction code: `dust_check react` over `@.apps/rpg-dust/reactions.pdl` and the locked `elements.pdl`; harness `dust_react` 39/0 | RAN: an unbalanced, a lost-atom, an extra-atom, a wrong-coefficient, an unknown-element, an unbalanced-parenthesis and a case-confusion (Co vs CO) reaction are each refused with the exact per-element difference; a code mutant that only detects lost atoms is caught by 5 checks |
| R1 | **BUILT 2026-10-23 (seed rule + parameter scoping; no real generator)** `@.apps/rpg-dust/ops/dust_seed.c` (splitmix64 anchor, child/path seed rule, stand-in noise page) with harness `dust_seed` 41/0; four code mutants caught. R1b DONE the same day: PARAM rows feed the level seeds at their level (`dust_seed lseed`), harness 79/0, three more mutants caught (all PARAMs at the root; parameter also moves levels above; PARAM order matters, which first SURVIVED because the fixture had one PARAM per level, then caught after a two-PARAMs-one-level fixture). NOT yet: files written by a real generator and a byte compare of files. Original row: `dust_seed` and a **determinism gate**: the same seed and address twice give identical bytes; a different seed differs; a changed parameter changes only the levels below it | byte compare of generated files; a negative test that flips one parameter and must change the hash |
| R2 | **HALF BUILT 2026-10-23: the generators and the conservation rule, no board.** `dust_gen region/area/conserve` (heights only, text files) with harness `dust_gen` 63/0. R2b BUILT the same day: `dust_gen zoom_in` / `zoom_out` / `page` (address operations, round trip tested, harness 87/0). R2c BUILT the same day: `zoom_in` / `zoom_out` / `start` are EVENT pages (made by verbgen, played through events-hq and the prisc VM) over an append-only zoom ledger; refusals are logged; harness `dust_zoom` 79/0. STILL NOT built: showing it in a board window, wiring the pages to a button, the screenshot. Original row: **Two adjacent levels** (`region -> area`) through the existing painter pattern, shown in the board window; a `zoom_in` event | screenshot of both levels from one seed; zooming in then out returns to the same address; area content agrees with the region's summary row |
| R3 | **Overrides ledger**: cut a tree, replay from empty, same page | delete the cache, regenerate: identical page including the edit; delete the ledger: the edit is gone |
| R4 | **One species, one body page** (`play -> body`): skeleton from a body-plan assembly, scaled by the scaling laws, proportions from weights | change a weight: proportions change inside the law's bounds; push a weight outside the bounds: the validator rejects it |
| R5 | **Molecular page for one reaction** (for example hydrogen plus oxygen to water) from `elements.pdl` and a reaction row; a `react` event | atom counts of each element equal before and after; remove an atom from the product row: the gate fails |
| R6 | **Cook and harvest** at play scale through the S1 resolver, with a material row | the resolver rejects a recipe whose inputs are missing; replaying the ledger gives identical inventory |
| R7 | Generated **menus** from `META | concept` links | adding a link adds a menu entry with no code change; an unknown concept id is rejected |
| R8 | Core seam for on-demand pages; more levels (system, globe) | all ten skins compile; verb-script diff identical for the nine existing games |
| R9 | Society layer, species voices, appearance drafts | owner judgement; ledger review |

Each step ends with: what I ran, what is only read, what is judgement, and the next smallest step (the same closing as the deep-dive prompt).

## 10. What I am NOT sure about (so it is decided, not discovered)

- **Scale of state.** The core keeps one file per pet and steps up to 16 entities. A planet has millions of entities. Needs chunking, summaries, and a cap on live entities; **unmeasured**.
- **Multi-rate time.** How levels with different ticks stay consistent (a minute at play scale is microseconds at the molecular level) is the least-designed part. The proposal (only the focused level runs; parents hold summaries) is **untested**.
- **File storage at this size.** The house plans an in-memory DB later (wraith-alpha); until then every cache is files. A very large world may need that sooner. **Unmeasured.**
- **Scene frame and views.** The core paints a fixed-size 2D frame (`FW x FH`); a level with a very large page needs the engine's tile view, not the painted scene. **Not tested at these sizes.**
- **Whether a weights-only choice of body plan looks right.** It may need hand-authored plans for a long time; the design allows that (section 6.1).
- **The engine-session start fault** seen today (a board stuck on "loading", dsr-rpg did not get an engine in the 9-board test) would hurt a multi-level game that opens and closes sessions. Cause not found; **open**.
- **df-hq stops at z=19** instead of the floor in the z test. Cause not found; **open**. rpg-dust relies on z for depth.

## 11. Decisions I need from the owner

1. Is the first slice **region -> area -> play** (terrain and buildings) or **play -> body** (anatomy)? I recommend the first, then anatomy.
2. Which **first species** and which **first three reactions**?
3. May the **core get the on-demand page seam** (R8), or should rpg-dust live with a fixed set of pages until later?
4. Who authors the first **material and bond tables** (hand, from public tables, locked), and may a worker draft them for review?
5. Is "educational model, conservation-checked" the right promise for the molecular level?

## 12. How the marathon refactor simplifies this (summary; the same text is in the email copy)

**Before the marathon (READ):** eight games were eight copies of one 1200 to 1500 line program, about 11,000 lines; windows were 16 near-identical hand files; each verb was a C function with no page; a fix reached one game.
**After (READ and RAN):**
- **A new game is a skin and data**: about 230 to 550 lines of C plus about 21 data files; the core is 1407 lines for all ten. About 11,000 lines of copies became about 4,600.
- **The windows are generated** from two templates by `petslike_tpl`; **the verb pages are generated** from `verbs.pdl` by `verbgen`, and `verbgen check` proves engine, manifest and pages agree.
- **The framework bends**: per-game sizes, a hook for a skin's own page export, and a hook for its own first-run spawn were added for rpg-edit-hq. A very different game (islands, 16 pets, its own painter) fit **without** forking the core; that is the evidence the rpg-dust painter will fit too.
- **Shared pieces are single files**: sky and day phase (`pet_sky.c`), footer pictures (`pet_footer.c`), engine wake-up (`pet_nudge.c`), art-to-3D frames (`csv_frames.c`).
- **The cost of a running board dropped**: the daemon idles at about 2 to 3% of a core with the board open (**RAN**: 34 to 49% before the gating).
- **The engine got a z fix once for every painted-map board** (**RAN on 7 of 9**, section 2): depth is visible and bounded, which an anatomy or underground level needs.
- **A gate culture**: compile every skin, diff verb scripts, hash generated files, negative-test the gate. rpg-dust's determinism gate (R1) is the same habit.
- **Heavy work has a path**: debil narrates and drafts images; the same route can run generators.
**What the marathon does NOT give (READ):** on-demand pages, multi-rate time, millions of entities, a declared core/skin interface (74 numbered seams, skins use core globals), reaction or anatomy code, or an in-memory DB. Those are rpg-dust's real new work (R2, R3, R8) and the reason for the slice-first order.

## 13. Next smallest step

Write **R0** (the ladder, seed and address rows plus their parse check) as paper plus one small C check, run it on a good and a bad file, and show the owner the rows before any generator exists.
