# Testing board behaviour you cannot pin down live (method, 2026-11-05)

Written after trying to prove "Enter on a pet possesses it" on the live editor and failing for reasons that were not the feature. Labels: **OBS** = I ran it, **LESSON** = what to do next time.

## The rule
When a live board is too fluid to hold still, do NOT keep driving it. Make the behaviour callable as a verb and test the real binary in a scratch house with fixed data. Use the live board only for one final "it looks right" frame.

## Why the live board failed (OBS, editor, 2026-11-05)
- With INT off the pets wander (Ember moved from (20,7) to (19,9) between two reads) and the daemon re-pins the xelector to the selected entity every tick, so a hand-set xelector cell does not hold.
- The same state files are written by the daemon, the engine, and you: a backup taken at one moment is a different "original" from the one taken a minute later. Take ONE backup before the first edit and restore from that.
- The engine session carries its own copy of `bv_menu_input.+x`, made at session start. A rebuilt binary reaches a running board only by relaunch (or by `cp` to a temp name in `<session>/ops/+x/` then `mv`).
- The board must show the editor's own book (`world_01/state.txt` map_id) or the daemon refuses on purpose; switching books through the menu changes the owner's view, so it must be switched back.

## The method that worked (OBS: `possess_enter.pal`, 11 PASS)
1. **Make it a verb.** `fire possess` runs `possess_at_xelector()` once (`petslike_core.c`). A verb process loads only the active pet and the trainer, so the verb calls `pet_load()` first; the resident daemon has everyone loaded, which is why the function never did.
2. **Scratch house, real binary.** `harness/cases/possess_enter.pdl`: `COPY` the real `rpg_edit_hq.+x` and the app's `*.pdl` into `$T/house/@.apps/rpg-edit-hq/`, write fixed `state/pet_ember.txt`, `trainer.txt`, `active.txt`, `flag_ctl.txt`, and the pc-hq `world_01` + `xelector_01` state files. `COPY` reads only from `$HOUSE`, so no asset sheets: the "missing RPG Maker sheets" line is only a warning for this verb.
3. **Check the file, not the message alone.** `EXPECT_HAS state/active.txt ember`, `flag_ctl.txt player`.
4. **Add the negatives in the same case:** an empty cell chooses nobody and changes nothing; a board showing another book is refused even with a pet on the cell.
5. **Mutation run (house rule):** `sed` a copy of the case so two expectations are wrong, run it, it must FAIL on exactly those, then `rm` the exact copy paths.

## Traps (each cost time today)
- **One `key=value` per line.** Pet and trainer files are line-based. I copied the single-line form I had printed with `tr '\n' ' '`; the parser matched nothing and every pet silently went to its spawn cell. If every loaded value equals a default, suspect the file format before the logic.
- **Read the neighbouring code before adding a mechanism.** I built an engine-to-daemon request file for pet possession, copying the brush placer, and only then found `possess_at_xelector()` already implements the owner's 2026-10-12 rule. Two selectors would have fought. I reverted mine (nothing of it was committed). `grep -n "enter-possess\|possess" petslike_core.c` first.
- **A trace line beats a theory.** I guessed at assets, strtok, and spawn rules; one `fprintf` of the raw file text and the parsed cell found the real cause in one run. Remove the trace and `grep -c` for it before committing.
- **Restore live state by exact path and hash:** the xelector state file (`sha256sum`), `state/active.txt`, `flag_ctl.txt`, `flag_follow.txt`, `flag_int.txt`, the engine's `bv_state.txt` selector, `last_book.txt`, and the book shown. Leaked engines from relaunching are stopped by exact pid.

## What is NOT proven
The engine delivering Enter to the daemon (keyboard history) and the board redrawing the choice. The live Enter-on-pet frame was never obtained: the owner's flow is INT off, xelector free, Enter on a pet, and the pets move. A reliable live proof would need a fixture that freezes the pets (e.g. the clock paused) while the key is sent.
