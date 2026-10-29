# Grok note: direction, the test loop, and the 3D editor (2026-10-29)

Written by Grok after the owner asked for the open questions to be
researched in the house first, then written down so they are not
forgotten. Reread this file. It lives on branch `grok` under
`13.agent-coms/GROK/`.

Labels: **READ** = read from a file this session. **RAN** = a command
this session, no window launched. **SURE** = the source says it.
**INFER** = a conclusion from those reads. **OPEN** = only the owner
can answer.

The co-lab room that led here is session `1793270969`. Claude's last
approved note was questions 6 and 7. Claude then hit its limit. This
file answers those questions and the five questions Grok had asked
the owner.

## 1. What the house is aiming at (READ)

The pivot is `16.game/GAMES-AS-BOOKS-AND-THE-EDIT-PIVOT-2026-10-28.md`.

- A book is a game. A page is a map. Address form is `book:page`.
- The livedesk page file is the entity list. Terrain stays the map
  grid. 3D is that same page stood up. 2D and 3D read and write the
  same row (`18.pc-hq/PAGE-FILE.md`, cited in the pivot, section 1b).
- Events are event pages scanned by blueprintz (`bp_scan` flags=0,
  `book_check --events`). Shared behavior is a small op, an event
  page, or a pal. Not a new header, and not a copy of
  `petslike_core.c`.
- Names of actors live in the per-book db layer. The trunk
  `actors.pdl` / `system.pdl` is shared. Do not edit the trunk.
  Map names live in the game's `rooms.pdl`, not the db
  (`GROK-HANDOFF-LATEST-2026-10-28.md`).
- rpg-edit-hq is still a petslike skin with its own maps. It is not
  yet the editor that opens any book. Roadmap item E,
  "rpg-edit opens any book", is still open in
  `16.game/ROADMAP-2026-10-27-GAMES-STORE-CHAIN-AI.md`.
- The pc-hq camera is the owner's. Do not restore an old yaw, pitch,
  zoom, or keybind (`pc-hq-camera` skill). `board_ctl camera` and
  `camset` write the session camera. Do not use them unless the owner
  asks for that camera change.

## 2. The five questions, answered from the house

### First deliverable

**INFER.** The first deliverable is not the MCL walk slice. The
owner's priority in the handoffs is events and names through
blueprintz and the db, and the pivot says rpg-edit must open a book
before more game features are shippable. A design-only note is not
enough by itself. The first code slice should be read-only: one book
listed as pages, one event page shown, text proof first.

### Which window may be opened

**READ.** A board launch froze the machine on 2026-10-28. Cause
unknown (`GROK-HANDOFF-LATEST-2026-10-28.md`). `board_ctl recover`
launches `button.sh` and can start the piececraft engine. That is a
live window. **OPEN** remains only: whether that one named app may
be preflighted after the text checks pass. Until that yes, tests
stay harness, `book_check`, and state files.

### What the "p" PNG is

**SURE.** In `board-viewer/ops/bv_menu_input.c` the key `p` (code 112)
is the house PNG dump. The comment says the board has no X window id
of its own, so the bytes are the 2D frame it just painted, written
through `g_dump_sh`, and the log is `/tmp/entity-menu-frame.log`.
The same code 112 is the last-resort PNG in
`10-user-docs/_.0.aigent-testing-u10.txt`.

**SURE, different tool.** `dump_frame_png_op.+x <hex_window_id>
<out.png>` captures a real X window, including a 3D canvas.
`board_ctl snap` waits until the engine rendered, then dumps, and
exits 3 if the picture matches the previous one. `scrot` is black on
the GL path (rpg-space driving skill).

**INFER.** For "did the state change", use the text file. For "does
the 2D board frame show it", press `p` or the relay equivalent and
read `/tmp/entity-menu-frame.png`. For "does the 3D canvas show it",
use `dump_frame_png_op` or `board_ctl snap` on the window that owns
the GL view. A stale PNG is not proof the change failed. Re-read the
text state.

### Slice order

**INFER.** Walk, then sand/gravel gravity, then names, is the MCL
lane in the 2026-10-28 handoff. It is not the house direction. The
direction is: book compliance, then rpg-edit opens a book, then
event pages and db names, with 3D as the same page stood up. MCL
walk can wait. `do_step` already exists in the shared core.

### Branch and pets

**SURE from the rules, unchanged.** Branch `grok` only. Pathspec
commits. No `petslike_core.c` edits. No mint, rename, or delete of
pets. No `xyzfs/users` in git. This note follows that.

## 3. How to test (READ + RAN)

Order, from `_.0.aigent-testing-u10.txt`. Do not skip ahead.

1. File relay. Append. Do not truncate. For a board, `board_ctl key`
   and `board_ctl click`. Arrow relay code 200 does not move the
   xelector.
2. Cheap text state. `board_ctl expect` on `state/` keys
   (`trainer.txt`, flags). `book_check --events`. A harness pal under
   `&.widgits/_shared-lib/harness` run with
   `/home/no/scratch-gab/bin/prisc`.
3. PNG last. Key `p` / code 112 for the board's 2D frame. Window dump
   only when the question is pixels.
4. xdotool only when 1–3 cannot do it. `board_ctl key` already moves
   the pointer because focus follows the mouse.

**RAN this session, no window.** These are executable:
`dump_frame_png_op.+x`, `board_ctl.+x` (usage line printed),
`book_check.+x`, `/home/no/scratch-gab/bin/prisc`. `gcc` is Ubuntu
11.4.0. `board_ctl` with no args prints usage and exits 128. No
quota number was measured. Do not invent one.

