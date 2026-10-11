# Reusable building blocks and tools for making different kinds of games

Status: DESIGN, written 2026-10-07 by claude. Nothing here is built except where "exists" says so.
Owner (2026-10-07), answering "how do you want to do these? different starting players? events? design docs first?": **"reusable building blocks for different kinds of game creation; tools."** And: "noise generated biomes?"
This is the organizing rule for everything designed this week (DSR, solar sandbox, planet physics, real-time 3D planet, mutaclysm-as-events, survival gathering, play economy, sessions/lobbies): **build each as a reusable block that a game page assembles, never as one game's private code.**

## 1. The shape
A **block** = (a) data rows that configure it, (b) an event page or small compiled op that does one job, (c) a pal harness that proves it and can fail, (d) a short doc. A **game** = a page that lists which blocks it uses and their settings (`game.pdl` `SETUP`/`GAME` rows) plus content data. A **tool** = an X11-HQ editor/viewer for a block's data (tomom-hq style: bounded steps, audit rows, undo; read-only viewers like concept-bank-hq). Different games (DSR, a survival planet, TSC duels, chess) differ in **which blocks and settings**, not in code.

## 2. Block catalog (what each is, status, doc)
| block | does one job | status |
|---|---|---|
| **Clock** | stepped phases or real-time fixed timestep + time-scale, per page | designed (needs-and-care, 3D doc); stepped only |
| **Physics node** | gravity / orbit / temperature / pressure as swappable, linked event pages | designed (SOLAR-SANDBOX section 8) |
| **Noise + terrain** | seeded noise fields (height, temperature, rainfall) | **not built** (see section 3) |
| **Biome table** | classify a tile from derived fields into a biome | not built (section 3) |
| **Chunk world** | lazy voxel chunks from a seed | one-biome hash version exists (`pc_generate_chunk.c`) |
| **Inventory + items + recipes** | one inventory, one craft command, item data | designed (mutaclysm doc); phone/inventory folders exist |
| **Needs** | hunger/thirst/hygiene/sleep as per-place day-tick rules | designed |
| **Gather/yield timer** | interact, wait, yield | designed (Grok handoff) |
| **Market** | goods, prices, trade, loans, rent, stocks (WSR ops via entity dirs) | WSR ops built; entity wiring designed |
| **Government + vote** | tunables, ballots, elections | designed |
| **Social ties + graph** | pair scores, graph snapshot, views | seed exists (`relations.pdl`); rest designed |
| **Session/seats/lobby/rating** | players, agents, peers, Elo, lobby ledger | TSC_ELO has parts; pattern designed |
| **Currency/chain/escrow/faucet** | play money, test chains, locks, rewards | **built in alpha** (PAL-CHAIN-MULTICHAIN-ESCROW-FAUCET-DESIGN) |
| **Map transfer / doors** | pages, access list, travel rows | exists (transfer op, `game.pdl`) |
| **Asset pallets** | textures/tiles by reference | exists (pallets.pdl, Mineclonia + CDDA sources) |
| **Harness** | pal cases, verdicts | exists |
| **Tools** | editors/viewers: tomom-hq, concept-bank-hq, pallets | partly exist |

## 3. Noise-generated biomes (answer)
**What exists is not noise.** `pc_generate_chunk.c` is deterministic and seeded per coordinate with an integer hash, but it generates **one biome (plains)**, no variety; mutaclysm's `biome_pass` (`02-procgen-design.txt`) says itself it is a **stochastic patch scatter, not real Perlin/simplex noise**. So real noise biomes are a **new block**:
- **Noise block** (pure function, one text-included `.c`): seeded **value or simplex noise** with octaves (fractal), `noise(seed, x, y, z) -> [0,1]`; the same (seed, coordinates) always gives the same value, so chunks regenerate identically (the property `pc_generate_chunk` already insists on). On a sphere, sample the noise in **3D** by the voxel's position so there is no seam.
- **Fields:** several independent noise fields (different seed offsets): **altitude**, **temperature** (combined with the derived latitude/insolation node so poles are cold and the equator warm), **rainfall/humidity** (combined with the derived weather/humidity node and distance from water).
- **Biome table (data, not code):** `BIOME | name | alt=min..max | temp=min..max | rain=min..max | surface=<block> | subsurface=<block> | tree=<kind>,<density> | ...` (the classic Whittaker-style lookup: cold+dry tundra, warm+wet rainforest, etc.). Biomes are rows in the planet's data and the table is **swappable per planet** like any node (a Mars table is mostly desert/ice).
- **Blocks link to pallets:** each biome row names Mineclonia blocks (`piececraft` pallet) and CDDA-style items for vegetation/loot, so art and content stay by reference.
- **Derived inputs come from the physics nodes** (temperature, rainfall), so changing a planet's mass, orbit or atmosphere changes its biomes with no generator change.
- **Harness:** same seed gives byte-identical chunks; noise range in [0,1] and continuity (neighbors differ by a bounded amount); a biome-table swap changes the classified tile; a temperature-node swap moves a biome boundary.
- **Cost:** generation per chunk, only where the player is; measure on this machine before committing sizes.

