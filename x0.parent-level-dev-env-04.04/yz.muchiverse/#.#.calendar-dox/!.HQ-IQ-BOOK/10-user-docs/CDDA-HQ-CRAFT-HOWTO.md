# cdda-hq crafting, the cellar and one zed (2026-10-17)

For players. The same steps are in the window: **Craft > Help: crafting and the cellar** (one step per click, in the green line under the coins).

1. **Pick who acts.** Menu > Control (or Tab). Each pet and the player has its own bag.
2. **Craft > Scavenge a rag (stand-in)** puts one rag in the actor's bag. It is a stand-in so the loop can be played: there is no real source of items yet (grok's rules say so; a real one replaces this row).
3. **Craft > Craft bandage (1 rag)** takes 1 rag and gives 1 bandage, and charges **1 minute of game time** to the clock (measured: exactly 60,000 ms with the clock paused; a 0 minute recipe does not move it). Without a rag: `cannot craft bandage: need the inputs in the bag`.
4. **Craft > Bag** lists what the actor carries. **Craft > Recipes** lists the recipes (and `N bad rows` if recipes.pdl has rows that are not valid).
5. **The cellar.** The Castle has a stair: a door in its left wall (the second door, left of the main one) leads to the **Cellar** page and the Cellar's door leads back. It is an ordinary DOOR row, there is no z level yet (the sprint rules say later).

6. **The zed.** One zed lives in the Cellar (a purple winged picture from the Monster sheet, standing below the stair). It acts **only in INT (turn) mode**, after each of your turns: if you are next to it, it hits you; otherwise it takes one step toward you. It never leaves the Cellar and there is no second monster.
7. **Craft > Strike the zed beside me** hits an adjacent zed (the zed answers at the end of that turn). **Craft > Wounds** lists who is hurt, for example `zed leg_l 2/3, rex head 4/5`.
8. **Wounds.** Six parts: head, torso, arm_l, arm_r, leg_l, leg_r. Each starts at the victim's max HP divided by 6 (Rex 34 gives 5, the zed 18 gives 3). One hit takes 1 from one part; which part is a fixed function of the event (so a replay gives the same wounds). A **leg at 0** makes walking say `(slow: a leg is wounded)`; you still move one cell. Nothing else changes yet: nobody dies, arms and head have no effect.

## Where the rules live (for editing)

- `@.apps/cdda-hq/recipes.pdl`: `RECIPE | id | output | count | tool | skill | minutes | inputs`, inputs `item:count` separated by commas, no spaces. Only tool `none` works (a recipe naming a tool answers `it needs a tool (not built yet)`); skill is read and not checked yet. A row that is not valid is counted as bad, never skipped silently.
- `@.apps/cdda-hq/rooms.pdl`: the `cellar` ROOM and its two DOOR rows. `state/zed.txt` (room x y dir) is the zed; absent = the default spot (Cellar 6,7). `state/wounds.pdl` is the readable copy of the wound table (`WOUND | who | part | left | max`), rewritten after every scan.
- Rule text: BOOK `16.game/RULES-cdda-hq.md` (grok). Event line: `16.game/S1-EVENT-LEDGER.md`. Every scavenge and craft is an `EVENT give_item|craft` row in `state/events.txt`; the bags are a replay of that file (Save, Load, New game already copy or clear events.txt).

## Verbs (for testing; the menu rows call these)

`cdda_hq.+x craft <id>`, `scavenge`, `bag`, `recipes`, `help`, `strike`, `wounds`.

## The log

Scavenge, craft, strike (both sides) and every refusal also write one line to the **chat scroll** as `<actor>: <what>` (`rex: crafts bandage`, `zed: strikes rex: head`). Open it with **Menu > Chat show / hide** (always on in the board window). It shows the last six lines, mixed with the pets' own talk. Wording is a first draft until grok answers `13.agent-coms/2026-10-17/from-sonnet-question-event-log.md`; it lives in `ge_describe()` in `_shared-lib/game_events.c`.

## Not built yet

Killing anything (nobody dies), effects of arm/head/torso wounds, the zed acting in real time (INT off), more than one monster or a spawn table, which pet is hostile (`party.pdl` column), tools, skill checks, a real item source, a z level for the cellar, a healing use for the bandage.

## Tested

Scratch copies and a scratch window, see the commit messages and `XO/cdda-c1-test.txt`, `XO/cdda-c2-test.txt`. Not tested: on the live cdda-hq window.

## The Help tab

The top bar has a **Help** tab (2026-10-17): Open help, Next page, Prev page, Close help. It opens a panel on the left of the picture with the full help for this game, one page at a time (CDDA-HQ-CRAFT-HOWTO.md). The text is `help.txt` in the app folder (pages separated by a line `---`, 47 characters wide, 10 non-blank lines a page); edit that file to change the help. The chat scroll is now on by default and shows the last ten lines on a dark strip.

## Glyph and 3D views

The zed is an entity (letter `z`) in the glyph view and the 3D book of its room; it follows the zed from room to room. The standalone window is 2D only; open the board (Desk > cdda-hq) for the other views.

## The log lines (grok's spec, 2026-10-17)

Each resolved event is one line in the chat scroll, `D1 08:02 Rex crafts bandage`: the game clock (left out when the game has no clock yet), the actor, a present-tense sentence. Finds and crafts that worked are green, hits and refusals grey. Examples: `Rex finds a rag`, `Rex cannot craft bandage: need the inputs`, `Rex hits the zed: torso 2/3`, `the zed hits Rex: leg_l 4/5`, `Rex cannot place stone: the cell is taken`. The scroll shows ten lines (grok asked for twelve; ten is what fits the pinned bar, every text row is about 30 px tall). Door crossings of the controlled pet are `D1 08:20 Rex moves to the Cellar`, and each new game day is `D2 00:00 the day turns` (the last day is kept in `state/clock_lastday.txt`). Not built: scroll back.

## Tabs (2026-10-17)

The old Craft tab is replaced by four, after Help: Inventory, Craft, Body, Look. Rows are built from data in `state/ui.txt` (keys below), each dropdown has ten rows or fewer and ends with `- cancel -`. They follow whoever you control (Menu > Control, Tab).

- **Inventory**: `Scavenge a rag`, then one row per item in the controlled actor's bag (`rag x2`, up to 7; `(bag empty)` when none; a click lists the bag in the note), then `Wear (later)`. Keys `n_inv`, `inv_<i>_label`.
- **Craft**: one row per RECIPE line in `recipes.pdl`, `Craft bandage 1m` (the recipe id and its minutes; up to 6), then `Recipes`, `Help: craft and cellar`, `Construct (later)`. Keys `n_craft`, `craft_<i>_label`, `craft_<i>_id`.
- **Body**: one row per body part of the controlled actor, `torso 2/3` (left over max, max is max HP / 6; head, torso, arm_l, arm_r, leg_l, leg_r; a click runs Wounds), then `Strike the zed beside me`, `Skills (later)`. Keys `n_body`, `body_<i>_label`.
- **Look**: quiet rows, no action: the zed (`zed here at 6,7` or `no zed in this room`), the cell ahead (`ahead: the zed`, a door, a placed item or nothing), up to three placed items, then the room's doors. Keys `n_look`, `look_<i>_label`.

Later (shown greyed, they do nothing): Wear, Construct, Skills; hunger, thirst and volume are not shown at all. Messages is the chat bar, not a tab. The verbs (`scavenge`, `craft <id>`, `bag`, `recipes`, `help`, `strike`, `wounds`) are unchanged. Test: XO/test_tabs_cdda_hq.sh.
