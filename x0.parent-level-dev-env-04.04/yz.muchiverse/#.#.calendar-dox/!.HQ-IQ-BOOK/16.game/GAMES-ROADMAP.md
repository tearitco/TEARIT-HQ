# Games roadmap: cdda-hq, df-hq, gta-hq on a shared z, vehicle, physics and learning spine

Started 2026-10-13. Decisions behind it: `GAMES-DECISIONS-2026-10-13.md`. **Every row below is NOT BUILT unless the Status column says otherwise with evidence.** When a milestone lands, fill its Proof and How-to columns in the same commit, and write its tangent doc (section 4). Owner rule: a milestone is done only with a fresh build, a real run, and something the owner can look at (a screenshot or state file), plus a note on what was not verified.

Status words: `planned`, `designing`, `building`, `proven` (evidence recorded), `blocked`.

## 1. Spine (shared; claude designs, house convention for sharing)

| # | Milestone | Owner | Proof / KPI (what must be observable) | How to use / modify (fill when done) | Status |
|---|---|---|---|---|---|
| M1 | **z-cell format on paper**: `world[z][y][x]` cell = terrain id, furniture id, flags, optional region size; ground, floors, cellars, altitude; scale as a concept; 90-degree turns now, room for free angles | claude, review by grok | A design doc; a fixture file with one 3-floor building plus a ground-to-sky column; a parse check that loads the fixture and prints per-z counts, with a negative test that fails on a bad row | `Z-CELL-FORMAT.md` sections 7-8; run `shared/zcell/zcell_check` on a file | proven on paper + fixture (run 2026-10-13); grok review: fits (v1.1, `zcell-v1.1-grok.md`); owner has not seen it; no game reads it |
| M2 | **Concept bank rows for physics and scale** (scale, gravity, drag, thrust as concepts with weighted slots; clone = candidate EDIT with scope) | claude | `concept_edit_validate` accepts a clone of the Earth gravity concept and rejects a malformed one; `phys_node_eval` reads a tunable from the bank and gives the same Earth/Moon/Mars vectors as today | | planned |
| M3 | **Decision ledger**: one marker-ledger line per NPC/enemy/vehicle decision (actor, state, action, outcome) plus the per-game daily review file format | claude + grok | One live character move produces exactly one ledger line; the review file lists it; size-growth marker, not mtime | | planned |

## 2. Games

