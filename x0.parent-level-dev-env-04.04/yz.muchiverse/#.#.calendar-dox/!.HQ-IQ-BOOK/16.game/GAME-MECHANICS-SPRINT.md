# Game mechanics sprint (df-hq, cdda-hq, mcl-hq)

2026-10-16. The loops worth building first. Decisions already made stay in `GAMES-DECISIONS-2026-10-13.md`. The cell file stays `Z-CELL-FORMAT.md`. This page is the verbs and the order.

Rules are rows. A screen appends an event. The daemon resolves the tail. The same verb is the menu click, the job, and the actor's own next step.

```
EVENT | time | actor | verb | target | at | result
```

`at` is world, z, x, y. A result is another row (item, wound, cell, job), not a special return. Drawing looks up an id. It does not learn the recipe.

Shared verbs: `embark`, `gen_map`, `spawn_actor`, `give_item`, `walk`, `wait`, `look`, `take`, `drop`, `wield`, `wear`, `eat`, `drink`, `craft`, `build`, `dig`, `install_part`, `strike`, `haul`. A game adds verbs. It does not add a second ledger.

Start, middle, and long are how much ledger exists. The board does not change mode.

## df-hq

Fortress mode. One site. Dwarves are actors. The player designates. Dwarves take jobs.

**Setup (embark).** World size, savagery, minerals. One civilization. Seven dwarves: labor prefs and a few skills, paid from a point pool. A wagon of food, drink, seeds, picks, an anvil. Pick a site on the embark map. That screen writes `embark`, `spawn_actor` ×7, `give_item`, `gen_map`. Then it is done.

**The turn.** A designation (`dig`, channel, stairs, chop, gather) is a mark on a cell. A free dwarf with that labor takes it: path to a stand tile, do the work, `haul` the product to a stockpile. Workshops are the same: an order is a `craft` job waiting for inputs in stock. The player menu is build, designate, stockpile, zone, squads. Each entry appends. Nobody has to be clicked to dig.

**Needs.** Each dwarf has hunger, thirst, sleep, and a mood. Eating, drinking, and sleeping are their events when the need crosses a line. A bad thought is a row. Enough of them and the dwarf stops taking jobs. That is the middle of a fort. It does not need a separate mood engine.

**Body.** A strike names a body part. The wound row is what the unit screen shows. Bleeding and infection are later rows on the same wound. Death is no more jobs for that actor.

**Map.** `gen_map` writes z-columns: soil, stone, ore, caverns below, sky above. The player `dig`s rooms and `build`s floors, walls, doors, beds, and workshops. A stockpile is a zone, not a building. Farms are a zone plus seeds. Start is a hole, beds, and a farm. Middle is workshops, a still, a barracks, and the first siege (`spawn_actor` with a hostile weight). Long is the accumulated dig: bedrooms, industry, and a cavern breach. Same verbs.

**What waits.** Military schedules, nobles, strange moods, liquid flow, minecarts, and world-gen history. The sprint is designate, job, haul, craft, eat, one hostile wave.

## cdda-hq

One survivor. Turns are the player's, then every other actor's one verb.

**Setup.** World: city size, distance between cities. Scenario, profession, traits, spent from a point pool. Spawn in a shelter or a house. Writes `embark`, `gen_map` for the overmap chunk, `spawn_actor`, `give_item`.

**The turn.** Move, look, smash, open, `take`, `drop`, `wield`, `wear`, `eat`, `drink`, `craft`, `wait`, sleep. Each costs time. Needs tick on that clock: hunger, thirst, fatigue, pain. Stats are strength, dexterity, intelligence, perception. They gate hit, carry, and which recipes are visible. Skills rise when the matching verb resolves.

**Body.** Hit locations, each with hp. `strike` picks a part. A broken part changes move and craft speed. Healing is an event over time, faster with items.

**Craft.** A recipe is inputs, a tool, a skill, and minutes. `craft` checks the tiles next to the actor (or the vehicle cargo), appends the output, removes inputs. A long craft is the same event across several turns until time is paid or the actor moves. That is also how a DF workshop job finishes. One resolver.

**Map and vehicles.** Overmap is coarse (forest, field, house, road). Stepping onto a house `gen_map`s that chunk: walls, doors, furniture, loot. A basement is z-1. A vehicle is its own grid of parts plus a `ZPLACE` on the map. `install_part` needs the part item, a tool, and a facing. Driving is `walk` on the vehicle's seat with fuel ticking. Start is loot the house and a weapon. Middle is a working vehicle and a stash. Long is more chunks generated as the player drives, plus NPC actors using the same verb picker.

**Enemies.** Spawned by the chunk or by sound. Each turn: see, step, or `strike`. No separate AI loop.

**What waits.** Factions, mutations, bionics, weather fields, and full city specials. The sprint is one house, a basement stair, a recipe list, a wound, and one monster.

## mcl-hq

A block world. The player breaks and places. Mineclonia textures stay the art. No Luanti server in this sprint.

**Setup.** Seed, then `gen_map` for the chunks around the spawn: biome, height, trees, ore in stone, water at sea level. One player actor. Writes `embark`, `gen_map`, `spawn_actor`. There is no class and no point pool.

**The turn.** Break a block (`dig` — time by tool), place a block (`build`), `take` drops, `drop`, select the hotbar. Gravity after the cell change: sand and gravel fall, water spreads one step. A door is `build` plus a later open, same as the other games. The chest is a container at `at`. Its contents are item rows, not a new screen type.

**Craft.** 2×2 in the inventory, 3×3 at a table. The recipe matches a shape, not only a pile. Output is one or more items. Furnace is `craft` with fuel as the tool and time on the clock. That is the only extra column versus CDDA.

**Body.** Health and hunger. Hunger ticks on time. At zero, health ticks down. `eat` fills hunger. `strike` from a mob or a fall subtracts health. Zero health is `spawn_actor` back at the bed or the world spawn, and the dropped pile stays.

**Mobs.** Spawn on dark cells, away from the player. Each turn: idle, step toward, or `strike`. Light on a cell suppresses the spawn. That is the whole enemy rule for the sprint.

**Days.** A clock phase the board already has. Hostile spawn follows the dark half.

**Start, middle, long.** Start: punch a log, craft planks, a table, a wooden pick, a door, a chest. Middle: stone and iron, a furnace, food, a shaft. Long: more chunks as the player walks, and a second z world (nether-style) reached by a portal block that is just a `ZLINK`. The end fight is not in the sprint.

**What waits.** Redstone, enchanting, villages as full generators, and a server. Trees and one chest in a generated chunk are enough scenery.

## Order to build

1. The event line and the resolver for `give_item`, `take`, `walk`, `dig`, `build`, `craft`.
2. df-hq: designations become jobs a dwarf takes.
3. cdda-hq: the same craft, plus a wound and one monster turn.
4. mcl-hq: the same dig and build, plus gravity and the 3×3 shape match.

Tiles, INT, and the board stay as they are. This page does not change them.
