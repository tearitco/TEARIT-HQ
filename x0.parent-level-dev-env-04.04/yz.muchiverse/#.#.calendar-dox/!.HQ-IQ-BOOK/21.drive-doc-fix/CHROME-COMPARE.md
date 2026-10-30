# Chrome compare — LyfeDesk strip against the edit board

Required before any sentence that says the toolbar matches LyfeDesk.
`HOW2-DRIVE-DOC-FIX.md` used to say "follow LyfeDesk" and then name one
row (blueprintz on player). That sentence does not walk the strip. The
clock and the pid cell were never a named row, so the loop never
compared them. The owner had to say the order.

Source of the left column: `#.desktop/livedesk_taskbar.pdl`, keys
`strip_btn_N_label` and `player_menu_N_label`. Read that file. Do not
copy this table forward if the pdl moved.

Source of the right column: `@.apps/rpg-edit-hq/board/pchq-board.xhtpm`
tab order, then one `entity_menu_frame_<render_pid>.txt`. A tab with
`class="no-nav"` must show nav `0` in that frame.

A blank cell is a miss. Do not invent a tab for a strip button this
board does not have. Write "absent" and leave it.

Filled 2026-10-29 from the pdl and from `entity_menu_frame_3326873.txt`
(book through clock on one row, In wrapped, pid nav 0). Die hit 94 C
on that launch. The window was stopped. Daemon 1400329 was left up.

## Strip order

| n | LyfeDesk | edit board | match |
|---|---|---|---|
| 0 | file | `tb-book` (the open book) | same slot. Menu is the book list, not new / save / save-as / load. |
| 1 | desks | `tb-page` | same slot. Menu is that book's desks. |
| 2 | pals | `tb-pals` | same slot. |
| 3 | palettes | absent | not invented. |
| 4 | edit | absent | not invented. |
| 5 | player | `tb-player` | same slot. Row order is its own table below. |
| 6 | db | `tb-db` | same slot. Rows are db-hq and sql-hq. |
| 7 | plugins | absent | not invented. |
| 8 | store | `tb-shop` (Shop) | same slot. Label is Shop, not store. |
| 9 | network | absent | not invented. |
| 10 | menus | `tb-menu` | same slot. |
| 11 | datetime | `tb-time` (clock, `D229 10:06` on the frame, nav 8) | same slot. Menu is game speed, not clocks & cals. |
| 12 | tools | absent | not invented. |
| 13 | (reserved) | absent | not invented. |
| 14 | h-ai | absent | not invented. |
| after the strip | no strip button | `tb-in` (In) | board-only. Sits after the clock. Wrapped to the next row on a 1260-wide window. |
| status, not a button | dock pid cell is `no-nav` | `tb-load` / `tb-loadw` (`3326873 %15`, nav 0) | same rule. It draws. It is not a nav target. |

## Player menu

LyfeDesk order is `player_menu_1` through `player_menu_9`: play,
play-test, stop, blueprintz, reset, new-game, Synch, save-game,
load-game.

| n | LyfeDesk | edit board | match |
|---|---|---|---|
| 1 | play | Play (`pm-play`), after INT and mode | later than the strip. INT and mode are board-only and sit above it. |
| 2 | play-test | absent | not on this menu. |
| 3 | stop | Stop (`pm-stop`) | present. |
| 4 | blueprintz | blueprintz (`pm-blueprintz`), after Stop | same slot relative to Stop. Driven earlier: `entity_menu_frame_3312177.txt`, y=183. |
| 5 | reset | absent on player | Reset house is on `tb-menu` (`mn-reset`), which is the wrong tab. |
| 6 | new-game | absent | not on this menu. |
| 7 | Synch | two rows: desk to game, game to desk | strip has one Synch. |
| 8 | save-game | Save (slot 1) | present, not the strip's save menu. |
| 9 | load-game | Load (slot 1) | present, not the strip's load menu. |

Tab order can match while this menu does not. Do not mark the player
cell done from the tab label alone.
