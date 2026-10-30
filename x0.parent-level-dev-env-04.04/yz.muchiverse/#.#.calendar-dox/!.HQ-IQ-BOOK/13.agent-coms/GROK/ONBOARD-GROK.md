# Grok onboarding (read this first)

Written 2026-10-29 after a live pass on `rpg-edit-hq`. The next Grok
session reads this file and does not re-derive the items below. Longer
notes from the same pass sit next to it:
`2026-10-29-direction-3d-editor-and-test-loop.md`.

House root, quote it:

```
/home/no/Desktop/github/work/NNEST-12.00/x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00
```

Book root is `#.#.calendar-dox/!.HQ-IQ-BOOK/`. Commits for this agent
go on branch `grok` with an explicit pathspec. The live checkout is
often `claude` and dirty. Do not switch it. Do not `git add` under
`xyzfs/users`. Do not launch a second board while one engine is
starting. Nice heavy work (`nice -n 15`). Stop only the pids you
started.

## What the house is building

A book is a game. A page is a map. Address form is `book:page`. The
page file is the entity list. 3D is that same page stood up. Events
are blueprintz event pages. Actor names live in that game's own db
layer, never the shared trunk `actors.pdl`. The pc-hq camera is the
owner's. Do not restore an old yaw, pitch, or zoom on pc-hq. The
edit board's own session camera is a different file and is fair to
drive while testing.

`rpg-edit` opening any book is still roadmap item E in
`16.game/ROADMAP-2026-10-27-GAMES-STORE-CHAIN-AI.md`. The pivot is
`16.game/GAMES-AS-BOOKS-AND-THE-EDIT-PIVOT-2026-10-28.md`.

## The editor, as of this pass

App: `@.apps/rpg-edit-hq`. Launch:

```
nice -n 15 sh @.apps/rpg-edit-hq/board/open_board.sh
```

The daemon `rpg_edit_hq.+x` may already be running. Do not start a
second one. The demo town is book `rpg-edit-hq`, rooms in
`rooms.pdl`: Islands (`start`), Bank, Store, Hotel, School, Cabin.
`ops/edit_last.sh` remembers the last book and page. A missing or
dead row resolves to `rpg-edit-hq` page `start`. The edit instance's
map folder currently contains only `rpg-edit-hq`, so a remembered
`mcl-hq` does not open that game yet. The window still opens the
demo map.

On 2026-10-29 after the drive test, Harold was left in the Bank at
`12,13`. Re-read `state/trainer.txt`. Do not trust this sentence
after the next session.

## How to see the board

Three pictures. They are not the same file.

| What you want | File | What a real one looks like |
|---|---|---|
| 2D map the game painted | `<app>/state/scene.raw` plus `scene.receipt.txt` | Edit Bank frame was 1360×440, wood floor, one sprite. rpg-space's is 400×416 and has hundreds of colors. |
| 3D view a person sees | `dump_frame_png_op <window_id> out.png` on the board window | Edit window was about 1260×556, thousands of colors. |
| Key `p` | `dump_board_frame.sh` writes `/tmp/entity-menu-frame.png` from `pieces/display/rgb_frame_2d.raw`, else `rgb_frame.raw` | On both rpg-space and the edit board, `rgb_frame.raw` was almost black (2 colors). That is not a failed paint. |

`scrot` is black on the GL path. A PNG that did not change is not
proof the state failed. Read `trainer.txt` and `events.txt` first.

## How to move the player and the camera

Tested 2026-10-29 on the edit board. In: was ON. INT flag was 1.
Control was the player.

Arrows are codes 1000 left, 1001 right, 1002 up, 1003 down. The
agent relay band 200–203 does not move the xelector. `xdotool key
Right` on the window also did nothing.

The move that sticks is one line on the **board session** relay, then
a wake:

```
<bv_session>/pieces/apps/player_app/interact_relay.txt
1001
126
```

