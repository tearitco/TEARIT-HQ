# df-hq: how to dig (user guide, 2026-10-16)

The same text is available inside the window: **Menu > "Help: how to dig"**. Each click shows the next step in the green line under the coins (six steps, then it starts over).

## What you need
- The df-hq window open (taskbar > df-hq), and a room with **rock**. Today that is the **Castle** (the grey block on the right side). Other rooms have none yet.
- **INT on** (the top-left tab says `INT on: <pet>`). Then the arrow keys walk the pet you control, one step per key press, and time only moves when you act. With INT off the pets walk on their own and you do not control anyone.
- At least one **other pet in the same room**. Pets only work in the room they stand in. They wander through the doors on their own, so one may need a while to walk in; tab `2.book` picks which pet you watch and steer (the others are the diggers).

## Mark one tile
1. Walk next to the rock and press an arrow key **into** the rock: you turn to face it and do not move.
2. **Menu > Dig: mark the tile in front.** The tile gets an **X** and the green line says `dig marked at x,y`.
3. Press arrow keys (each one is a turn). A free pet walks next to the mark; the X becomes a **diamond** when it has taken the job; then it digs. The tile turns into floor and you can walk in.

If you face open floor, the green line says `nothing to dig at x,y` and nothing is written.

## Mark a block (area)
1. Face one corner of the block and use **Menu > Dig area**. A marker tile shows the first corner.
2. Walk to the opposite corner, face it, and use **Dig area** again. Every rock cell of the rectangle between the two corners is marked (up to 64 marks at a time) and the corner marker disappears.

## Take a mark back
Face the marked tile and use **Menu > Dig: cancel the mark in front**. The X disappears. (A mark a pet has already finished cannot be cancelled: it is dug.)

## Save, load and reset
- **Player > Save (slot 1)** now also saves the dug cells and the marks (`events.txt` and `jobs.txt` are in the slot); **Player > Load (slot 1)** brings them back exactly as saved: cells dug after the save are rock again, marks made after the save are gone.
- **Menu > Reset house** moves the placed items aside (as before, kept as `house.txt.<time>.bak`) and now also **puts the rock back** and clears all marks. The old `DIG` rows stay in `events.txt` as history, followed by a `RESET` row.
- Before 2026-10-16 evening neither Save nor Reset touched dug cells (dug cells stayed dug forever).

## Good to know
- A cell enclosed by rock on all four sides is not worked until a neighbour is dug; the pets take the reachable marks first.
- Only one pet takes a mark. A pet that leaves the room gives its mark back.
- The files behind it (append-only, nothing is edited): `@.apps/df-hq/state/jobs.txt` (`DESIGNATE`, `CLAIM`, `DONE`, `CANCEL`, `CORNER` rows) and `state/events.txt` (`DIG` rows).
- Not built yet: how long a dig takes, tools, stone kinds, what drops, hauling, priorities, stairs and digging down. Those are the game rules and are listed in `@.apps/df-hq/README.md` (FORTRESS-TODO).
- Typed form, if you prefer a command: `df_hq.+x dig`, `dig <x> <y>`, `undig`, `dig_area`, `help`.

## The Help tab

The top bar has a **Help** tab (2026-10-17): Open help, Next page, Prev page, Close help. It opens a panel on the left of the picture with the full help, one page at a time. The text is `help.txt` in the app folder (pages separated by a line `---`). The chat scroll is on by default, ten lines, dark strip.
