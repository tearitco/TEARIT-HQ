# Games: owner decisions and vision (2026-10-13)

Source: a Q&A between the owner and claude on 2026-10-13. Labels: **SAID** = the owner's words or choice. **READ** = from house docs or files, not run. **MINE** = claude's inference or recommendation, not yet confirmed. The full dated Q&A is `XO/25.games-plan/2-OWNER-QA-LOG.md`. The roadmap that uses these is `GAMES-ROADMAP.md` in this folder.

## 1. The principle (SAID)
Everything in the house should one day work this way: rules live in concept banks and weight banks, are read from there, can be cloned or evolved for a purpose, and the system learns, repairs and develops itself toward the vision, or at least builds systems and saves tokens. Games are where it is proven.

## 2. Who builds what (SAID)
- **grok:** df-hq mechanics and the CDDA rules (crafting, vehicles, map generation, NPCs and enemies, stats, inventory, menus). Grok knows both games better than claude.
- **claude:** the infrastructure under them, the integration notes (what the house engine already gives each rule, what is missing, one proof each), and the gta-hq design with the owner.
- A scripted **playthrough** of each of cdda, df and gta is the target.
- gta-hq is not frozen. Grok keeps its code and starts no new gta work until the gta design exists.
- Pace: slow is fine.

## 3. z-levels (SAID)
z-levels are very important (planes and spaceships need them).
- **Meaning: both.** Detailed z near the ground (cellars, floors). Beyond that the scale changes for sky and space. Detail is derived by a renderer op from distance.
- **Cell size is flexible.** At altitude a cell can stand for a region. Even on land a large building can be one region of 1 or 4 tiles or whatever. Keep one standard, use sense. **Scale is a concept in the concept bank.**
- **Rotation:** keep it simple now (90-degree steps), leave room for free angles.
- **Shared things** follow house convention: inline for one user; text-include for pure code with two or more users; op + fork + IPC for stateful code with two or more; never header + link.
- The z format is designed on paper before any code (roadmap M1).

## 4. Vehicles (SAID)
- A vehicle is a **grid of cells in the same cell format as a building**, each cell holding a brix part. Brix socket rules decide what may attach. Cars, planes and spaceships are one system.
- Like CDDA: a vehicle is a mini moving building. You walk to a seat. Engaging the driver's seat starts it.

## 5. Scale (SAID)
Both a **seamless climb** (island to orbit, cells coarser as you climb) and a **scale switch** (a system map where you pick a body and land). **One scripted flight proves both:** island, orbit, system map, other body, back.

## 6. Physics (SAID, with READ context)
- **Hybrid.** Every physical rule is tied to a concept and a weights bank, read from there, clonable and evolvable.
- **Base: extend `&.widgits/physics-nodes`** (READ: `phys_node_eval.c`; rows `NODE`, `LINK`, `TUNABLE`, `OVERRIDE`; scope page > planet > system > default; Earth/Moon/Mars vectors; harness `phys_nodes.pal`). Design: `SOLAR-SANDBOX-AND-PLANET-PHYSICS-DESIGN.md` sections 3 and 8.
- Not built (READ): flight nodes (thrust, drag, lift), a link from a node's tunables to a bank, any vehicle-as-parts physics.
- **Bank = the existing concept bank (MINE, "house standard", owner agreed to ask):** `&.widgits/concept-bank`: masters and spokes with weighted slots; changes arrive as candidate `EDIT` lines checked by `concept_edit_validate`; a human promotes; nothing is written directly. A physics rule is a concept; its weights are slot values; a clone is a candidate edit with its own scope.
- **Microbiology** is wanted as another concept domain, important for R&D (SAID).

## 7. Behaviour and learning (SAID)
- NPCs, enemies and vehicles are **FSMs whose transitions are events**. Logged outcomes tune the transition weights. **A planner may propose new states** where the FSM has no rule. Every change is reviewed by the owner before it is kept (no auto-promotion).
- Every decision and outcome appends one line (actor, state, action, outcome) to a marker ledger. That ledger is the training data and the review source. See `13.agent-coms/2026-10-13/NOTE-FROM-CLAUDE-to-opencode-and-grok-pipeline-direction.md`.
- **Review unit** (MINE, owner said "keep a roadmap and tangent documents"): one review file per game per day, one line per proposal.
- Rails: the system may not change the clock, wallets, user data or island physics constants without the owner (owner: "lets see as we go", so this list is provisional).

## 8. gta-hq art (SAID)
Larger island, size chosen from a menu. Better pixel graphics and animation. Parts (`.brix`-style) that evolve and scale; **a middle renderer op derives the detail level from parts, distance and a detail setting**, so there is no separate close/far art file. GTA-level look comes from Stable Diffusion **trained by hand with word banks and concept banks**, hand-tuned outputs, delegated where possible to tomom / gemma / API workers, **claude as final judge only**. Nothing is generated at runtime.

## 9. Far-away and landing (MINE, awaiting owner)
- **Far-away vehicles:** full detail only near the viewer; everything else on a coarse clock using cheap formulas from the physics nodes (for example an orbit position from the Kepler node).
- **Landing maps:** generated from the body's concept rows plus a seed, with hand-authored overrides only for the places that matter.
- **First z build:** cdda-hq (a building with a cellar, floors and a stair), plus a small second test of a ground-to-sky column with a plane or ship so the format is not building-only.

## 10. Not decided
Cell format details at altitude; the exact bank file layout for physics rows; who owns the shared z code; the safety-rail list.