### cdda-hq (rules: grok; infrastructure: claude)
| # | Milestone | Proof / KPI | How to use / modify | Status |
|---|---|---|---|---|
| C0 | **cdda-hq spec** (owner's bar: menus, movement, stats, inventory, crafting, stairs, vehicles, NPCs, enemies, mass building, map generation). Grok writes the CDDA rules sections; claude writes engine-fit and proof per item | The spec exists, each item lists CDDA rule / engine support / gap / one proof | | planned |
| C1 | **One room with CDDA ids** (UltiCa 32 px; Slice C of `CDDA-DF-SUBAGENT-BUILD-2026-10-12.md`) | A frame or state where a wall corner uses `t_wall_wood_corner_*`, grass uses a `t_grass_center*` PNG, and a different neighbour falls back to `unconnected` | | planned |
| C2 | **Building with cellar, floors and a stair** (first z build; needs M1) | A character walks across z=0, steps on the stair, state z changes to 1, in 2D and after the `0` toggle in 3D | | planned |
| C3 | **A car as a part-cell grid** (needs C2 and brix parts): walk to the driver's seat, engage, it moves | Seat engaged in state; vehicle cells move together; a passenger inside moves with them | | planned |
| C4 | **Menus, stats, inventory, crafting** on db-hq rows and events-hq pages | Open the inventory, craft one item, stat row changes; all from `.pdl`/events, no new C for the rule | | planned |
| C5 | **NPC / enemy FSM with logged decisions** (needs M3) | One enemy chases, one NPC idles; each decision is one ledger line; one weight change appears in the review file | | planned |
| C6 | **Scripted cdda playthrough** | A script runs start to a visible result; owner watches it | | planned |

### df-hq (grok)
| # | Milestone | Proof / KPI | Status |
|---|---|---|---|
| D1 | Designation becomes a job row; one mover walks to the adjacent stand tile (Slice E of the subagent doc) | The map file shows the dug cell; freeglut program untouched | planned |
| D2 | Several movers under a per-tick path budget | A count of movers and the budget in state; CPU measured | planned |
| D3 | Stockpiles and save shape (`meta/map/items/units`) | Save, reload, equal state | planned |
| D4 | z-levels for df-hq after M1 and C2 | Dig down a level, a mover follows | planned |
| D5 | Scripted df playthrough | As C6 | planned |

### gta-hq (claude with the owner, design first; grok's code stays as is)
| # | Milestone | Proof / KPI | Status |
|---|---|---|---|
| G0 | **gta design page**: island size menu, detail level by a renderer op from parts / distance / setting, animation, the Stable Diffusion word-bank pipeline, claude as final judge | The page exists and the owner has answered its open questions | planned |
| G1 | **Island size from a menu** | Choose a size, the island regenerates at that size; CPU measured | planned |
| G2 | **Detail-level op**: one brix assembly drawn at three distances from the same parts | Three screenshots of the same car at three distances | planned |
| G3 | **Word-bank to image pilot** (by hand, no runtime generation) | One concept goes word bank, prompt, image, judged; the judged image becomes a brix asset | planned |
| G4 | Scripted gta playthrough | As C6 | planned |
| G3a | **img-hai window** (text-to-image GUI for brixels; owner's request 2026-10-13): slices H1-H6 in `IMG-HAI-DESIGN.md` | H1: window screenshot + ui state listing the parts and existing drafts; H3: a Draft click makes a new image on debil with the window responsive | **built and relay-tested 2026-10-13** (H1-H5 done: palette, mixing canvas, words, Draft, Mac, batches, judging, tuning trials); owner review pending; menu entry not built. Docs: `&.hq-apps/img-hai/docs/` |

## 3. Flight, scale and learning (needs the spine)

| # | Milestone | Proof / KPI | Status |
|---|---|---|---|
| F1 | **Flight nodes** (thrust, drag, lift) added to `physics-nodes`, summed from part cells | Harness vectors; swapping an implementation changes downstream values; a clone override changes one vehicle only | planned |
| F2 | **Ground-to-sky column test**: a plane moves up through cells that get coarser | State shows altitude and cell size per z; a screenshot at ground and at altitude | planned |
| F3 | **Scale switch**: orbit to a system map, pick a body, land on a generated map (concept rows plus seed) | The landing map exists; leaving returns to the same spot | planned |
| F4 | **One scripted flight**: island, orbit, system map, other body, back | The script ends where it started; ledger lines recorded | planned |
| F5 | **Learning loop for behaviours**: weights tuned from the ledger; a planner proposes a new state; review file lists both; nothing kept without the owner | One proposal accepted, one rejected, both recorded | planned |
| F6 | **Microbiology as a concept domain** (R&D) | A concept page with weights; one R&D event reads it | planned |

## 4. Tangent documents (write each when its milestone lands)
Each gets: what was built, proof recorded, how to use it, how to modify it, what was not verified. Put it beside the code and link it from this file.

| Tangent doc | Written with |
|---|---|
| `Z-CELL-FORMAT.md` (the format, fixture, parse check) | M1 |
| `PHYSICS-IN-THE-CONCEPT-BANK.md` | M2 |
| `DECISION-LEDGER-AND-DAILY-REVIEW.md` | M3 |
| `CDDA-HQ-SPEC.md` | C0 |
| `VEHICLES-AS-PART-CELLS.md` | C3 |
| `DF-HQ-JOBS-AND-MOVERS.md` | D1-D2 |
| `GTA-HQ-DESIGN.md`, `DETAIL-LEVEL-OP.md`, `WORD-BANK-TO-IMAGE.md`, `IMG-HAI-DESIGN.md` | G0, G2, G3, G3a |
| `FLIGHT-AND-SCALE.md` | F1-F4 |
| `BEHAVIOUR-LEARNING-LOOP.md` | F5 |
| `MICROBIOLOGY-CONCEPTS.md` | F6 |

## 5. Order and dependencies
M1 first (paper). Then M2 and M3 can run beside C0/C1 and D1 (grok). C2 needs M1. F1 and F2 need M1 and M2. C3 needs C2 and parts. F3/F4 need F1/F2. G2 needs the brix parts. Nothing starts a later milestone before the earlier one's proof is recorded. Two agents never edit the same game's `ops/` at once.

## 6. Open items
Cell details at altitude; exact bank layout for physics rows; ownership of shared z code; the safety-rail list ("lets see as we go"); bitlands source and DF pack licences (for df-hq art); whether the TPMOS cataclysm-hq and dwarf-fortress copies are retired.
