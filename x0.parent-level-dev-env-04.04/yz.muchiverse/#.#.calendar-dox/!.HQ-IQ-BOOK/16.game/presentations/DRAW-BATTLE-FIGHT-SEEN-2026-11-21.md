# Draw Battle: the fight on the board (2026-11-21)

RAN. Two real frames of the live window, pid 3432793.

The picture had been frozen while the ledger stepped. Teario's driver wakes the board by appending `126` to the session interact relay after it rewrites `events.txt` (`game_core.c` `engine_nudge`). A changed events file is drawn on the next frame, and nothing asks for that frame while the mouse is still. The fight now sends that same `126`.

Reset put the seed back: team 1 soldier at 2,7 and team 2 roman at 10,7. Two seconds later the events file read soldier 6,7 and roman 7,7. The later frame shows the roman alone on the grass and the bar says soldier dead at 6,7, roman up at 7,7, hp 36.

- `draw-battle-fight-2026-11-21.png` — the first dump of this run
- `draw-battle-fight-later-2026-11-21.png` — roman standing, soldier dead on the bar

Placed units are team 1. They only walk when a team 2 body is alive. Play is the switch. Stop freezes the steps.