`bv_session=` is in `@.apps/rpg-edit-hq/board/state/ui.txt`. Right
moved Harold from Islands `3,4` to `4,4` and the xelector followed.
Up from that cell went through a door into the Bank at `12,13`.
Writing the same arrow only on `<app>/state/interact_relay.txt` steps
him and the xelector pulls him back, so `trainer.txt` looks unchanged.
Proof of a real step is a new `STEP|trainer|...` line in
`state/events.txt`.

Camera, same session, key code on the app relay (`state/interact_relay.txt`)
because the daemon forwards `1`–`4` with `board_cam_key`:

- `49` is key `1`. It set `camera_mode=1`, `cam_yaw=180`, `cam_pitch=6`. The picture went from looking down into a room to eye level: sky, grass, a building.
- `50` is key `2`, same pitch 6.
- `51` is key `3`, free roam, starts at pitch -90 (straight down).
- `52` is key `4`, pitch -90.
- `q` / `e` yaw by 10. `r` / `t` pitch by 10. `w` `a` `s` `d` pan.

The steep Bank picture was pitch, not a stuck player. Do not write
pc-hq's camera to "fix" the edit board.

## Reuse: a harness and an FSM, not another handwritten session

A playability fact that was proved by hand gets written down once as
a pal harness (`&.widgits/_shared-lib/harness/`: a `.pal`, a
`cases/<name>.pdl`, the shared case and verdict ops). Run from that
folder: `prisc <name>.pal`, then read `results/<name>.txt.verdict.txt`.

Use these. Do not retype the relays they already cover.

| pal | what it is |
|---|---|
| `edit_last.pal` | last book row, or Islands when the row is missing or the book is gone |
| `edit_doors.pal` | the door FSM. Table is `@.apps/rpg-edit-hq/doors.fsm.pdl`. Checker is `ops/edit_doors.sh`. The proved row is Islands `4,4` + up (`1002`) to Bank `12,13`. An unproved door is exit 2. `scene-self black` fails, `scene-self good` passes. |
| `edit_open.pal` | read-only `book:page`. Checker is `ops/edit_open.sh`. Real listing: `rpg-space` page `bridge`, event `col_breach`, 7 pages, 15 events. The order after that is `ROADMAP-EDITOR-2026-10-29.md`. |

Add the next proved door as one `RULE` row, then one `must` line in
`cases/edit_doors.pdl`. Do not invent Store, Hotel, School, or Cabin
until a `STEP` line exists.

## Choice: `find` does not run a harness (2026-10-29)

The owner left the wiring decision to this agent. The choice is: `harness_bank_op find` stays a ranked list. It does not exec the top pal.

Why. The rank is hand-written synonyms and slots, times a Laplace weight from old PASS and FAIL rows (`08-roadmap/design-docs/HARNESS-BEHAVIOR-BANK-DESIGN.md`). A nearby word can rank a heavy pal first. That folder also holds harnesses that drive a live game (`play_rpg_space_fridge.pal`, digipet day-tick). A search that runs them would move a trainer or start a board. The design itself says nothing reads the bank except `find`, and that a planner which runs pals is not built.

What does run. A session runs a pal by name when the work is that pal: `prisc edit_last.pal` or `prisc edit_doors.pal` from `&.widgits/_shared-lib/harness`. Those two do not touch a live board, a live trainer, or `xyzfs/users`.

What would change the choice. A `SAFE | scratch` row on a behavior seed, a `find` that refuses every entry without that row, and one named command that runs only the winner. Not a fuzzy search with a side effect. That command is not built. Do not build it on a guess.

Known play is an FSM, not a new C brain. States are pages (`start`,
`bank`, `store`, `hotel`, `school`, `cabin`). A transition is one
arrow plus the door that fired, stored as event-page or case rows.
`FSM for the known, GOAP for the new` (`13.agent-coms/2026-10-13/NOTE-FROM-CLAUDE-to-opencode-and-grok-pipeline-direction.md`).
The Islands door walk is known. It belongs in that table. A bump, a
view that disagrees with `trainer.txt`, or a book the editor cannot
open yet is a failed row, not a reason to rediscover the path.

