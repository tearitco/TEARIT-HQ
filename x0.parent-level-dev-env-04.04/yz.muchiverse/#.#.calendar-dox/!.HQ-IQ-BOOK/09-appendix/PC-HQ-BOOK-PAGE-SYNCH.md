# Book, page, and Synch

Indexed notes for the next agent. Player > Synch from pc-hq reads the
active livedesk desk `.pdl` and writes
`@.apps/piececraft-hq/pieces/display/synched_entities.txt`. The board
draws those names. Synch from the taskbar appends `hero_01` to that
desk file if it is not already a row. It does not relaunch desk pals.

## Where the row is

| Place | Which control | Code |
|---|---|---|
| pc-hq board | Player, the 5th toolbar tab | `pchq-board.xhtpm` item `pm-synch`, verb `player synch` in `pchq_board_action.sh` |
| livedesk taskbar | Player, cell 9 | `livedesk_build_player_menu()` command `livedesk:synch-from-pchq` |

Menu (pc-hq tab 4) stays for dynamic entries. Do not put Synch there.

The taskbar binary must be rebuilt (`ops/build_khtpm_strip.sh`) and
the taskbar relaunched before cell 9 shows the row. The pc-hq window
must be relaunched before it reads the new `pchq-board.xhtpm` row.

## Two different BOOK:PAGE pairs

These were given the same words. They are not the same files.

Livedesk, `khtpm_taskbar_manager.c`:

- `ktb_get_file_label` prints `book:<session name>`.
- `ktb_get_desks_label` prints `page:<desk name>`.
- The function names still say session and desk. The comment dated
  2026-09-22 says a session is a BOOK and a desk is a PAGE.

pc-hq, `pchq-board.xhtpm` plus `pchq_board_projector.c`:

- Tab `tb-file` is labeled Book. Its rows are map projects
  (`load-map`: default, mineclonia_sample, cdda_sample, test_walls,
  test_terraces). The comment in the projector says file = directory,
  desk = map (`PALCRAFT-DESIGN.md`).
- Tab `tb-desk` is labeled Page. A choice writes
  `CONFIRM_SET_DESK:<id>` into the widget command inbox.

A 3D map inside a loaded project is `pieces/system/board_manifest.txt`
(`z_base`, `z_count`) and one text grid per layer. Entities for the
hero live under that project's `pieces/`, including `hero_01/state.txt`.

## What Synch is supposed to do

Approved, not built:

- Taskbar Player > Synch replaces the desk's current page with the
  last active pc-hq book and page, entities included.
- pc-hq Player > Synch replaces that window's book and page with the
  desk's current book and page, entities included.
- It is a copy of the page, not a label change.
- One entity shown on both the desk and in pc-hq, moving together,
  waits until both windows are on that same page.

## Open questions

1. Which directory is "the last active pc-hq"? The board window does
   not write a single `last_pchq.txt`. The projector knows
   `active_level` for the Book list. Nothing yet records which pc-hq
   window was focused last when two are open.

2. Does "copy the page" mean the map project directory
   (`pieces/system/maps/<name>/`), the livedesk desk directory, or
   both? They are different trees that happen to share the words book
   and page.

3. Which entity files move with the page? `hero_01/state.txt` is one
   file. Desk pals are separate packages and are not under that hero
   path. A copy that only takes the map grids will not move Asa. A
   copy that takes every pal on the desk is a different operation.

4. The 3D diamond is a stack of wire `#` grids, one per Z layer, read
   with the same `z_base` / `z_count` the map uses. The desk placer
   reads one flat file, `TP_RANGE_MATRIX`, written by
   `tp_gen_range_matrix`. Nobody writes one matrix per layer yet.

5. Hero Act Move still exits `move_entity_on_desk.sh` before
   `tp_arm_placer_rmmv` when the entity path contains `/pieces/`.
   The desk diamond and the pc-hq diamond are still two drawers.

The review that led here is
`12.calendar/2026-09-29/pc-hq-placer-and-synch-review.txt`.
