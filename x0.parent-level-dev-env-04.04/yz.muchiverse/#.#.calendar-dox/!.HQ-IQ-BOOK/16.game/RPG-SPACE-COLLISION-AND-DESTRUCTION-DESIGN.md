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

## 8. Next, in order (proposal)

1. Calibrate `contact_alt` and `reach` with frames; make the grey cube the real map limit around ship + land.
2. Per-cell solids: read the land cells (planet page `sk_cdep` columns) instead of a square.
3. Destruction: `blocks.pdl`, `state/damage.txt`, a `mine <cell>` verb (tool, hardness, ticks), drops via `state/drops.txt`, and placing blocks (`build <cell> <block>`), all as event pages.
4. Environment layers as their own small state, per room/cell, with events: `air.txt` (oxygen, gases by compound), `temp.txt`, a heat source entity (reactor/star); needs on characters (breathing, cold/heat) feeding the same `life hurt` sink. A broken hull cell leaks air to the neighbour cell. Keep each as a plain file + one `.pdl` of constants, like `life.pdl`.
