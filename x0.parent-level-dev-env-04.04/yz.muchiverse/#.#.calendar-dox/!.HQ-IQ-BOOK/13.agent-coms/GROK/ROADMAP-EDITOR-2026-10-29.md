# Editor roadmap (2026-10-29)

The first user of `rpg-edit-hq` is an author opening `book:page`, not a longer walk. This is the order. Do not skip ahead.

## 1. Read-only open. Done.

`ops/edit_open.sh` and `&.widgits/_shared-lib/harness/edit_open.pal`.
`prisc edit_open.pal` passed 4 and failed 0.

A real call, no window:

```
book=rpg-space page=bridge event=col_breach pages=7 events=15
```

`book_check --events` on `rpg-space-pchq` the same day: desks pass (B1–B4, 7 pages), blueprintz passes (D1–D2, 15 events, flags=0). `conformant=no` because A2 fails: the board still keeps position in private files instead of a page file. The same A2 failure is on `mcl-hq` and on `rpg-edit-hq`. The text open does not claim the book is conformant.

## 2. Show that page in this window. Done for the 3D bar. 2026-10-29.

`open_board.sh` links `maps/<book>` at the other book's map tree when the edit package does not have it, then sends `CONFIRM_START_MAP` and `CONFIRM_SET_DESK`. The link lives under `rpg-edit-hq-pchq`, which is gitignored.

Observed on window `0xa00003`, 1260×556: the top bar read `book:rpg-space` and `page:bridge`. The debug line read `rpg-space:bridge`. The view was a ship corridor and a starfield, not the Islands room. The engine log said `Map 'rpg-space' loaded`. `board/state/ui.txt` had the same two labels. Picture: `/tmp/edit-bridge.png`.

`state/scene.raw` is still the editor daemon's own 2D paint. It did not become the bridge. A2 is still open: the page file is not the authority. The next proof of one edit is a row that changes both `scene.raw` and this window.

## 3. One edit, both pictures.

Change one entity row on the opened page. `scene.raw` and the window dump show that same row. No second list for 3D.

## 4. The other Islands doors.

Store, Hotel, School, Cabin. One arrow, one `STEP` line, then one `RULE` in `doors.fsm.pdl`. A bump stays out of the table.

## Seen while driving, 2026-10-29. Not fixed yet.

A person-style pass on the open window `0xa00003`. Frames `/tmp/human-1-open.png`, `human-2-pitch.png`, `human-3-step.png`, `human-4-bookclick.png`. Fix these in one row later. Do not start that row from this list alone.

1. The 3D picture does not fill its pane on the first frame. `human-1-open` left a black gap around the view. The pane filled only after the view changed (`human-3-step`). The owner called this a nit and still wants it fixed.
2. On open, In was off. The bar said `book:rpg-space` and `page:bridge`, and the person was not in the world until a later key.
3. Key `1` (code 49) did not change the picture. `human-1-open` and `human-2-pitch` are the same bytes.
4. One right arrow on the session relay did move the 3D view: `pos 5,4,-7` became `pos 9,8,-7`, still labeled `rpg-space:bridge`. The same arrow also wrote `STEP|trainer|store|8|15`. The daemon's Harold is in the Store. The window's Harold is on the bridge. Two positions, one key.
5. Clicking the book tab did nothing. `menu_open` and `desk_menu_open` stayed empty. `human-4-bookclick` matches `human-3-step`. There is no way, from that click, to switch back to `rpg-edit-hq`.
6. The debug block (`time`, `pos`, `ray`, `pid`) sits on top of the 3D view. The chat on the right was still talking about the Cabin while the bar said bridge.
7. Clicks on In, Page, Menu, and Pals did not open a menu. `menu_open` stayed empty across `/tmp/ux-in.png`, `ux-page.png`, `ux-menu.png`, and `ux-pals.png`. The tabs are drawn. A person cannot see the choices behind them.
8. The page menu, in `board/pchq-board.xhtpm`, runs `fire goto` on the editor daemon's rooms. That daemon's `ui.txt` listed Islands and Bank (`n_room=6`) while the bar said `rpg-space` / `bridge`. The switcher and the picture are two books.
9. The book menu's own row is "blueprintz (events of this book)". It does not list the books the launcher can link. There is no row that sends `CONFIRM_START_MAP` back to `rpg-edit-hq`.
10. Pets, coins, and chat stay the editor daemon's party. The canvas is whichever map was loaded. A pal saying "Pal 13! over here" is not an event of `rpg-space`.

What these ought to be, when the fix row starts:

- One book on screen. The bar, the page menu, the pets, and the chat all name the loaded book. The page menu lists that book's desks from its `game.pdl` (`bridge`, `quarters`, `hold`, …) and sends `CONFIRM_SET_DESK`, not `fire goto` on the editor's Islands.
- The book menu lists books the editor can link, one row each, and the current one is marked. Choosing one is `CONFIRM_START_MAP`. Switching back is a row, not a relaunch.
- In tells the truth. Off means "watching". On means arrows move the body in the picture, and the daemon does not also step a Harold in a different room.
- The first frame fills the pane. The debug block is a toggle, not the default face of the view.
- NPC talk is an event page of the open book (blueprintz), not a leftover chat from another game. Sharing a book is the link the launcher already makes, shown as a book-menu row, not a second copy of the map.

## 5. `find` stays a list.

It does not run a pal. `edit_open.pal`, `edit_doors.pal`, and `edit_last.pal` run by name.
