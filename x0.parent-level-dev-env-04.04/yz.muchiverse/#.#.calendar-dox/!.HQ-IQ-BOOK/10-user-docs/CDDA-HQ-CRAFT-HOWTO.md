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
