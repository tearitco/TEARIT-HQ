# Handoff 2026-10-28: mcl-hq is now a book on the owner's desk (Claude to Grok)

Read this first, then `GROK-HANDOFF-2026-10-27-VISUALS-AND-ORGANIC-TRACK.md` (still valid), `GAME-MECHANICS-SPRINT.md`, `RULES-mcl-hq.md`, and the report `GROK-HANDOFF-MINECRAFT-MODES-AND-CIVILIZATIONS-REPORT.md`. Labels: **SAID** owner's words, **RAN** I ran it today, **MINE** my proposal.

## 1. What the owner said (SAID, 2026-10-28)
- The editor's end state is **one window that becomes any game** (Road B in `near-tear-gear.md`). What is edited first: **events / blueprintz, and the names of maps and entities, through the db or whatever fits**.
- **Petslike games first**; the other game-like apps are checked for compatibility afterwards.
- Games **must be books on the desk, and desk books must be openable in the editor** ("that's how Synch gets to work"). The obvious ones that fit the format are installed; scan and check.
- "Grok later, soon": MCL is Grok's specialty, so mcl-hq is the first book handed over.

## 2. What is ready (RAN today)
- **10 games export a page and pass `book_check` with 0 FAIL**; 9 were installed on the desk as books (cdda-hq civ-rpgce df-hq dsr-rpg gta-hq mcl-hq rpg-dust rpg-edit-hq tcg-rpgce; rpg-space was there; rpg-pet is not exportable: its first room has no size). 17 books on the desk now; the 163 original files are byte-identical (sha256) and the open book is still `s1`.
- **mcl-hq book**: `xyzfs/users/0a9558a7-.../home/livedesk/sessions/mcl-hq/` (`session.pdl` + `desks/city.pdl`: hero_01, xelector_01, camera_01 and 5 pets as rows, path column = `@.apps/mcl-hq/state`). The hero cell 8,8 is the core's first-run default (the game was never played), not a played position.
- **`book_check` on mcl-hq with `--events`: 16 pass, 0 fail**; blueprintz scans its **37 event pages, flags=0**, and compiles the book (121 pages).
- Backup of the whole desk sessions tree before the install: `/home/no/scratch-gab/desk-backup/` (tarball + sha256 list; NOT in /tmp, which is wiped on reboot).
- Only ONE page per book exists (the controlled character's room); the other rooms are not desk pages yet, so a Synch to them says `page-not-in-book`.
- The taskbar Synch was **not pressed live** and no game window was launched today (the 2026-10-28 15:21 freeze is unexplained).

## 3. Your slices (MINE, smallest first; each ends with something visible)
1. **`walk` in mcl-hq.** `RULES-mcl-hq.md` says `todo:walk`. Make it call `do_step`. *Proof:* a pal harness case in `&.widgits/_shared-lib/harness` (cases pdl + `.pal`, never a new `.sh`): one `walk` changes `state/trainer.txt` by exactly one cell, a wall blocks it, and the EVENT row is written. *Stop here and report* before slice 2.
2. **Gravity for sand and gravel** as rows in the rules page (not C). *Proof:* harness: remove the block under a sand cell, the sand falls one cell per tick, a ledger row per move. Mutation-test it (break the rule, the case must fail).
3. **Events and names through blueprintz (the owner's priority).** Open one mcl-hq event page in blueprintz, rename a map or an entity through the db (`&.widgits/db-hq`, actors / maps; **which db holds which name is a question for the owner**, find it and write it down first), save, then: `bp_scan` flags=0, `book_check --events` still 16/0, and the new name shows in the window. *Proof:* before/after PNG with `dump_frame_png_op.+x` + the state file. Only on a scratch copy first.

## 4. House rules for this work (full list: `AGENTS.md`)
- Branch **`grok`**, commit only your own paths with an explicit pathspec, never `git add -A`, never merge / push / stash / reset unprompted; read `git diff --cached --name-only` first. The owner reviews every slice.
- **Do not touch** `petslike_core.c` (11 games share it), `bv_menu_input.c`, the renderer, `khtpm_core_render.c`-family files, or `xyzfs/users` without the owner's OK. The editor-becomes-any-game work (Road B: `PET_APP` is a compile-time define used 24 times in the core) **stays with Claude**.
- `nice -n 15`, one heavy process at a time, a **footprint line** (idle and active, browser open) in each commit. No live board launch unless the machine is cool and idle.
- Free models only; never loop to a 429. Never report a number you did not observe; negative-test every check (`harness_case_op` silently ignores unknown verbs).
- Anything you change on the owner's real desk must be backed up under the home folder and verified by hash.

## 5. How to check your work
`book_check <pchq_root> --house <house> --page <page.pdl> --events` (`&.widgits/_shared-lib/ops/+x/book_check.+x`); harness: `cd &.widgits/_shared-lib/harness && /path/to/prisc <name>.pal`, then read `results/<name>.txt.verdict.txt`; page export: `game_page_export.+x @.apps/mcl-hq <scratch session dir> --house <house>`.

## 6. Open questions for the owner (do not guess)
Which db holds map names and entity names (slice 3); whether the other mcl-hq rooms should become desk pages (needs a page-per-room export); whether rpg-pet's first room gets a size so it can be a book.
