# RPG-SPACE: collision and destruction (design, 2026-10-25)

**Status (2026-10-25 evening): the FIRST SLICE of section 3 rule 1-2 is built and run (section 7); destruction, mining/building and the environment layers (section 8) are NOT built.** Written for future agents after the owner asked (2026-10-25) "do we have collision codes yet?", "research how RPG Maker MV does collision vs Unreal", and "what about destructability (which we should have) ala minecraft?". Read this before touching ship/land collision, damage or breakable blocks. Related: `RPG-SPACE-LIFE-DEATH-RESPAWN.md` (the damage sink), `rpg-space-vehicle-entities/README.md` (ship and land entities), `RPG-SPACE-DEATH-RESPAWN-GRAVITY-NOTES.md`.

## 1. What exists today (verified 2026-10-25 by grep over engine, core, skin, docs)

- **Walking collision, 2D floors only:** `&.widgits/_shared-lib/petslike_core.c` `passable(x,y)` = inside the room AND `!floor_blocked(room,x,y,0)` AND `!other_at(room,x,y)`. A refused step answers "bump" (costs no turn). Shared by rpg-dust and rpg-space. Flat, no height.
- **Placement collision:** `floor_blocked` also answers "spot taken by #N" / "there is scenery there" when placing things.
- **3D engine:** `bv_render_3d.c` has NO walkable/wall registry (its own comment says so). `bv_menu_input.c` states gravity/collision only ever apply to what the xelector controls.
- **Ship vs planet:** NOTHING. In Direct flight the ship passes through the land. The grey debug cube (`debug_land=1`) is MEANT to be the MAP LIMIT END, i.e. the passability boundary for entities (owner 2026-10-25; the RPG Maker equivalent is `Game_Map.isValid`). Today it is drawn from the shown desk's volume only, so the planet sticks out of it: the boundary must enclose ship AND planet, and entities must not leave it. Nothing enforces it yet.
- **Damage sink that exists:** `rpg_space.+x life hurt <id> <n>` (and the death/respawn event pages) take damage from any caller.

## 2. How the references do it (research, shallow: web search only, no source read)

