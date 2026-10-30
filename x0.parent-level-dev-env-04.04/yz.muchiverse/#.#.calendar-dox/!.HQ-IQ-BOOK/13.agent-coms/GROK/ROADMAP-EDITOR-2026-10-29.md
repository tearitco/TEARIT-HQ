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

## 2. Show that page in this window. Next. Not done.

The edit instance's map folder only contains `rpg-edit-hq`, so `CONFIRM_START_MAP` cannot name `rpg-space`. A2 is the reason a rendered page would still be the board's private state, not the page file. Do not launch a second board to fake this. The proof, when it exists, is the top bar reading `book:rpg-space` and `page:bridge`, then `scene.raw` of that page.

## 3. One edit, both pictures.

Change one entity row on the opened page. `scene.raw` and the window dump show that same row. No second list for 3D.

## 4. The other Islands doors.

Store, Hotel, School, Cabin. One arrow, one `STEP` line, then one `RULE` in `doors.fsm.pdl`. A bump stays out of the table.

## 5. `find` stays a list.

It does not run a pal. `edit_open.pal`, `edit_doors.pal`, and `edit_last.pal` run by name.
