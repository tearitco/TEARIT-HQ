# 18 — pc-hq

The front door for the piececraft board: the pc-hq window, its 2D and
3D drawers, the hero, and how that window is supposed to share a page
with the livedesk. The game machine (actors, events, playtest) is
chapter `16.game`. This chapter owns the board itself.

- `LEARNINGS.md` — start here. What a later agent keeps having to
  rediscover: the diamond, the daemon, the dump, Book/Page, Synch,
  and the canvas right-click.
- `SYNCH.md` — the agreed direction. Taskbar Synch sends the desk's
  book and page to every open pc-hq. A pc-hq Synch sends that
  board's book and page to the desk. Both then show that one page.
  Read this before changing Synch code.
- `PAGE-FILE.md` — the storage change Synch depends on. pc-hq reads
  the livedesk page file. The hero is one entity row. The camera
  follows the xelector. The xelector and the camera become rows in
  that file.
- `MOVE-AND-TAKE.md` — **status + roadmap (2026-10-05).** pc-hq Move is
  built (range finder, placer, z levels, animation) on a library shared
  with the desk. Lists what is verified vs only compiled, the open items
  in order (hero position row, post-move tick hook, z-falloff decision,
  Take, Place/Mine/Build), and the relay test recipe. Rule it follows:
  the desk is the functional parent.
- `XELECTOR-ENTITY.md` — the xelector is an entity in pc-hq (cell, possession,
  menu), now on the bottom bar; the livedesk has none yet. Lists what to decide
  before it gets one (cursword may be the desk analogue — unconfirmed).
- `INTENDED.md` — what commit `38b775390` was supposed to do, the
  check that passed, and the taskbar-binary hole if the rows vanish.
- `../09-appendix/PC-HQ-BOOK-PAGE-SYNCH.md` — older notes. The
  2026-09-30 header on that file describes `079038706`, which is
  not the agreed direction. Trust `SYNCH.md`.
- `../02-architecture/PLAYTEST-DESK-AND-PCHQ.md` — taskbar cell 9 and
  the Player menu as a playtest button. Synch on that same menu is
  not the playtest.
- `../12.calendar/2026-09-29/pc-hq-placer-gaps.txt` — the gap list from
  the day the diamond and the hero menu were still only questions.
- `../12.calendar/2026-09-29/pc-hq-placer-and-synch-review.txt` —
  questions asked before the row was built. Its "Not started" line
  is older than the row. Trust `LEARNINGS.md` for status.

- `CURSWORD-POSSESSION-DESIGN.md` - cursword as a possessor (xelector is
  its scaffolding), shared inventory/hearts/hunger HUD, and Place/Take
  built on both. Design only, with what exists and what is missing.
- `BUG-CURSWORD-ARMED-MENU-KEYS.md` - armed cursword eats the arrow keys
  and Esc meant for its own context menu. Cause traced, fix proposed,
  not applied.

Older design notes under `08-roadmap/design-docs/` (`pc-hq-INDEX.md`,
`PCHQ-3D-RAYCAST-AND-VOXEL-HIGHLIGHT-DESIGN.md`,
`PCHQ-3D-MOVE-OVERLAY-DESIGN.md`) still say the ray and the move
overlay are design-only. Those lines are stale. The code has moved
past them. The Move overlay is now built: see `MOVE-AND-TAKE.md` for what
landed and what is still open.
- `IN-GAME-LAYOUTS-PLAN.md` — **plan (2026-10-06).** Draw user-made menus and bars inside the
  pc-hq board window (not floating windows), saved as layouts and called by name from the Events menu
  (and on the livedesk), made in an agent-usable layout studio. Lists what exists (the canvas overlay
  strip the hotbar uses), the gaps (no overlay chrome, one footer, floating menus leave the viewport),
  the phases, and how it builds on the layout-studio seed doc.
