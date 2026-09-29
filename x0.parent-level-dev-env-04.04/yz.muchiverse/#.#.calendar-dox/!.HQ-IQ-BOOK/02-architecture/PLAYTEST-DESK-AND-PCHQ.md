# Playtest: desk, PC-HQ, and one entity

The play button for a book:page is the taskbar Player menu. It is
the same job as RPG Maker's Play: start playback, stop it, and reset
the entities and common events on the desk that is open. PC-HQ uses
the same flag so a game window can be playtested the same way. An
entity may also carry its own play control, so that one entity runs
without starting the rest of the page.

This is not `play_event.sh`. That script runs one `event.pal` and
also every other common event whose trigger is on-click. The
receipt-page proofs in the 2026-09-23 notes used `prisc+x` on one
pal. They never flipped play mode. Do not cite them as a desk
playtest.

## The flag

`44.xyz.01.00/#.desktop/khtpm_play_mode.state.txt`

One line, `mode=on` or `mode=off`. Absent file means off. The desk
taskbar and PC-HQ both read and write this file. It is not the
livedesk clock, and it is not the organ clock.

Checked 2026-09-23. The file started `on` and was left `on`.

## Desk path

Taskbar cell 9. `khtpm_taskbar_manager.c` builds it in
`livedesk_build_player_menu` when `which == 9`. The strip item is
`strip-cell-9`, label `player`. Fallback rows:

- `1.play: ON` or `1.play: OFF` — `livedesk:play-toggle`
- `stop` — `livedesk:play-stop` (force off)
- `reset` — `livedesk:reset-entities`
- Cancel

On the live strip, pid 132125, focusing nav 9 and pressing Enter,
then Enter on the play row, turned `mode=on` into `mode=off`. The
same gesture turned it back.

## PC-HQ path

`@.apps/piececraft-hq/pchq-board.xhtpm` has a Player dropdown added
2026-09-15 to match the desk menu. The rows call
`ops/pchq_board_action.sh <session_dir> player toggle|stop|reset`.

`stop` writes `mode=off`. `toggle` flips the flag. `reset` leaves
the flag alone and, when the board has a current map id, writes
`CONFIRM_START_MAP` for that map.

The script exits before the player branch unless the first argument
is a real session directory. A bare name does not change the flag.
Checked with `pieces/sessions/1789878426-114528`: stop went `off`,
toggle went `on`, and a second stop and toggle did the same.

## One entity

Some entities carry their own play control so one entity can be
playtested without the other entities on the page. That control was
not driven in the 2026-09-23 check. Do not treat the page-wide flag
as a substitute for it.

## What is still design

`11.brainstorm` holds `PLAY-MODE-ENTITY-HARNESS-AND-MOVE-EVENTS`
from 2026-09-14. The PC-HQ action script says the per-entity
Play-Mode context menus in that design are not built. The flag
above is the part that is real.
