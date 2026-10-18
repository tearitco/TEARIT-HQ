# mcl-hq blocks: mine, place, craft (2026-10-17)

For players. The same six steps are in the window: **Craft > Help: blocks, crafting** (one step per click, painted in the green line under the coins).

## What you can do

1. **Pick who acts.** Menu > Control (or Tab) switches between the pets and the player. The chosen one is the actor.
2. **Face a block.** Turn INT on and press an arrow into a block: you cannot walk into it, but you turn to face it. Blocks are drawn with the Mineclonia textures (stone, cobble, dirt, grass, planks, log). The starting ones are in the Overworld, east and west of the middle.
3. **Inventory > Mine block ahead** takes the block in front of the actor. It goes into the actor's bag (green line: `mined stone (bag 2)`). Nothing in front: `no block at x,y: face a block`.
4. **Inventory > Hold next block** picks which block in the bag Place will use (it cycles; the green line says `holding planks (3)`).
5. **Inventory > Place block ahead** puts the held block on the free cell in front. It costs one from the bag. It refuses on a block, on water, on scenery, on a door cell, and on a cell where someone stands (`cannot place at x,y: not free`).
6. **Craft > planks x4** turns 1 log in the bag into 4 planks. Without a log: `cannot craft planks: need the inputs in the bag`.
7. **Craft > Bag: what I carry** lists what the actor carries.

Each pet and the player have their own bag. A mined block can be walked into afterwards: the cell is free again.

## Where the rules live (for editing)

- Starting world: `BLOCK | room | x | y | w | h | id` rows at the end of `@.apps/mcl-hq/rooms.pdl` (ids: grass dirt stone cobble planks log).
- Recipes: `@.apps/mcl-hq/recipes.pdl`, `RECIPE | id | grid | output | count`. The grid is nine cells row-major: `.` is empty, a name is a cell, a `,` may separate two names that touch (`log........`). A row that is not exactly nine cells is counted as bad; **Bag**-style check: the verb `recipes` prints the recipe ids and `N bad rows`.
- Rule text for the verbs: BOOK `16.game/RULES-mcl-hq.md` (grok). Event line and ledger: BOOK `16.game/S1-EVENT-LEDGER.md`. Every mine / place / craft is an `EVENT dig|build|craft` row in `state/events.txt`; the world of blocks and every bag is a replay of that file, so Save, Load, New game and Reset behave (Save/Load already copy events.txt; New game gives the starting blocks and empty bags back).

## Verbs (for testing; the menu rows call these)

`mcl_hq.+x mine [x y]`, `place [id] [x y]`, `hold [id]`, `craft <id>`, `bag`, `recipes`, `help`. With two numbers the actor acts on that cell of the room it stands in.

## The log

Every mine, place and craft (and every refusal) also writes one line to the **chat scroll** as `<actor>: <what>` (for example `rex: mines stone`, `rex: cannot place planks there`). Open it with **Menu > Chat show / hide** (it is always on in the board window). It shows the last six lines, mixed with the pets' own talk. The wording is a first draft until grok answers `13.agent-coms/2026-10-17/from-sonnet-question-event-log.md`; the text lives in one place, `ge_describe()` in `_shared-lib/game_events.c`.

## Not built yet (on purpose, RULES-mcl-hq.md says later)

Gravity, chests, mobs, the 3x3 crafting table versus 2x2 (every recipe works anywhere today), tools and break times, drops other than the block itself, a second recipe.

## Tested

Scratch copies, paths rewritten, verbs and a window: see the commit message of the mcl-hq blocks commit and `XO/mcl-blocks-test.txt`. Not tested: on the live mcl-hq window, with the pets' own walking around blocks over a long time, a block placed under a pet.

## The Help tab

The top bar has a **Help** tab (2026-10-17): Open help, Next page, Prev page, Close help. It opens a panel on the left of the picture with the full help for this game, one page at a time (MCL-HQ-BLOCKS-HOWTO.md). The text is `help.txt` in the app folder (pages separated by a line `---`, 47 characters wide, 10 non-blank lines a page); edit that file to change the help. The chat scroll is now on by default and shows the last ten lines on a dark strip.

## Glyph and 3D views

The blocks are `B` cells (two high) in the glyph view and the 3D book; the board re-exports by itself when a block is mined, placed or crafted (a second or two). The standalone window is 2D only; open the board (Desk > mcl-hq) for the other views.

## The log lines (grok's spec, 2026-10-17)

Each resolved event is one line in the chat scroll, `D1 08:02 Rex crafts bandage`: the game clock (left out when the game has no clock yet), the actor, a present-tense sentence. Finds and crafts that worked are green, hits and refusals grey. Examples: `Rex finds a rag`, `Rex cannot craft bandage: need the inputs`, `Rex hits the zed: torso 2/3`, `the zed hits Rex: leg_l 4/5`, `Rex cannot place stone: the cell is taken`. The scroll shows ten lines (grok asked for twelve; ten is what fits the pinned bar, every text row is about 30 px tall). Not built: door and day-change lines (the pets already say "went through the door"), scroll back.

## Tabs (2026-10-17)

The old Blocks tab is now two tabs, after Help.

**Inventory** (the bag of whoever is controlled): one row per item in the bag, `stone x3`, the block Place will use marked `(held)`; clicking a row holds that block. Then Hold next block, Mine block ahead, Place block ahead, - cancel -. An empty bag shows one quiet row `(bag empty: mine)`. At most six item rows (there are six block kinds).

**Craft**: one row per `RECIPE` line of `recipes.pdl`, labelled with the output (`planks x4`); clicking it crafts that recipe. Then Recipes, Bag: what I carry, Help: blocks, crafting, - cancel -. At most six recipe rows.

Data: `state/ui.txt` keys `n_inv`, `inv_<i>_label`, `inv_<i>_key`, `n_rec`, `rec_<i>_label`, `rec_<i>_id`; they follow the ledger replay, so they change after every mine, place, hold and craft.

Later (not shown, no fake rows): chest, furnace, the 9-slot hotbar (`state/hotbar.txt` does not exist yet; the inventory replay is the whole bag), hunger, armor, a 3x3 versus 2x2 crafting grid.
