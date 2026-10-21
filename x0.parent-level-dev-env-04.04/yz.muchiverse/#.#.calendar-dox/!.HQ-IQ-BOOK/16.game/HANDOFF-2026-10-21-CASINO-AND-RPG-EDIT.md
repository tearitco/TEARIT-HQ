# Handoff 2026-10-21 — Casino Island, then rpg-edit-hq

Read this before the next game task. Nothing in this note is built
except what it says already exists. Do not invent a camera. Do not
edit `bv_menu_input.c`, `bv_render_3d.c`, or
`piececraft-hq/pieces/system/keybinds.pdl`.

## Where Casino Island is

It is not a page on the board the owner had open (book `TON`, page
`centroid-u`). It is not named `rpg-bet`.

It is the LyfeDesk window **Casino Island**, app id `bet-rpg`:

- Folder: `44.xyz.01.00/@.apps/bet-rpg`
- Launch: `sh 44.xyz.01.00/@.apps/bet-rpg/button.sh`
- Title row: `toy.pdl` says `Casino Island`
- Cast in `party.pdl`: Vale, Nia, Sol, Kit, Bram, Wynn on People4
  indexes 0–5. Not the shooter (Actor2). Not the studio (Actor3).
- Design: `16.game/BET-RPG-DESIGN.md`. The status line at the top of
  that file still says "nothing is built". That line is stale. The
  folder, the walk window, and the five table games are in the tree.
  The night hotel bill is not.

The owner later said the dropdown name they were looking for is
`casino-island`, not "bet island". The folder is still `bet-rpg`.
The window title is Casino Island. Do not rename the folder unless
they ask. `bp_tree.sh` still does not list it.

Chrome law that later edits of this window must keep: one footer of
the six pets, game acts on the left bar only, right bar is chat, both
bars minimize and the map grows. An older sentence in
`BET-RPG-DESIGN.md` and in `16.game/00-INDEX.md` that puts the acts on
the footer is out of date.

## Next project: rpg-edit-hq

The owner asked for one more pet-like project. Internal id
`rpg-edit-hq`. Do not decide how the editor works in the first pass.
Do not start it until this handoff has been read.

What the first pass is:

1. A new app beside the others: `44.xyz.01.00/@.apps/rpg-edit-hq`.
   `toy.pdl` title is `edit`, so taskbar cell 12 (the toys menu)
   lists it. `button.sh run` opens the pc-hq board through
   `piececraft-hq/open_pchq_board.sh`. That row is in place. The
   own pc-hq instance, daemon, and party are still later. Not a
   mode of bet-rpg or rpg-pvp-hq.
2. Cast, locked 2026-10-21 from `sessions/s1/desks/teru-test.pdl`
   and `&.widgits/db-hq/data/actors.pdl`. Trainer is **Ava**, not Asa.
   Footer is these six, in this order. Glyphs are the desk marks
   (actor face/character fields are empty; there is no separate
   Terumon PNG sheet):

   - Ember, `terumon_001_ember`, 🔴
   - Glacine, `terumon_002_glacine`, 🔵
   - Murmur, `terumon_003_murmur`, 🟣
   - Solvent, `terumon_004_solvent`, 🟢
   - Ava, trainer, 👩
   - Cursword, 🗡️

   Robot `robot_chat_001` (🤖) is the extra only if a seventh slot
   is asked for. Do not put Asa on the footer. Do not reuse the
   casino, shooter, or studio names.
3. The pet chrome from `16.game/PET-HQ-MUST-HAVES.md`: one footer of
   pets, acts on the left, chat on the right, minimize, shared
   day/night clock, INT, and `0` / `1`–`4` forwarded to that board's
   own session (the nine daemons already do this as of `47257dd34`).
   People stay extruded. Billboard is furniture only.
4. The first map is the RPG Maker board the owner named, not a newly
   painted town. Book `TON`, page `centroid-u`. The picture they
   attached is that page: floating islands, a cabin, bridges, water.
   Open that book and page. Do not redraw it as cobble or as Casino
   Island. If the book id on disk is not the letters `TON`, find the
   book whose label is TON and whose page label is `centroid-u`
   before copying any files.
5. Window title is a LyfeDesk name. Internal folders stay
   `rpg-edit-hq`. Do not write the old "livedesk" spelling in the title.

Out of scope until the owner says otherwise: editor tools, saving a
new map, chips, combat, camera equation changes, and sitting anyone
at a casino table.

## Order for the next session

1. Answer "where is Casino Island" with the path and `button.sh`
   above if they still cannot see it. Do not rebuild it.
2. Find book TON / page centroid-u on disk and write the path into
   this note.
3. Scaffold `rpg-edit-hq` as an empty pet window on that map, with a
   new cast. Proof is the window open on that page, six new names on
   the footer, and INT moving one of them. A clean compile is not
   the proof.
4. Commit on branch `grok` with an explicit pathspec from the grok
   worktree. Do not commit the live `claude` checkout. Do not push
   unless asked. Do not `git add` anything under `xyzfs/users`.
