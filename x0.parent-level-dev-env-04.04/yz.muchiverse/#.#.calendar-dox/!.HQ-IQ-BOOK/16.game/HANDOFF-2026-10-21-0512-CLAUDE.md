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

## Added 2026-10-21 05:19 PDT — the map is not RPG Maker yet

The owner said so. `start/map.txt` is still the 16x16 glyph patch.
The next build of this board replaces that with the same kind of
town the other pet boards use:

- Buildings from RPG Maker overworld art (`World_B` cells, the way
  `mcl-hq/rooms.pdl` `SCENERY` rows do). Not cobble blocks, not the
  shooter's house stand-in.
- Trees that stay trees (`Outside_B` on the other boards).
- Rooms and doors. A door teleports in and out. The trigger is the
  floor in front of the door. Same `ROOM` / `DOOR` / `SCENERY` row
  shapes as `mcl-hq/rooms.pdl`.
- The eight sprites wander: Ember, Glacine, Murmur, Solvent, Ava,
  Cursword, Asa, robot. They are on the footer already. They do not
  walk yet. There is no daemon. Do not use People4, Actor2, or Actor3.

Book TON / page centroid-u is still not a file on disk. If the owner
points at that floating-island picture, that page becomes the
outside. Until then, build the town on book `edit` page `start`.

## Later, this window is an editor

Not in this pass. The owner said the same window will later work the
way the palette tools and the events editor work: you edit an RPG
game's events from it. Palettes pick the art. Events are the pages
the pets already run (doors, speech, the walk). Do not invent a
second event format. Do not build the editor until they ask. The
town and the walking sprites come first.

Hotbar `_` on this board was fixed in `b30627d89`. It writes
`rpg-edit-hq/board/state/pchq/visible.txt`, not piececraft's. `grok`
was pushed through that commit at 05:19. New commits still go on
`grok` and are not pushed unless they ask again.
