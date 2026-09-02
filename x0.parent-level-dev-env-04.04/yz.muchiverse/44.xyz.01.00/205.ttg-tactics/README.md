# 205.ttg-tactics — Community Tabletop Tactics

House **dual-render** tactics game (ASCII `current_frame.txt` + RGB + optional `gl_mirror`).
Not freeglut. Spec: [`DESIGN.md`](DESIGN.md). Handoff: [`../!.clone-clowning.md`](../!.clone-clowning.md).

## Run
```sh
sh button.sh compile
sh button.sh run          # dual view if muta system vendored
sh button.sh harness      # 01 move, 02 illegal, 03 regicide
sh button.sh kill
NO_GL=1 sh button.sh run
```

## Inject keys (same path as humans / AI)
```sh
printf '13\n' >> pieces/apps/player_app/history.txt   # Enter
printf '1001\n' >> pieces/apps/player_app/history.txt # arrow right
printf '97\n' >> pieces/apps/player_app/history.txt   # 'a' attack
printf '101\n' >> pieces/apps/player_app/history.txt  # 'e' end turn
```

## Status (PR0–2 slice)
- Title + 1v1 match, 12×12 army spawn, move, attack, regicide, pot ante files, ledger, dual compose, harnesses.
- Full clocks UI / Elo / farmer build / hot-seat polish: later PRs per DESIGN.md §16.
