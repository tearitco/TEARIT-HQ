# cdda-hq crafting and the cellar (2026-10-17)

For players. The same steps are in the window: **Craft > Help: crafting and the cellar** (one step per click, in the green line under the coins).

1. **Pick who acts.** Menu > Control (or Tab). Each pet and the player has its own bag.
2. **Craft > Scavenge a rag (stand-in)** puts one rag in the actor's bag. It is a stand-in so the loop can be played: there is no real source of items yet (grok's rules say so; a real one replaces this row).
3. **Craft > Craft bandage (1 rag)** takes 1 rag and gives 1 bandage, and charges **1 minute of game time** to the clock (measured: exactly 60,000 ms with the clock paused; a 0 minute recipe does not move it). Without a rag: `cannot craft bandage: need the inputs in the bag`.
4. **Craft > Bag** lists what the actor carries. **Craft > Recipes** lists the recipes (and `N bad rows` if recipes.pdl has rows that are not valid).
5. **The cellar.** The Castle has a stair: a door in its left wall (the second door, left of the main one) leads to the **Cellar** page and the Cellar's door leads back. It is an ordinary DOOR row, there is no z level yet (the sprint rules say later).

## Where the rules live (for editing)

- `@.apps/cdda-hq/recipes.pdl`: `RECIPE | id | output | count | tool | skill | minutes | inputs`, inputs `item:count` separated by commas, no spaces. Only tool `none` works (a recipe naming a tool answers `it needs a tool (not built yet)`); skill is read and not checked yet. A row that is not valid is counted as bad, never skipped silently.
- `@.apps/cdda-hq/rooms.pdl`: the `cellar` ROOM and its two DOOR rows.
- Rule text: BOOK `16.game/RULES-cdda-hq.md` (grok). Event line: `16.game/S1-EVENT-LEDGER.md`. Every scavenge and craft is an `EVENT give_item|craft` row in `state/events.txt`; the bags are a replay of that file (Save, Load, New game already copy or clear events.txt).

## Verbs (for testing; the menu rows call these)

`cdda_hq.+x craft <id>`, `scavenge`, `bag`, `recipes`, `help`.

## Not built yet

Wounds (parts, strike), the one monster turn (zed), tools, skill checks, a real item source, a z level for the cellar. They are the next slice (RULES-cdda-hq.md has the wound and monster rules).

## Tested

Scratch copies and a scratch window, see the commit message and `XO/cdda-c1-test.txt`. Not tested: on the live cdda-hq window.
