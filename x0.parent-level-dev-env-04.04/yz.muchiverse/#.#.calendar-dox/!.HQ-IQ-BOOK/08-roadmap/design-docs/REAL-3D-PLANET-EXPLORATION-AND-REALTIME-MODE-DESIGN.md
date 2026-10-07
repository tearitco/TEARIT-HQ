# A real 3D planet you can walk on, and a real-time ("GTA mode") clock

Status: DESIGN + Grok handoff, written 2026-10-07 by claude. Nothing here is built.
Owner (2026-10-07): "gravity, grass, rainfall; I want a real 3D planet we can actually explore using a Minecraft-like player; use Mineclonia assets and CDDA assets (explain this to the Grok handoff, it will understand), but also use macro gravity and movement when in 'GTA mode' real-time, not 'roguelike' or 'market stepping'."
Builds on: `SOLAR-SANDBOX-AND-PLANET-PHYSICS-DESIGN.md` (planet numbers -> derived gravity/climate/weather), `MUTACLYSM-AS-EVENTS-DESIGN.md`, `SURVIVAL-GATHERING-FARMING-MINING-WATER-HANDOFF.md`, `PCHQ-3D-RAYCAST-AND-VOXEL-HIGHLIGHT-DESIGN.md`, memory notes on the GPU raymarch daemon (about 17 fps live).

## 1. Two clocks, one world (the key decision)
The world's events are the same in both modes; only **who advances time** differs:
- **Stepped mode** (roguelike / market): time moves when an action happens or on a coarse tick (the DSR day/phase clock, mutaclysm turns). Good for the economy, fast-forward, headless and harness runs.
- **Real-time mode** ("GTA mode"): a **fixed-timestep simulation** (proposed 20-60 ticks per second, independent of the frame rate) advances continuously; the player moves, falls, jumps and swims in real time; weather and growth run at a **time-scale** (e.g. 1 real second = N game minutes). The **stepped economy rides on top**: the day/phase ticks of the DSR design are driven by game time, so the market keeps its slow cadence while the body moves in real time.
Rule: both modes write to the same ledgers and use the same derived physics. A mode is a clock source (a `SETUP` row), never a second implementation of a mechanic. Real-time is **opt-in per page**, because this machine is CPU-limited (background pages stay stepped or frozen).

## 2. The planet as a real voxel world
- A planet is a **ball of voxels** generated from its numbers and a **seed**: radius and mass from the body row, terrain from layered noise, water from the water fraction, biomes from latitude x temperature x rainfall (all derived: see the physics design). Chunks are generated **lazily** around the player (`pc_generate_chunk.c` exists in pc-hq; reuse its chunk idea, check its format first).
- **Spherical gravity:** "down" points at the planet's center; strength is the derived surface gravity g = G M / R^2 (so the Moon feels light and Mars lighter than Earth). Jump height, fall speed, fall damage, carry weight and projectile arcs all use that g. Near the surface of a large planet a flat local approximation per chunk is acceptable; the curved view/walk-around-the-horizon is a later layer.
- **Grass, rainfall, weather:** grass/vegetation growth reads derived temperature, insolation, rainfall and soil water; rain events come from the weather bands; water collects and flows in simple voxel rules; all as event rules over derived values, with the strength dials (0 = off, 1 = real) from the physics tunables.
- **Player:** a Minecraft-like first/third-person body: walk, jump, swim, place and break voxels, pick up and craft, with the inventory and needs systems of the house (no second copy). Real time at the fixed timestep above.
- **Viewer:** the GPU raymarch daemon (`bv_render_3d --daemon`, see memory notes) is the existing 3D path; whether it can show a planet-scale chunked world at playable frame rates is **unmeasured**; measure before promising. The renderer rule stays: no per-app code in `khtpm_core_render.c`.
- **Transitions:** the system page (solar sandbox) is the hub; `Enter <planet>` lands the player on that planet's surface page; `Leave orbit` returns. Launch/landing costs come from derived escape velocity and delta-v.

