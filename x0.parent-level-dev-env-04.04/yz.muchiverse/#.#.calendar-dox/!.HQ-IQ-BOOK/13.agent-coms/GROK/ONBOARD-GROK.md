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

## Where the next session starts

Skip the dump-path and the movement-path work. Start on playability
and the editor:

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

## Do not redo

- Do not treat key `p` as the 2D or the 3D picture.
- Do not use arrow codes 200–203.
- Do not launch `mcl-hq` to test the editor.
- Do not edit `petslike_core.c` for an editor slice.
- Do not mint, rename, or delete pets.
- Do not stage runtime files (`*.pdl` state, logs, `scene.raw`, pid
  files) into a code commit.
