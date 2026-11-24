# cdda-hq, df-hq, mcl-hq: honest status (2026-11-23)

Labels: READ = read in files, RAN = run today, SEEN = opened a PNG, NOT PROVEN = no evidence.

## Short answer to the owner
* Started? Yes, all three, 2026-10-12/13. Last touched 2026-11-04 (cdda 48, df 55, mcl 48 commits on their folders; the newest commit, 2026-11-04 area, is the lamps rollout, not game work).
* How far? Each is a skin of `petslike_core.c` (the dsr-rpg engine copy). The map is still the six dsr-rpg rooms (City, Bank, Castle, ...) with RPG Maker tiles; cast is the dsr-rpg party (Rex, Gus, Pip, Orin, Maren, Fenn, Harold). Nothing is CDDA, Dwarf Fortress or Minecraft in look or world; only a small set of verbs is. SEEN: cdda-hq launched today shows "cdda-hq - City", RPG Maker houses and trees.
* Presentations made? Old ones only (XO games-screens, XO df-presentation from 2026-10-16). New human-path videos were NOT made today (see "Not done"). The Minecraft-like thing with real 3D blocks, hunger and possession is the separate `@.apps/vox` (videos XO nov-22 16, 17, 19).

## Harness (RAN today, niced, through cpu_gate, one at a time, verdict files dated 2026-11-23 21:59)
| case | result today | what it proves | what it does NOT prove |
|---|---|---|---|
| play_cdda_hq | VERDICT PASS 50/0 | scripted verbs on a scratch copy: scavenge 2 rags, craft bandage (clock +60000 ms), stair door to cellar, strike the zed (wound rows, zed answers), save / new game / load, ledger replay | any screen, any key press, In:, 3D; the zed's turn when you do not use INT |
| play_df_hq | PASS 36/0 | dig_area marks 3x3 (9 DESIGNATE rows), undig/dig, the controlled dwarf walks until 9 DIG/DONE rows and `give_item stone` x9, save/new/load, replay | haul, stockpiles, labor, a screen, keys |
| play_mcl_hq | PASS 43/0 | mine stone and logs (empty cell refused), craft planks twice (third refused without a log), hold/place with 3 refusals, placed block stops the walker, save/new/load, replay reopens mined blocks | gravity, chests with content, mobs, 3D, keys |
The harness stand-in window is `tail`; none of these assert what is ON SCREEN (the owner's standard says that is a failed test kind for a human-facing claim).

## cdda-hq
* READ exists: verbs buy control craft delpal goto load newgame newpal newroom place remove save scavenge strike use (+ native int/play/stop); `recipes.pdl` has ONE recipe (bandage = 1 rag, 1 minute); a cellar page behind a door row (no z); one zed in the cellar that acts only in INT turn mode; wound rows per body part (state/wounds.pdl). 36 event dirs in `events/`.
* Doc lag: BOOK RULES-cdda-hq.md says "no recipe file", "dig later"; the code has recipes.pdl and strike. The doc is older than the code (fix the doc, not the code).
* Rules are C: `cd_rule()` via `GE_RULE_HOOK` in `ops/cdda_hq.c`. Event pages exist only as verb plumbing; the craft/strike outcomes are not event-page rules. NOT D92-compliant.
* SEEN live today (frames in XO 7.cdda-hq-human-path-2026-11-23): launch via cpu_gate + `button.sh run`; tab 1 label `INT off`; click at 60,48 turned it `INT on: Rex` (no `In:` label: petslike boards use the INT tab, and there is no HUD `pos` or `poss` on this 2D window; the pchq `In: ON` bug D117 is a different template). Rex was auto-walking (pets live on their own) and was in the Cellar; relay `down down right right` was followed by Rex moving next to the zed (frame 3). The arrow keys reached the game through the per-pid relay file (200-203), not xdotool. First frame taken immediately showed no move: the window lags by seconds; wait before reading.
* Keys that exist (keybinds.pdl READ): arrows, Tab (control switch), 0 (2D/3D), 1-4 (pov), Esc. Everything else (scavenge, craft, strike) is a MENU click, not a key. So "keys only" cannot show crafting or fighting today.
* Distance to playable by the owner's standard: far. Missing: a CDDA-like map (z, terrain), more than one recipe/monster, HUD pos, In:/possession path on the 3D board, event-page rules.
* NOT PROVEN: 3D view of cdda-hq, possession by Enter (no HUD poss on this skin), strike by keys.

## df-hq
* READ exists: dig (mark a rock cell), undig, dig_area (corner + corner, up to 64), jobs file `state/jobs.txt` (DESIGNATE, CLAIM, DONE, CANCEL, CORNER), a free dwarf takes the nearest mark and digs (one stone via give_item), diamond/X marks on screen, save slots include jobs and events, a `test-fortress` save. Haul, stockpiles, labor, priorities, build, craft: NOT built (RULES doc: later).
* Rules are C: `df_rule()` hook in `ops/df_hq.c` and the dig job logic in C. NOT D92-compliant.
* NOT RUN live today. Marks and dig are menu-driven (Designations > Mine); keys are the same arrow/Tab/0/1-4/Esc set, so a keys-only video would show only walking next to rock.
* This one is nearest to "units doing jobs" (a DF idea, labelled DOC as an owner vision, not copied code).

## mcl-hq
* READ exists: mine (block ahead into the bag), hold (cycle), place (cost one, three refusals), craft (recipes.pdl: planks from log, 2x2/3x3 grid string), chest verb, blocks grass dirt stone cobble planks log, placed blocks stop walkers, mined blocks persist in saves.
* Rules are C (`mcl_rule()`); gravity, mobs, chest contents later. NOT D92-compliant.
* NOT RUN live today. Mine/place are menu rows (Inventory > Mine block ahead), not keys. `@.apps/vox` already does real mine/place by keys (`l`, `p`) with a DAMAGE/PLACE ledger and possession; that is the playable Minecraft path, so mcl-hq should either be retired into vox or reduced to a skin test.

## What it would take to be ops + event pages (D92)
For each rule (craft, strike, dig job, mine, place, hunger, monster turn): a pure module + CLI op + registry row + harness case + event page in `@.apps/<game>/events/`, numbers in RULE rows; then delete the `*_rule()` branch. Estimate by count of rules: cdda about 4 (scavenge, craft, strike, zed turn), df about 3 (dig, claim, walk-to-job), mcl about 5 (mine, hold, place, craft, chest). NOT PROVEN as an estimate (no spike done).

## Carries into TEARITIZATION (DOC: BOOK TEARITIZATION-FORWARD-GUIDANCE-2026-11-23.md)
* df-hq job list + claim + CLAIM/DONE rows = units doing jobs; the append-only job ledger with byte cursor is the pattern.
* mcl-hq/vox block ledger = terrain editing.
* cdda wound/turn rows = turn order, damage as rows.
* Fog: none of the three has it.
* Do NOT copy: any CDDA, Dwarf Fortress or Minecraft code, text, item lists, textures or sprites. These skins only borrow the idea of a verb; the tiles are RPG Maker sheets already in the house.

## Findings
1. relay arrows (200-203) reach INT on a petslike board, but visibly lag (seconds); a first frame can show nothing. SEEN.
2. No `In:`/`poss` HUD on petslike boards; the owner proof recipe (In: ON, HUD pos) does not map onto them one to one. Needs an owner decision: prove them with INT label + room label + events.txt rows, or give them the pchq board.
3. Docs lag code: RULES-cdda-hq.md (recipe, strike exist).
4. `lc_clock.+x` (pid 3009180) started by my launch is still running after I closed the window; it should exit when its owner is gone (rule), I did not kill it.
5. scrot and x11grab give black frames of the window (known); `dump_frame_png_op.+x` works.

## Not done
No video was recorded for any of the three (no voice, subtitles, 3D, df-hq or mcl-hq live run). Only one live window run (cdda-hq) with three stills. Live state: `pchq_board_view.txt` hash unchanged (dfaff4416...), cdda-hq state restored from a tar taken before the launch.