## 4. What blocks a step (Claude question 7)

**SURE.** `do_step` is `petslike_core.c` around line 890. It calls
`passable`. If that is false it writes `bump` and returns 0. If it
moves, it appends `STEP|...` to `events.txt`. A door cell then
teleports.

**SURE.** `passable` (same file, line 869) is false when the cell is
outside `0..COLS` or `TRIG_ROW..ROWS`, or `floor_blocked` is set, or
`other_at` is set (another body on the cell).

**SURE for mcl-hq.** `floor_blocked` is in
`@.apps/mcl-hq/ops/mcl_hq.c` line 132 (the skin, included into the
core). It returns blocked for: a cell `mb_find` already occupies, an
overmap cell that is not land (ocean), a scenery rectangle, and a
placed item whose category is not `wall`. A placed item with
`cat[].wall` is skipped (`continue`), so that decor does not block.
That matches Claude's correction: in this catalog `wall` means decor
on the wall row, not a blocking tile.

**INFER.** Three blocking kinds to name in a test: the map edge, a
solid placed block or scenery or ocean, and another pet on the cell.
Do not use the `wall` category as the "wall blocks" mutant.

## 5. Recommended features (not started)

These match the pivot and the part of the work that is visible and
already specified. None of them is a license to launch a board.

1. **Read-only book open in rpg-edit.** List one book's pages and one
   event page. Proof: `book_check --events` still green, blueprintz
   flags stay 0, a text listing of page id and event name. No new
   map format. This is the open half of roadmap item E.
2. **One entity row, two views.** An event page writes one entity row
   on a page. 2D and 3D both show that row because they already share
   the page file. Proof is the row text first, then a `p` PNG of the
   2D frame, then a GL dump only if the owner has allowed that
   window. Do not add a second entity list for 3D.
3. **Names through the per-book db, on a scratch copy.** Branch
   `actors.pdl` into that game's `db/` with `db edit`. Rename one
   actor. `db status` must not show a trunk edit. Restore the scratch
   copy after the proof. Never mint a pet.
4. **MCL walk and gravity stay later.** Walk is `do_step` plus
   `passable`. Gravity for sand and gravel is rows on the rules page,
   one cell per tick, not new C. Do that after the editor can show
   the page the rows belong to.

A feature that needs the whole machine is not done. If a later test
launches anything, it is `nice -n 15`, one process, and the commit
says the cost.

## 6. Still open (owner)

1. Which book is the first read-only open: `mcl-hq`, `rpg-space`, or
   rpg-edit's own maps?
2. After the text proof, may `board_ctl preflight` run on that one
   app? A board launch froze the machine on 2026-10-28.
3. When a picture is required, is the proof the board `p` file
   `/tmp/entity-menu-frame.png` (2D frame), or
   `dump_frame_png_op` on the LyfeDesk window (3D canvas)?

Everything else in this note is settled enough to start slice 1 in
section 5 once those three are answered.

## 7. Owner answers, checked on the machine (2026-10-29, later)

The owner answered the three open items: the editor opens whichever
book was last edited, or an empty/demo project; run what is needed;
test which picture is the proof.

**Last edited vs demo. RAN, no new window.** There is no last-edited
picker in `rpg-edit-hq/button.sh`. That script opens the floating-island
edit board. The daemon already running (`rpg_edit_hq.+x`, nice 15)
has `state/ui.txt` saying `book:rpg-edit-hq` and `page:Bank`.
`rooms.pdl` names the demo town: Islands, Bank, Store, Hotel, School,
Cabin. That is the demo project, and it is what is open in the
editor process now.

The desk copies under the owner's livedesk `sessions/` are not a
last-edit list. `cdda-hq`, `civ-rpgce`, `df-hq`, `dsr-rpg`, `gta-hq`,
`mcl-hq`, `rpg-dust`, `rpg-edit-hq` and `tcg-rpgce` share one
`session.pdl` timestamp (`1793237243`), so the last name in that
install (`tcg-rpgce`) is not "last edited". The newest desk book with
its own earlier stamp is `rpg-space` (`bridge`, `1793193932`).
`pre-design` / `office` is older (`1792593145`).

**INFER, the rule to build.** On save, write one row naming the book
and page. On open, read that row. If the file is missing or the book
is gone, open the demo `rpg-edit-hq` Islands (page `start`), not a
blank map and not `tcg-rpgce`.

**Pictures. RAN.** `dump_board_frame.sh` on `rpg-edit-hq-pchq` exited
1: `no 2d frame`. The edit board session directory
`1793230715-81882` is deleted; a `pc_trigger_watcher` still has that
deleted cwd. `/tmp/entity-menu-frame.png` was not created. Key `p`
only converts `pieces/display/rgb_frame_2d.raw` into that PNG. It is
the 2D frame the board painted, and only after a live board session
exists.

`dump_frame_png_op e00002 /tmp/grok-tile-dump.png` on the already-open
cursword tile wrote a real PNG, 80x80, 1060 bytes, exit 0. That tool
is the X-window capture. It sees a LyfeDesk window, including a 3D
canvas when that window is the one you pass. No board was launched.
Load after the dump was 1.69. The 2026-10-28 15:21 freeze is still a
reason not to start a second board while this daemon is up.

**Which file proves what.** Text state first. Key `p` proves the 2D
board frame, and it failed closed here because that frame was absent.
`dump_frame_png_op` proves pixels of a real X window, and it worked
on a window that was already open. A 3D proof waits until a board
window is actually on screen. Do not treat a missing `p` PNG as a
failed edit.
