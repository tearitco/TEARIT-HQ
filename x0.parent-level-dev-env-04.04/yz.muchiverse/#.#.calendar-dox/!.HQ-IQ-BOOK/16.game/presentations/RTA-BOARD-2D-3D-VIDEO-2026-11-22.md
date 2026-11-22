# RTA board in 2D and 3D, the video (2026-11-22; D113; owner complaint: "none of the rta was in 2D/3D, so I couldn't verify it actually did anything")

Video: `XO/31.Nov/nov-22/15.rta-board-2d-3d-video-2026-11-22/rta_board_2d_3d.mp4` (126.9 s, 1280x890, English voice (en-US-AnaNeural via `pet_tts_local.py`, the one allowed Python runner), Japanese subtitles, Actor1 face left of the words) and nine stills (2D and 3D, before and after).

## What was built (D113)
- App `@.apps/rta-board` (window `board/open_board.sh` + `pchq-board.xhtpm` copied from Teario-Play, `button.sh`, data `board.pdl`, atlas `atlas.rgba`). Launch: `sh @.apps/rta-board/button.sh run` (through `cpu_gate`). Window title "Robot Trap Auto", 1074x556.
- Op `@.apps/rta/ops/rta_board_op.c` (`project <config>`): the pc-hq DESK is a PROJECTION of `#.desktop/rta_life_ledger.txt`, the rules and the road grid. It writes `cells.txt`, `cells.marker` (grows only when the cells change), `cells.rgba`, `map.txt`, `map.png`, `events.txt` (car row, then joe) and the panel feed `@.apps/rta-board/state/game_ui.txt`. Nothing is stored: in the car, joe is on the car cell; out of it, joe stands on the first free cell beside the car (`BESIDE` rows in `board.pdl`); the car is the last DRIVE row, else the first road cell of the south-most road row (the rule rta_momentum_op uses).
- `rta_life_op` runs the projection after every ACCEPTED verb when `#.desktop/rta_board.pdl` sits beside the ledger (opt-in marker file written by `open_board.sh`; no file, no picture, every harness unchanged; a refused verb does not touch the picture).
- City: `@.apps/rta/board_city.pdl` painted by the unchanged painter `rta_city_op` into `road.grid` (32x14, was 4x2): a harbour inlet crosses the main street, the `KIND | bridge | road | 9` deck spans the water (road row 11 is all road). Found by a run: the painter ends a feature at a polygon edge over 250 m or a line step over 80 m, and grass that reaches the border floods to harbour.
- Found live (not by a harness): the engine reads `events.txt` only BESIDE a `map.png` (without it the car and joe were invisible in 2D and 3D); the game popup overlay did not draw, so the same text (who, hunger, fuel, turn) is in the footer line of the window.

## What the video shows (real windows, real pages)
Blueprintz (1180x780, book rows rta_part, rta_enter, rta_drive x3, rta_exit, rta_sleep, rta_eat; Pause/Fire/Step ev driven with `relay_drive` taps) next to the board window (2D: key 0; 3D: key 0 again, camera mode 3). The two Blueprintz rows missing from its saved source list (rta_part, rta_sleep) were appended to `state/bp_sources.pdl` for the run and the state was restored from a tar (sha256 identical before and after).
Setup rows written by the op directly (not on screen): PART engine, tires, steering = ok. Final live ledger: PART tank ok 3, ENTER joe car, DRIVE x3 (cells 1, 2, 3; fuel 2, 1, 0), EXIT, SLEEP 3 (hunger 86), EAT (hunger 56), TURN 9.
Pictures (all opened):
- before enter, 2D: car (barrel sprite) at (0,11) on the road, joe at (0,10) on the grass; footer "on foot (car at 0,11) hunger 80 fuel 0 turn 0".
- after enter: joe on the car cell, footer "in the car ... fuel 3 turn 1" (the barrel is hidden under joe).
- after 3 drives: joe at (3,11) on the road, footer "car at 3,11 fuel 0 turn 4"; the same in 3D: the cyan car cube and joe three cells along the street.
- after exit: car stays at (3,11), joe at (3,10) on the grass, 2D and 3D; footer "on foot turn 5".
- after sleep 3: hunger 86; after eat: hunger 56, turn 9.
Audio: volumedetect per scene, mean -22.8 to -27.8 dB, max -3.2 to -5.9 dB (no silent scene).

## Harness
`rta_board` 42 (case `cases/rta_board.pdl`): projection after parts, enter, 3 drives, exit, sleep, eat equals the expected events.txt rows and panel text; refused verb leaves the picture byte for byte; replay twice and delete-and-regenerate give the same bytes (cells, events, panel); hand-edited events.txt is overwritten (no ghost data); no config = no desk; the bridge (car at x=20, 24, 28 over the inlet at drive_speed 4); NEGATIVES: wrong entity name never rides in the car, a moved road tile changes the cells, a config without keys refuses 3, the 8th drive refuses 13. Regression: rta_life 62, rta_pages 81 (two checks updated: the scratch house now moves the car on road row 11), rta_city 11, rta_car 9, rta_play 13, rta_momentum 6, bp_fp_state 13, mech_new 43, vox 361, all green.

## Not done
The car is the barrel piece (`!Other1`), not a car sprite; 3D is the top-down camera (mode 3) and the key `f` resets its pan; no walking on the board (`rta_walk`); the car cannot turn (rta_momentum moves +x only); the popup overlay does not draw (the text is in the footer); the window does not redraw by itself after a verb until one key reaches the engine (the driver sent `f`); `#.desktop/pchq_board_view.txt` (window size preference) was rewritten by the board window and could not be restored byte for byte (only its hash was taken before); the live binaries (`rta_life_op.+x`, `rta_board_op.+x`) are gitignored and were rebuilt in place.
