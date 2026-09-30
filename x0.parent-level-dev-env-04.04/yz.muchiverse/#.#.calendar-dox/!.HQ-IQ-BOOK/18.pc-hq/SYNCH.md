# Synch direction

Agreed 2026-09-30, before the code changes. Game-dev use: both
windows show one book and one page so the desk and pc-hq can be
looked at together. This page is the target. The code on
`079038706` does not do this yet.

## What a press does

Synch copies one side's book and page onto the other side. Both
then show that same book and page. The names are not exchanged.
The taskbar press does not write `hero_01` into the desk file.

| Press | Source | Who changes |
|---|---|---|
| Livedesk taskbar, Player, cell 9 | the desk's current book and page | every pc-hq that is open |
| One pc-hq, Player, tab 5 | that board's current book and page | the livedesk taskbar |

A taskbar press reaches every open pc-hq. A pc-hq press reaches
the taskbar. It does not push that page out to the other boards.

## What "the same thing" means

The shared thing is the map. pc-hq and the livedesk read the same
grid. The livedesk draws the 2D slice of that page. pc-hq draws
that same page in the board window, 2D or 3D.

Seen together on 2026-09-30, before a Synch that does this:

- Desk: `book:pre-design`, `page:teru-test`. Dark cell grid. Hero,
  clapper, robots, terumon, and the other desk pals on cells.
- pc-hq, 2D: `book:test_walls`, `page:Desk 1`. Brown floor, green
  wall columns, trees, hero, chicken.

After a taskbar Synch, every open board shows `pre-design` /
`teru-test` and draws that grid. After a pc-hq Synch, the desk
shows `test_walls` / `Desk 1` and draws the 2D slice of that grid.

## What the script does now

`pc_synch_request.sh` follows the table above. A taskbar press
writes the desk's book and page into
`@.apps/piececraft-hq/pieces/display/open_book_page.txt`. The
drawers read that file when it names a real desk file. A pc-hq
press writes the board's page into the desk session's
`active_desk`, and takes the board's book only when that session
directory is already on disk. It does not write a `hero_01` row.
`079038706` was the older name-list script. It is not what a press
runs now.