**RPG Maker MV: grid, passability is data.** Before a step the engine asks whether the destination is passable; blocked tile or a blocking event refuses the step. Per-tile passage flags in the tileset (X blocks, O passes, star passes behind); events have a "through" option; region IDs add terrain rules (plugins). Works because movement is cell by cell; single-point checks break once movement or sizes leave the grid. Sources: [Himeworks dev log](https://himeworks.com/?p=3420), [Steam discussion on passage symbols](https://steamcommunity.com/app/363890/discussions/0/1730963192535439663), [Rosedale plugin](https://www.dekudeals.com/items/rpg-maker-mv-rosedale-collision-altering-plugin). **Verified in the real MV source** (`rpg_objects.js`, copy at `~/Desktop/🤖️🪤️🏠️/xv.👨🏽‍🚀️RMMV+sec]linux<newline>🐧️]0002/RMMV_TSOTS]LINUX=elf?🧝🏽️/__.Tearrmv SpaceShop388.m/www/js/`; the folder name contains a newline, find it with `find ... -name rpg_objects.js -path '*SpaceShop388*'`; another copy: `~/Desktop/🤖️🪤️🏠️/GITS/rpg-atlas-stuff/rpg-maker-mv-og-mt/js/`):
- `Game_CharacterBase.canPass(x,y,d)` runs these checks IN THIS ORDER, first failure refuses the step: (1) `$gameMap.isValid(x2,y2)` = inside `0..width/height` = THE MAP LIMIT; (2) `isThrough()` (or debug through) returns true at once = ghost mode; (3) `isMapPassable` = `Game_Map.isPassable` on the tile you leave AND the tile you enter (reverse direction); (4) `isCollidedWithCharacters` = another NORMAL-priority event on that cell, or the boat/ship vehicle there.
- `Game_Map.checkPassage(x,y,bit)` walks the stacked tiles of a cell top to bottom: flag `0x10` (star) is skipped, the first tile whose bit is clear says passable, the first whose bit is set says impassable. Passability is DATA in the tileset flags, not code.
- `Game_Vehicle.isMapPassable`: boat and ship use their own water tile rules, and an AIRSHIP returns `true` always: it flies over everything, only the map limit and `isLandOk` (landing needs a land-ok tile and no event) stop it. This is the model for our Direct flight: free flight over the land, collision only where we say so (low altitude, solid cells, the limit).

**Unreal Blueprints: object types + per-pair response + events.** Every object has a type and a response to every other type: Ignore, Overlap (event, no block), Block (stops the move and fires Event Hit). Overlap fires On Component Begin Overlap; both sides must have overlap events enabled and neither may Ignore the other (a known pitfall). A Blueprint reacts to the event. Sources: [O'Reilly excerpt](https://www.oreilly.com/library/view/unreal-engine-physics/9781784394905/ch03s10.html), [Epic docs](https://docs.unrealengine.com/4.26/ja/BlueprintAPI/Collision/OnComponentBeginOverlap), [forum thread on custom channels](https://answers.unrealengine.com/questions/232105/view.html).

**Minecraft: destruction is per-block data + a progress counter.** Each block has hardness (base break time = hardness x 1.5 s with the right tool, x 5 s without), tools have a speed multiplier per material (wood 2, stone 4, iron 6, diamond 8, netherite 9, gold 12), progress accrues per game tick (speed / hardness / 30 with the right tool, / 100 without; whole ticks), and blast resistance (about 5 x hardness; bedrock and obsidian immune) decides what explosions destroy. Sources: [Minecraft wiki, Breaking](https://minecraft.gamepedia.com/Break), [PocketMine BlockBreakInfo](https://apidoc.pmmp.io/d3/d10/classpocketmine_1_1block_1_1_block_break_info.html). NOT found: loot-table and explosion-drop rules (do not invent them; they are our own design below).

## 3. Proposed design for our engine (fits the house: data in .pdl, behaviour in event pages)

**One rule, three layers, like RPG Maker + Unreal + Minecraft:**
1. **Rule (RPG Maker `canPass`, same ORDER):** map limit first (the grey cube), then a through/ghost flag, then solid cells, then other entities; before a vehicle move, test the vehicle's footprint at its altitude against solid cells (land overlay cells from `pieces/land_01`, other vehicles, placed blocks). Blocked = the move is refused (the ship stays), like `passable()`.
2. **Responses (Unreal, trimmed):** `collision.pdl` rows `COLL | typeA | typeB | block|overlap|ignore`, e.g. ship/land block, ship/ship block, ship/drop overlap, pet/ship overlap. Block and overlap each run an event page (`events/collision`, `events/overlap`), trigger parallel, exactly the death/respawn shape; the page calls verbs (`life hurt @actor n`, `helm bump`, `break <cell>`).
3. **Destruction (Minecraft):** `blocks.pdl` rows `BLOCK | tile | hardness | blast_resist | drops | breaks_into`, a per-cell damage counter in `state/damage.txt` (`room|x|y|z|progress`), progress added by an actor's tool/attack/impact each tick; at `progress >= hardness` the cell is removed (engine overlay deletes it), drops fall as items using the SAME drop mechanism as death (`state/drops.txt`, `life take`), and the cell may leave rubble (`breaks_into`). Explosions and ship impacts apply `blast` damage vs `blast_resist` to a radius of cells.

**Impact damage:** ship hits solid land = damage to the ship's crew (`life hurt`) scaled by speed (altitude change or cells per tick), plus damage to the cell hit if the cell is destructible. Toggle everything with `collision.pdl` rows `COLL | enabled | 1`, `damage_per_hit`, `destructible 0|1` (same style as `life.pdl`, re-read every 2 s).

**The grey cube is step 1 of that order.** It is the map limit end: ship and every entity are refused a step that leaves it (like `isValid`), and it must be drawn around ship + planet. A flying vehicle (airship rule) is free over the land ABOVE the solid cells and collides only when its footprint overlaps a solid cell at its altitude.

## 4. Where it would go

- Overlap test: `@.apps/rpg-space/ops/rpg_space.c` (skin; `sk_helm_verb` direct-flight move already knows footprint + `direct_z.txt` altitude) using the land cells the engine overlays (`bv_render_3d.c` `maker_load`, `pieces/land_01`). The skin cannot read engine cells cheaply today: first decide whether the skin reads the planet page file itself (cheapest) or the engine publishes a solid-cell mask (like `cam_home.txt`).
- Debug bounds: extend the grey wireframe (`g_land_box_on`) to ship + land so the box shows what collides.
- Removal of a destroyed cell: edit the page/desk the overlay reads, then reload; or an engine "removed cells" file; NOT built, check cost before choosing.

## 4b. Design principle (owner 2026-10-25): events / pal / pdl, and simulate only where asked

Keep every system (collision, damage, destruction, air, temperature, needs) as **event pages + .pal/.pdl data**, with C ops only doing the work a page calls. That keeps it malleable, scalable, customizable and abstractable. Fidelity is OPT-IN per entity or per case: we do not run cellular respiration (or any deep simulation) for every entity; the default is the cheap abstraction (a number that drifts, a need that ticks), and a special case can switch the detailed model on (a flag/row in its .pdl, or an event that attaches it). Build each system as a cheap default plus an optional detailed layer, never as a mandatory deep model.

## 4c. Tone and data shape for mining, chemistry (respiration) and thermo (owner 2026-10-25)

- **Fun first:** mining, ingestion/respiration and heat follow the Minecraft / No Man's Sky / Factorio feel: simple readable rules (a hardness, a tool speed, a gauge that fills and drains, a recipe, a belt), not lab simulation.
- **Room for realism in the DATA:** the records carry the extra fields from the start even when the default game ignores them, so a game developer who reuses or repurposes this can switch on a realistic mode without a schema change. Examples: a block row has `hardness` and `drops` (used) plus `composition` (compound list), `density`, `melting_point`, `specific_heat` (unused until a realistic ruleset reads them); a gas has `symbol`, `partial_pressure`, `toxicity`; an organism has a simple `breath` need (used) and an optional `metabolism` profile (unused by default; the opt-in detailed layer of 4b). Unknown columns are kept, never dropped (same rule as every .pdl in the house).
- **Rulesets, not forks:** the fun ruleset and a realistic ruleset are different .pdl files over the same state files; the engine and the event pages do not change.

## 4d. Future ship controls (not built, owner 2026-10-25)

More key controls will go with the ship, each an event page bound to a key in `keybinds.pdl`, using the same vehicle entity and verbs:
- **Mining beam:** hold to damage the cell in the beam line (the `mine` verb of roadmap step 3); drops go to the ship hold.
- **Self-shrinking:** the ship (and crew) shrinks to enter a microverse, to the planned "dustopia" scale (a scale change of the same entities, with its own map and its own environment layers). Needs a scale field on the vehicle entity and a way to hand a page/desk to the engine at another scale; no design yet.
- Others as they come (scanner, tractor, hull patch); each is a verb + a page + a key row, so adding one never touches the engine.

## 4e. More things to fly among (not built, owner 2026-10-25)

Later: **asteroids** (to mine) and **other ships** (to battle and trade with). Both fit what exists: an asteroid is a land-like entity (own `pieces/<id>` with position, size and a block model, destructible per `blocks.pdl`); another ship is a vehicle entity like `ship_01` (own state.txt, model, owner/faction, crew) driven by an event page or an AI pilot instead of the helm. Collision between them uses the same `collision.pdl` type pairs (ship/asteroid, ship/ship: block or overlap), trading and battle are event pages (hail, trade menu, fire = damage through the same `life hurt` / cell-damage sinks). Needs, before building: more than one vehicle entity and one land overlay in the engine at once (today the engine reads ONE `ship_01` and ONE `land_01`; `veh_1..9` names exist for vehicles, the land overlay is single).

## 4f. Everything registers as an entity; ships have an inventory (owner 2026-10-25)

- Asteroids, other ships and passengers all **register as entities** (same entity registry/links the rest of the house uses: id, type, position, owner), so any page, verb or UI can find them the same way.
- **A ship has an inventory that also lists the entities inside it** (crew, passengers, pets, cargo items), not only items. One list per ship, e.g. `pieces/<ship>/inventory.txt` (`kind|id|n|...`; kind = item or entity), the same shape as the characters' `inv=` and `state/drops.txt` so items move between a character, a drop and a ship hold with one mechanism.
- Why: ease of use and **macro storage UI** solutions (one hold screen, bulk move, "everything in the ship"), and so a passenger boarding or leaving is just an inventory move plus a position change (ties to the pets-as-passengers / door idea, and to respawn "on the bridge").
- **Decided (owner 2026-10-25):** an entity inside a ship has a **position, relative to the ship** (that is all; my earlier word "seat" was only a possible named spot like the helm console, not needed). When a ship is destroyed its inventory **drops** (same mechanism as death, below). **Capacity is a .pdl number** for now.
- Not built. This round is infrastructure/scaffolding for later detail: everything stays events / blueprints / pdl based (4b).

## 4g. Remnants: how destruction, mining and death all leave things behind (owner 2026-10-25)

One mechanism for everything: **when something is destroyed it leaves "remnants"**, small items (mini blocks) that **sit where they fell until collected**, like Minecraft. A remnant may or may not be damaged. This is how mining works (a mined block leaves its remnants), how a destroyed ship or asteroid works (its inventory and parts drop), and how death already works (`state/drops.txt`, half the stacks, anyone can take).
- **Data:** a remnant is a drop row (today `id|room|x|y|n|item|0`) with room for a condition/damage field and a source (what it came from); collecting is the existing `life take`-style verb. Keep unknown columns (4c).
- **Flow, all event pages:** destroyed event -> a page decides the remnants (from `blocks.pdl` / `drops` of the thing, or the inventory for a ship or character) -> the drops are written -> they are drawn as mini blocks in the scene (NOT built: drops are still not drawn today) -> a collect action moves them into an inventory. Whether a remnant is damaged can come from the damage the thing had (a .pdl rule).
- **Infrastructure first:** do not hard-code per-thing rules; each destroyed type names its remnants in its .pdl row, and a generic `destroyed` page turns the row into drops.

## 5. Rules for whoever builds it

- Verify with a 3D frame ("if you didn't see it in 3D it doesn't exist"): ship blocked at the planet, a cell removed, a drop appearing.
- Negative-test: collision off must let the ship through; a destroyed cell must really be gone after reload.
- Put numbers in .pdl, behaviour in event pages; do not hard-code hardness or damage in C.
- Do not extend the old slide approach (rewriting the book per step); entities and overlays only ([[ship-and-land-are-entities]] direction).

## 6. Owner answers (2026-10-25) - these are decisions, not questions

1. **Crew damage:** every hit costs the crew a very slight amount, more at speed; it is mainly there to register the ship's damage for now. The percentages change later through .pdl and events (built that way: `collision.pdl`).
2. **Destructible:** (not answered directly; mining/building is the goal, so design for per-block destruction of land and hull, flagged per block in `blocks.pdl`).
3. **Mining and building are A MAJOR GOAL** of the game, not an extra.
4. **Environment is a goal too:** oxygen / gas levels (chemical-compound level events or entities, "or other?"), temperature, thermonuclear (reactor / star heat) as needs and levels. A destroyed hull cell opening to vacuum is part of this (answer to the old vacuum question: yes, via events).

## 7. First slice BUILT and RUN (2026-10-25)

- `@.apps/rpg-space/collision.pdl` (re-read every 2 s): `enabled`, `block`, `contact_alt` (-4), `reach` (12), `crew_base_pm` (5) and `crew_speed_pm` (5), per-mille of max hp (minimum 1 hp).
- `rpg_space.c` section 75: `sk_coll_check(nx,ny,nz)` is called by `helm move` (after the map-limit check, which already existed as `direct_max`) and `helm lift`. Hit = ship altitude <= contact_alt AND |offset| <= reach around the planet centre. A hit logs `COLLISION|time|ship|x,y,z|speed n|blocked`, plays `events/collision` (trigger parallel; the page runs `collision hit`; fallback = the effect directly), refuses the move when `block=1`.
- Effect (`collision hit [speed]`): every crew member loses `max(1, mhp*(base+speed*spd)/1000)` hp through `life hurt`; `state/ship_damage.txt` (`hits= dmg= last=`) is the ship's own damage register. Speed 1..4 from the time since the previous helm action. `collision` alone prints the status.
- RAN: lowering from altitude 11 to -5 and -6 was refused both times, 2 COLLISION rows, the page ledger shows "Play event page_1 (trigger=parallel) for collision", crew lost 4 hp (hero, 450 max) and 1 hp (pets, 20 max) per hit at speed 1, register `hits=1 dmg=1`.
- **NOT verified / NOT built:** `contact_alt -4` and `reach 12` are GUESSES (calibrate: lower the ship in a low side camera and find where the hull meets the land; then set them); land is a square, not per-cell solids; the grey map-limit cube is still drawn from the desk volume and does not enclose the planet; no horizontal collision with tall land features; ship vs ship/drops/other entities; Galaxy mode.

## 7b. Infrastructure built and RUN (2026-10-25, second slice): mining, remnants, destroyed page, ship hold

All data + event pages; C only does what a page calls (4b). Files in `@.apps/rpg-space/`:
- `blocks.pdl`: `BLOCK | kind | hardness | drops | count | damaged_pct | composition | density | melting_point` (the last three are kept for a realistic ruleset, unused) and `TOOL | name | power` (hand 1, pick 2, beam 4). `hold.pdl`: `HOLD | capacity | 40`, `HOLD | wreck_drop | 100`. Both re-read every 2 s.
- Verbs (`rpg_space.+x`): `mine <kind> <room> <x> <y> [tool]` adds the tool's power to a per-cell progress (`state/damage.txt`); at hardness it writes `state/destroyed.txt` and plays `events/destroyed` (trigger parallel; fallback = the effect directly), whose page runs `destroy last`. `destroy <kind> <room> <x> <y> [source]` turns the BLOCK row into a remnant. `hold status | give | board <id> [x y] | leave <id> | store <char> <item> [n] | fetch <char> <item> [n] | wreck`.
- Remnants = rows of `state/drops.txt`, now `id|room|x|y|n|item|0|damaged|source`; collected with `life take <id> <char>` (a damaged remnant arrives as `<item>_damaged`). Death uses the same rows (source `death`).
- Hold = `state/hold.txt` (`item|<id>|n|0|0` and `entity|<id>|1|x|y`, entity position relative to the ship); capacity from hold.pdl; `hold wreck` drops `wreck_drop` percent of every item stack as damaged remnants at the respawn spot and releases the entities (log row WRECK). Log rows DESTROYED, BOARD, LEAVE, WRECK in `state/events.txt`.
- RAN: 3 beam strikes on `ore` (hardness 5): 4/5, then broke (destroyed-page ledger shows "Play event page_1 (trigger=parallel) for destroyed"), DESTROYED row, 3 iron lying at bridge 7,6; harold collected them; 12 `hullplate` destroyed gave 5 damaged of 12 (40% rule) collected as `scrap` 32 and `scrap_damaged` 20; hold: give, store, board with relative position, leave, fetch, wreck (6 items fall as damaged remnants, 1 entity released). Bug found on the way: every verb is a fresh process, so `rand()` repeated the same first number (8 undamaged in a row); seeded in the effect.
- **NOT built:** finding the block under a cell from the engine (`mine` is told the kind), drawing remnants in the scene (they are only rows and a `drops` count; the owner cannot see them yet), a beam KEY and its page, building/placing blocks, the grey map-limit cube, collision with other entities, ships/asteroids as multiple entities, the environment layers.

## 7c. Remnants drawn in the scene + auto pickup, BUILT and RUN (2026-10-25, third slice)

- **How they are drawn:** every remnant row of `state/drops.txt` becomes one extra row in its room's `events.txt` (`x y r g b rem <idx> <dir> 0 *`), the SAME path the pets and the hero are drawn by (skin section 43 appends them after the hero row so footer ids do not move). Each item kind and damaged/undamaged state has a frame `rem_<idx>_<dir>_0.rgba` (16x24, a small block at the foot of the cell, written on first use into `<ASSETS>/../tsots-characters/frames/`), so it shows as a mini block half a cell wide. Colours by item (stone grey, iron rust, gem cyan, scrap steel, bread, fish); damaged ones are darker.
- **Revolve (owner 2026-10-25):** a lying remnant cycles through four facings (wide, three-quarter, edge-on, three-quarter) so it looks like it turns, to show it can be picked up. `PICKUP | spin_ms` in `blocks.pdl` (220; 0 = still). RAN: the events row cycled dir 2,6,4,8 within one second.
- **Auto pickup:** any living character within `PICKUP | range` cells (2) of a remnant picks it up by itself; the remnant slides cell by cell towards the character for `PICKUP | ms` (600), then the take runs (`life take`) and it is gone. RAN: a remnant created next to the crew was taken by Solvent within a second (TAKE row). NOT seen: the visible slide across several cells (the taker stood on the same cell), the damaged one arrives as `<item>_damaged`.
- **Daemon:** `sk_remnants_tick` every 90 ms in the daemon tick (skin section 79); it only rewrites events.txt when a row changes (the spin changes it about 4-5 times a second while a remnant lies there) and nudges the engine. Daemon CPU measured 5% with one remnant lying, 2% without.
- **Tried first and failed (do not repeat):** drawing remnants as engine VEHICLE pieces (`pieces/veh_1..9`). The engine loaded them (a temporary debug line showed `loaded=3`, correct position and size, and the box count was far below the limit) but nothing was drawn in the rpg-space scene, not even a 4x4x4 test block. Cause not found. Also the hero piece's `pos_z` is NOT the standing level (it resets after a reload; it read 9 not 17): a tuning value `remnant_z` (17) was added for the standing level, currently unused by the events path.
- **System observation (same day):** with the rpg-space and an rpg-dust board both open the machine forked about 190 processes/s (load average 7). The forks come from four `prisc+x` board loops (two per board) each spawning `bv_dispatch.+x` and `sh`; not from the skin daemon. Closing an unused board removes half of it. A stale 15-hour-old rpg-space stack was among them.
- **Still NOT built:** a beam key, `mine` finding the block under a cell, placing blocks, several ships/asteroids at once, the environment layers, the grey map-limit cube enclosing the planet, collision calibration.

## 7d. Mining key, resource nodes, map-limit cube (2026-10-25, fourth slice)

- **Resource nodes (answers "mine finding the block under a cell"):** `blocks.pdl` rows `NODE | room | x | y | kind | amount` (amount -1 = endless) seed `state/nodes.txt` once (delete the state file to reseed). A node is drawn in the scene (a bigger block than a remnant, colour of what it yields, frames `rem_4x_<dir>_0.rgba`). Breaking one gives one load of remnants and lowers its amount; at 0 it disappears. The engine does not need to know the block under a cell: the nodes ARE the minable things (fun-first, Factorio-style ore patches); cutting walls/planet cells later would add rows of another kind.
- **Beam / mine key:** `m` (relay code 109, row `KEY | mine | 109` in `keybinds.pdl`). The skin runs `mine here`: the node in front of the controlled character (or under it); the tool is the pick on foot and the ship's `beam` at the helm (`flag helm`), power from blocks.pdl `TOOL` rows. Verbs: `mine here [tool]`, `mine nodes`, `mine <kind> <room> <x> <y> [tool]`. RAN: `mine here` x3 with the pick broke an ore node (2/5, 4/5, broke; amount 2 -> 1; damaged iron made); four lines of `109` appended to `state/interact_relay.txt` broke a rock node twice (3 -> 1), DESTROYED rows logged. NOT verified: a real keystroke `m` from hardware reaching the skin (xdotool could not be aimed at the board window; the khtpm window should forward every key while it is armed, per its own comment), and the beam variant at the helm.
- **Map-limit cube:** the engine's `debug_land` box (grey wireframe) is now the whole map limit: the planet page and its eight neighbours, 48 x 48 cells (`px-16 .. px+32`), height 0..34, moving with the land, so the hull touches a wall at the `direct_max` limit. Was: one page, height 0..20 (the planet stuck out of it). Toggle `debug_land=1` in view.pdl. NOT verified visually: a high camera showed only a faint grey edge along the top of the frame (thin grey lines in a dark scene); a brighter or thicker line would help. The ship is still stopped at the limit by the move check, not by the cube.
- **Collision contact values still uncalibrated** (`contact_alt -4`, `reach 12` in collision.pdl).

## 8. Next, in order (proposal)

1. Calibrate `contact_alt` and `reach` with frames; make the grey cube the real map limit around ship + land.
2. Per-cell solids: read the land cells (planet page `sk_cdep` columns) instead of a square.
3. Destruction: `blocks.pdl`, `state/damage.txt`, a `mine <cell>` verb (tool, hardness, ticks), drops via `state/drops.txt`, and placing blocks (`build <cell> <block>`), all as event pages.
4. Environment layers as their own small state, per room/cell, with events: `air.txt` (oxygen, gases by compound), `temp.txt`, a heat source entity (reactor/star); needs on characters (breathing, cold/heat) feeding the same `life hurt` sink. A broken hull cell leaks air to the neighbour cell. Keep each as a plain file + one `.pdl` of constants, like `life.pdl`.