## 3. Assets: what Mineclonia and CDDA are, and the licensing caution
- **Mineclonia** is a Luanti (formerly Minetest) voxel game, a continuation of **MineClone2**: a Minecraft-like block game (blocks, items, mobs, crafting, farming, biomes) whose **block textures, models and item/node definitions** are the thing we want. It is **not installed on this machine** (I searched; found nothing). The owner or Grok has to supply a download. **Licensing:** its code is free software and its media carries **per-file licenses (typically Creative Commons, some requiring attribution and share-alike)**; verify the exact terms from the download before copying any asset into the repo, keep the upstream license/credits file beside any imported asset, and record the source URL and version. Do not copy assets before that check.
- **CDDA (Cataclysm: Dark Days Ahead)** is the turn-based survival roguelike that mutaclysm is modeled on. Two things are useful: its **JSON game data** (items, recipes, monsters, terrain, vehicles; licensed CC-BY-SA-3.0 per mutaclysm's own doc) as a **content/rules reference**, and its **tilesets** (2D sprite sheets; each tileset has its own license) as 2D art. A full source tree is referenced in the house (`catacylsm.DDA-0.F-dev.../0001/data/json/`, per `01-cdda-architecture.md`, which also says: read only the specific category file you are porting). The house already has a CDDA-shaped game (mutaclysm) and a CDDA emoji video.
- **How the two combine:** Mineclonia supplies the **3D block look** and block/item vocabulary (textures per node type); CDDA supplies **survival content data** (item stats, recipes, monsters) that becomes **event/recipe data (`.pdl`)** per the mutaclysm and gathering designs. Mapping tables (`BLOCK | <mineclonia node> | <house item> | texture=...`, `ITEM | <cdda id> | <house item> | ...`) keep the two vocabularies separate from our own ids.
- **Attribution and keeping them separate:** imported assets live in a clearly named folder with their license files, never mixed into our own data; our `.pdl` rows reference them by path; replacing an asset must not change gameplay.

## 4. Handoff for Grok (read this paragraph first)
Grok: the owner wants a **real-time, explorable, Minecraft-like 3D planet** in the house, using **Mineclonia (Luanti block game) assets for the look** and **CDDA (Cataclysm DDA) data for survival content**, while the **economy stays stepped**. Concretely, in this order: (1) **confirm asset sources and licenses** (get the Mineclonia download, read each media license, write a `THIRD-PARTY-ASSETS.md` with source/version/license/attribution; same for the CDDA JSON and a tileset if used); (2) build the **real-time fixed-timestep clock** as a clock source for a page (stepped mode unchanged) with a harness proving the stepped and real-time runs give the same results for the same events and seed; (3) build **spherical-gravity movement** (derived g, jump, fall, swim) for a player body on a small test planet; (4) **chunked voxel planet from a seed** with a node-type table mapping Mineclonia nodes to house items; (5) feed **grass growth and rainfall** from the derived values; (6) only then performance work (measure the raymarch daemon on chunked data). Follow the house rules in `SURVIVAL-GATHERING-FARMING-MINING-WATER-HANDOFF.md` section 3 (compiled ops, no shared headers, append-only ledgers, no shell mechanics, harness that can fail, own branch/paths, rehearse data in beta, do not touch `xyzfs/users`). Do not rebuild hunger/inventory/crafting: they exist or are being designed once. Report what you could not verify (no GUI test) instead of claiming it.

## 5. Build order
1. Asset sources + `THIRD-PARTY-ASSETS.md` (no code).
2. Clock source: fixed timestep + time-scale, stepped unchanged; harness.
3. Derived gravity function (from the planet physics design) + a movement op using it; harness vectors (Earth, Moon, Mars jump/fall).
4. Chunk generator and node table; viewer measurement.
5. Grass/rain/weather rules over derived values.
6. Link to the solar sandbox pages (enter planet = land on this world).

## 6. Open questions for the owner
1. First-person, third-person, or both?
2. Real-time update rate and the game-time scale (1 real second = how many game minutes)?
3. Can you provide the Mineclonia download (or point me at where it is), and which CDDA tileset, if any?
4. Is multiplayer in this world in scope (the session/seat pattern) or single-player first?
5. Flat-per-chunk gravity first (recommended) or a truly curved horizon from the start?
