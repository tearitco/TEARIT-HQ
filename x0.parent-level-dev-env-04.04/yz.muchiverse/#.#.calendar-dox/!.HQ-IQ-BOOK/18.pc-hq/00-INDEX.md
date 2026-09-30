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

Older design notes under `08-roadmap/design-docs/` (`pc-hq-INDEX.md`,
`PCHQ-3D-RAYCAST-AND-VOXEL-HIGHLIGHT-DESIGN.md`,
`PCHQ-3D-MOVE-OVERLAY-DESIGN.md`) still say the ray and the move
overlay are design-only. Those lines are stale. The code has moved
past them.
