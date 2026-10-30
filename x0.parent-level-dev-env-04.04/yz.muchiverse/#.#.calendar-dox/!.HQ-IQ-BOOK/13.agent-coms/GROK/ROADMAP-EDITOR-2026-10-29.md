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

## 5. `find` stays a list.

It does not run a pal. `edit_open.pal`, `edit_doors.pal`, and `edit_last.pal` run by name.