## 4. Tools (the editor side of the blocks)
Each block gets, when it is worth it, one X11-HQ window in the existing three-part shape (xhtpm layout, css, one compiled manager): **tunables editor** (physics nodes, biome table, clock rates), **seed/preview viewer** (a small picture of a generated planet or chunk, the one you wanted beside "time"), **ledger viewers** (market, votes, escrow). They follow the tomom-hq rules: default to a scratch target, audit rows, undo, no live-data writes without a backup. A **game-maker** hub window later lists blocks, shows a game's `SETUP` rows, and launches a game page, so "creating a game" is choosing blocks and filling data.

## 5. Who does what ("different starting players?")
- **Docs/specs first** for each block (done for most), then **one block at a time**, each with a harness; I use a fresh tight-prompt agent per block (as with the chain build) and keep the lanes separate.
- **Lane proposal:** claude (manager): block specs, sandbox/doors, chain/economy blocks, noise/biome block spec; **Grok:** real-time clock, spherical gravity movement, voxel chunks, gathering/farming/water (handoff docs exist); **the opencode lane** keeps worker/fetch/row-projection work and should not touch these files.
- **Starting players** (the game-side question) are just **seat rows** in setup (human, agent, peer), same blocks for every game.
- **Events:** every block is event data/pages wired by `LINK` rows; no block hardcodes a game.

## 6. Build order (suggested)
1. Write the block interface rule (block header: inputs, outputs, tunables, harness name) and a catalog file (`blocks.pdl`) listing the table above.
2. Physics node format (`NODE`/`LINK`) + evaluation op, with gravity and orbit as the first two nodes.
3. Noise block + biome table + a scratch chunk picture to compare against `pc_generate_chunk` output.
4. Real-time clock source (Grok).
5. Tunables editor window for physics nodes and biome tables.
6. A first assembled game: the solar sandbox page using blocks 2-5.

## 7. Open questions for the owner
1. Is a single `blocks.pdl` catalog (with a viewer window) the right "game creation" entry point, or do you want a separate game-maker app first?
2. Biome count and style for the first planet (earthlike ~8 biomes proposed)?
3. Value noise (simple, cheap) or simplex (smoother, a bit more code) first?
4. Should block ownership follow the lane split above?

## 8. Plugins, events or ops? (owner question, 2026-10-07)
The house has already ruled on this (khtpm standards, `PRISC-OPS-ARCHITECTURE.md`): **no `.so`/plugin loading, no linking to share behavior.** So a "plugin" here is an **op + event data**, never a loaded library:
- **Behavior that changes per game = event pages + data rows** (what the game does, wired by `LINK`; swappable per page/planet/system). This is the default for nearly every block above.
- **Heavy or stateful work = a compiled op** (one verb, self-contained, called from an event page via `exec`; stateful ones are separate ops reading ledgers by cursor). Built-in physics nodes, noise, chain, escrow are ops; an event page may override them.
- **Pure shared math = a text-included `.c`** (noise, orbit, gravity formulas), never a header+link.
- **"Plugin" for outside authors** = a folder carrying `NODE`/`BLOCK` rows + event pages + (optionally) its own op binary + a harness, dropped into a game. Registration is a row (the event command registry), not code loading.
Rule of thumb: try data/events first; add an op only when speed or state demands it; add a shared include only when two or more ops need the same pure function.

## 9. Recipes in Canvas-Craft (owner question, 2026-10-07)
**Canvas-Craft is the house crafting bench** (`CANVAS-CRAFT-DESIGN.md`, 2026-09-06): a Satisfactory-style 3-panel UI over the **quark -> subatomic -> element -> compound -> bio recipe registry** (`elements]new=RECIPEZ+]z2.txt`, one recipe per line, parents by line index), with `canvascraft_items.pdl` (item prices/start stacks), a manager (`ops/canvascraft_manager.c`), a launcher (`open_canvas_craft.sh`) and a `user-pallet` category in `pallets.pdl`. I have only read its design header and file list, not the manager code, so I do not know how complete the built part is.
**Yes: make it the one recipe source** for the blocks above, instead of mutaclysm's `recipes.txt`, the Grok crafting step and shop goods each having their own:
- A **recipe block** = rows `RECIPE | id | out=<item>:<n> | in=<item>:<n>,... | tool=... | time=<ticks> | station=...` (`.pdl`), read by one `craft` command (the one in the mutaclysm and gathering designs).
- **Layers on one tree:** the existing chemistry tree (atoms -> compounds -> bio) as the base layer; **survival recipes** (CDDA-style items, Mineclonia-style blocks) and **planet recipes** (refining, fuel for the rocket cost from the physics nodes) as added layers; **economy goods** (DSR commodities: food, water, clothes) are recipe outputs, so supply chains fall out of recipes.
- **Canvas-Craft stays the viewer/editor** (the tool): browse a recipe, see inputs/outputs, edit recipe rows with audit rows and undo; the harness replays crafts.
- **Physics tie:** `time`, `tool` and `station` can reference derived values (heat from the temperature node, pressure for boiling), so a recipe can behave differently on Mars.
- **Next step:** read `canvascraft_manager.c` and the registry file, write the `RECIPE` row schema as a superset of the existing line format (with a converter, no loss), and add harness cases that replay the current registry through the new `craft` command.
