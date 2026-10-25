# RPG-SPACE: collision and destruction (design, 2026-10-25)

**Status: DESIGN ONLY. Nothing in sections 3-6 is built.** Written for future agents after the owner asked (2026-10-25) "do we have collision codes yet?", "research how RPG Maker MV does collision vs Unreal", and "what about destructability (which we should have) ala minecraft?". Read this before touching ship/land collision, damage or breakable blocks. Related: `RPG-SPACE-LIFE-DEATH-RESPAWN.md` (the damage sink), `rpg-space-vehicle-entities/README.md` (ship and land entities), `RPG-SPACE-DEATH-RESPAWN-GRAVITY-NOTES.md`.

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

## 5. Rules for whoever builds it

- Verify with a 3D frame ("if you didn't see it in 3D it doesn't exist"): ship blocked at the planet, a cell removed, a drop appearing.
- Negative-test: collision off must let the ship through; a destroyed cell must really be gone after reload.
- Put numbers in .pdl, behaviour in event pages; do not hard-code hardness or damage in C.
- Do not extend the old slide approach (rewriting the book per step); entities and overlays only ([[ship-and-land-are-entities]] direction).

## 6. Open questions for the owner

1. Do crew die/take damage on every hit, or only above a speed threshold?
2. Is everything destructible (planet, hull) or only marked blocks?
3. Is mining/building a goal (tools, hardness) or only impact/explosion damage first?
4. Should a destroyed hull cell open the ship to vacuum (ties to the later ozone/vacuum events)?
