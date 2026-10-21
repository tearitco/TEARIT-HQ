# Handoff to Claude — 2026-10-21 05:12 PDT

The owner's weekly limit on this session is low. Read this and keep
going. Do not reopen settled questions.

## What is open

- The **edit** board is its own window. Title `edit`. Book `edit`,
  page `start`. Process is `khtpm_core_render` on
  `@.apps/rpg-edit-hq/board/pchq-board.xhtpm` with
  `PCHQ_PKG=@.apps/rpg-edit-hq/rpg-edit-hq-pchq`.
- Launch again with `sh @.apps/rpg-edit-hq/board/open_board.sh`.
  That replaces only this instance. It does not open or replace the
  piececraft-hq board.
- 12.toys is taskbar cell `strip-cell-12`. The row label is **edit**
  (`@.apps/rpg-edit-hq/toy.pdl`). The menu scans `toy.pdl` when the
  cell opens. No taskbar rebuild.
- The piececraft board (the one the owner called pc-hq) was left on
  book `rpg-pet`. Do not launch `piececraft-hq/open_pchq_board.sh`
  for this project.

## Map

`board/maps/edit/start/map.txt` is a 16x16 starter: `.` empty, `f`
floor height 1, `W` walls height 3. It is not book TON and not page
`centroid-u`. Those names are not files anywhere under this house.
The owner showed a floating-island picture and called it book TON
page centroid-u. The next map step is to replace `start` with that
picture when the owner points at the file. Do not invent the islands.

Footer `bar.txt`, in order: Ember 🔴, Glacine 🔵, Murmur 🟣,
Solvent 🟢, Ava 👩, Cursword 🗡️, Asa 👨, robot 🤖. Names and glyphs
are from `xyzfs/.../sessions/s1/desks/teru-test.pdl` and
`&.widgits/db-hq/data/actors.pdl`. Trainer the owner settled on is
**Ava**, then they also asked for **Asa** and a second robot. Both
are on the footer. `teru-test` has only one robot (`robot_chat_001`
🤖), so the robot seat uses that mark. The board footer holds 16
names, then a second strip. Eight fits.

Actor face and character fields are empty. There is no Terumon PNG
sheet. Do not borrow People4, Actor2, or Actor3.

## Casino Island, already built, leave it

Folder `44.xyz.01.00/@.apps/bet-rpg`. Window title Casino Island.
Toys row **Casino Island**. Not named `rpg-bet`. Not a page of the
TON book. Cast Vale Nia Sol Kit Bram Wynn on People4 0–5. One footer
of those pets, acts on the left, chat on the right. Night hotel bill
is not built. `BET-RPG-DESIGN.md` still says "nothing is built" at
the top. That line is stale. Do not rename the folder.

## Do not touch

- Camera equations in `bv_menu_input.c`, `bv_render_3d.c`, or
  `piececraft-hq/pieces/system/keybinds.pdl`. Do not restore an old
  camera. INT `0` and `1`–`4` on the nine pet daemons already append
  the key to that game's own session (`47257dd34`). POV 2 and 3 rows
  were added to those city `keybinds.pdl` files. mcl-hq was rebuilt.
  The other eight daemons were not.
- `xyzfs/users`. Never `git add` it. Never switch the live checkout
  off branch `claude` (that branch still tracks user data; a switch
  deletes it).
- Do not kill `khtpm_core_render` windows you did not start.
- Editor behavior for rpg-edit is not designed. Do not add tools.

## Git

Commits for this work are on branch `grok`, worktree `/tmp/grok-bb`.
Latest relevant commits: `170a277b6` the edit board, `3e84d0c7f` the
Ava cast note, `47257dd34` the INT camera forward. Not pushed unless
the owner asks. Commit with an explicit pathspec from that worktree.
The live tree stays `claude`. Do not `git add -A`, stash, or reset.

## Next push, in order

1. Confirm the owner can see the edit window (book `edit`, page
   `start`). If the process is up and they still cannot see it, raise
   that window. Do not open piececraft.
2. When they name the TON file, swap `start` for that map. Keep the
   eight footer names.
3. Pet chrome (left acts, right chat, minimize, clock, INT) is the
   must-have list in `16.game/PET-HQ-MUST-HAVES.md`. The edit board
   is still the plain pc-hq template, not that chrome. Add it only
   when they ask.
4. Do not start a daemon, a party walker, or editor tools until they
   ask.