The 2D check inside the harness is `scene.raw` size and a color count
above the black-buffer floor. The 3D check stays a window dump only
when a board is already open. Key `p` is not either check.

## Where the next session starts

Skip the dump-path and the movement-path work. Build the harness and
the door FSM first, then use them for playability and the editor:

1. UI. The board chrome (In, book, page, pets, chat) against the 3D
   view. What a person cannot tell from the picture.
2. Playability. Walk the Islands doors (Bank, Store, Hotel, School,
   Cabin) and record which steps bump. One arrow, one `STEP` line,
   one window dump when the picture is the question.
3. Editing state. What a save writes today (`last_book.txt`,
   `trainer.txt`, the page file) and what an edit of one entity row
   should change in both the 2D scene and the 3D view.
4. Roadmap item E. `rpg-edit` still cannot open another book. The
   row remembers `mcl-hq`. The map folder does not contain it.

A presentation, when one is actually asked for, goes in
`/home/no/Desktop/github/work/XO/!.PRESENTATIONS/<prefix>.<name>/`
with `snapshots/`, `manifest.txt` (`file | seconds | caption`), and
`REPRODUCE.md`. Render with the archive copy of
`make_presentation_video.py`. The 2026-10-29 cut is
`30.edit-board-page-bank/`. Do not build one for a black `p` frame.

## Dropdown thumb sits in the menu (owner frame, 2026-10-29 23:00)

The book menu on the edit board is open. The scroll thumb is in the
middle of the rows. On pc-hq it sits at the right of the menu. Do not
invent a new scrollbar. Two existing paths already draw this thumb.

The taskbar / dock menu (`layout_dock_bar` in
`_.monads/_.livedesk-taskbar/ops/khtpm_core_render.c`) is the one that
looks right. When the list scrolls it adds `DD_STRIP_W` (62) to the
menu width and sets each row to `col_w - DD_STRIP_W`. The `^` / `v`
arrows and the thumb live in that strip, to the right of the labels.

The edit board does not take that path. Its book rows are
`<item class="dropdown-child">` inside `<sidebar id="rail">` of
`@.apps/rpg-edit-hq/board/pchq-board.xhtpm`. Those are laid out by the
sidebar dropdown pass in the same C file (the comment that starts
"menu shape: a windowed list"). That pass sets the row width to the
trigger width, caps it at 320, and registers the generic bar at
`dw + 66`. The bar is then not at the right edge of the painted rows.
`.dropdown-child { width: 210px }` is in both `pchq-board.css` files.

A second thumb is the pet column. `<row id="side-l" class="ov-scroll">`
registers `generic_sbar_register` on the pinned bar's own width
(same file, the `ov-scroll` comment, 2026-10-23). The book menu opens
on top of that column, so the pet thumb draws through the middle of
the menu. pc-hq's board has no `ov-scroll` pet bar.

Fix with those two pieces. Do not add a third scrollbar, and do not
bring back the pals commit that capped a dropdown at 12 rows and
never drew a thumb (`04-bugs/bug_bounty.md`, 2026-09-22). While a
dropdown scope is open, skip the `ov-scroll` register. Give the
sidebar dropdown the same `DD_STRIP_W` split the dock menu already
uses, so the row width and the thumb share one menu width.

The see stage missed this. A long menu is not done when the labels
are distinct. The thumb has to be on the right of the rows, the same
place pc-hq puts it.

## Do not redo

- Do not treat key `p` as the 2D or the 3D picture.
- Do not use arrow codes 200–203.
- Do not launch `mcl-hq` to test the editor.
- Do not edit `petslike_core.c` for an editor slice.
- Do not mint, rename, or delete pets.
- Do not stage runtime files (`*.pdl` state, logs, `scene.raw`, pid
  files) into a code commit.
