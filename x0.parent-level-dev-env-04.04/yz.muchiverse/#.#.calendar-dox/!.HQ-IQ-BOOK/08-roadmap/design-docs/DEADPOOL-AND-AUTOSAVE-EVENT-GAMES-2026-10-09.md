# Deadpool and autosave for event-play games (Doom first, all event games after)

2026-10-09. Owner decision, written by claude. **Design only, nothing built.** Doom is the first user; the rules are generic so every book that plays through
events (TSOTS, dwarf fortress, cdda, mineclonia, civ ...) uses the same files and verbs.

## 1. Plain words (the owner's rule, in one place)

1. **Nothing is ever deleted when it "dies".** A monster, pickup, door or any event pal that is killed, taken or used is **sent to the deadpool**: a list of dead
   pals and their sprites. The deadpool row remembers where it came from.
2. **Restart repopulates.** When the game is restarted (New Game / Restart / Reset), every deadpool row goes back to its **original position** on its page, alive,
   with its original sprite and counters. The deadpool is empty again.
3. **Save state is separate and always wins for "Continue".** The game **autosaves the last state** all the time (every state change). Continue / reopening the book
   loads that last state **including who is in the deadpool**, so a monster you killed stays dead after you quit and come back. Only an explicit restart repopulates.
4. These two are not the same thing: *restart* = go back to the book's original layout; *autosave* = come back exactly where you stopped. Manual Save/Load slots
   (`save_slot N`) are extra snapshots on top; they use the same files.

An earlier summary said "entities do not die". Correct reading: an entity never stops existing in the book's data; it just stops being **active on the page**.

## 2. Why this is cheap in this house

The original layout is already immutable data: `<desk>/events.pdl` (`EVENT | x=.. y=.. glyph=.. | trigger=.. cmds=..`) plus the package `<desk>/ev/<n>/`. Those
files are never edited while playing. So "original position" needs no copy: a deadpool row is just a pointer `(book, desk, event id)`; restoring = deleting the row.

## 3. Files (all `.pdl`, all under the book folder, runtime state is not committed)

`maps/<book>/deadpool.pdl`
```
SECTION | KEY            | VALUE
DEAD    | desk           | e1m1_hangar
DEAD    | ev             | 17
DEAD    | x_y            | 12,40
DEAD    | glyph          | E
DEAD    | sprite         | frames/former_human_0_down_0.rgba
DEAD    | pal            | ev/17
DEAD    | died_at        | 2026-10-09 14:20:11
```
One block per dead pal (repeat the `DEAD` rows; start a new block with `desk`). `x_y`, `glyph` and `sprite` are copied from `events.pdl` at death time so the
list is readable on its own and survives a converter re-run.

`maps/<book>/autosave.pdl` (written by the same verbs after every change, atomically: write `.tmp`, `mv -f`)
```
AUTO | map_id   | doom
AUTO | desk_id  | e1m1_hangar
AUTO | mode     | on
AUTO | hero_xy  | 4,34
AUTO | saved_at | 2026-10-09 14:20:11
```
plus a copy of `state.pdl` (`hp ammo keys battle ...`) and `deadpool.pdl` beside it. Manual slots `slot_NN/` hold the same three files.

## 4. Verbs (extend `doom_event.sh`; register each in `#.ref/menu/event_commands.registry.pdl`)

| Verb | Does | Notes |
|---|---|---|
| `kill_event <desk> <ev>` | add the DEAD block, hide the cell (rewrite the cell glyph to floor in the **runtime** view, not `map.txt`), disarm the package | call it from a monster's death page and from pickups after use |
| `revive_all` | empty `deadpool.pdl`, restore all glyphs/packages | used by restart only |
| `restart_game` | `revive_all`, reset `state.pdl` to defaults, set desk to the first level, write autosave | New Game calls this |
| `autosave` | write `autosave.pdl` + state + deadpool | called by every verb that changes state |
| `continue_game` | read `autosave.pdl`, restore desk, state, deadpool, play flag | Continue / book reopen |
| `save_slot N` / `load_slot N` | copy/restore the three files to/from `slot_NN/` | already exists for book+state; add deadpool |

Idempotence rules: `kill_event` on an already-dead id is a no-op; `revive_all` on an empty deadpool is a no-op; every verb exits 0 on bad input (empty id must
never do harm).

## 5. How "hidden" works without editing `map.txt`

`map.txt` stays the original. The projector/board build the visible layer from `map.txt` **minus** the deadpool. Two ways, pick the cheaper after reading
`emit_map_events` and `pchq_board_projector.c`:
- (a) the footer/touch code skips any event whose `(desk, ev)` is in `deadpool.pdl` (cheapest; the glyph on the 2D/3D view still shows, so also drop the sprite);
- (b) when a desk loads, `pc_generate_chunk` overlays `f` on dead cells. Needs a loader change in claude's lane.
Start with (a) and the sprite hide; ask claude for (b) only if the glyph must vanish in 2D.

## 6. Evidence the owner expects

State-file diff per verb (`deadpool.pdl` before/after a `kill_event`; empty after `restart_game`; non-empty and identical after `continue_game`), then one window
PNG of the page before the kill, after the kill, and after restart. The scratch-house harness in `XO/17.DOOM/DOOM-BOOK-BIBLE.md` runs these without touching the
live game.

## 7. Open questions

1. Does a manual `Load N` also restore the deadpool? (Proposed yes: a slot is a whole-game snapshot.)
2. Should respawning monsters ever leave the deadpool during play (timed)? (Not now.)
3. Hero death: `hp=0` -> `stop_game`; autosave keeps the last good state, Continue loads it.
