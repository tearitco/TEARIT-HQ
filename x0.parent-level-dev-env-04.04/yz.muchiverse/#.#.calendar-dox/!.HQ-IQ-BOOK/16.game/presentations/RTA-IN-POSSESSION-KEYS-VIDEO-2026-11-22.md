# Robot Trap Auto (RTA board) played by KEYS in possession: video write-up (2026-11-22, D116)

## What was recorded (screen recording, 7 scenes, same pipeline as the Voxel video)
Live window `@.apps/rta-board/board/open_board.sh` through `cpu_gate`; English voice + Japanese subtitles; XO `18.rta-in-possession-keys-video-2026-11-22/rta_in_possession_keys.mp4` (+ `snap.txt`, `marks.txt`).

## Setup versus keys
- SETUP: window opened by the launcher; four `rta_life_op part` rows (engine, tank 3 fuel, tires, steering) so the car can drive.
- KEYS (session key relay, decimal codes): `57` release, `13` Enter on Joe's token (cursor already on his cell; HUD `poss People1`), `105` (i) enter car, `1001` (right arrow) drive x3 (car 0,11 -> 3,11, fuel 3 -> 0), `111` (o) exit, `107` (k) eat (hunger 80 -> 50), `110` (n) sleep 8 turns (turn 6 -> 14, hunger 66), `48` (0) 2D and back, `50` (2) third-person 3D. `snap.txt` holds the ledger tails: ENTER, DRIVE x3, EXIT, EAT, SLEEP rows, each from one key.

## Design (D116)
The same tailer as the Voxel window (D115): `vox_keys_op` now accepts a KEYMAP row with a fifth field, a command (`$D` = house root, words split on spaces, no shell), instead of `vox_op act`. `@.apps/rta-board/keys.pdl` names the `rta_life_op` commands; every rule stays in `rta_life_op`; the board re-projects through the existing `rta_board.pdl` hook. Harness `rta_keys` (14): a drive before enter is refused and writes nothing, the five keys write ENTER/DRIVE/EXIT/EAT/SLEEP rows, an unmapped key is counted, sabotage (command field ignored) applies nothing, source cmp'd.

## What cannot be done by keys, and why
- Arrow keys cannot walk Joe: the ledger has no walk row for him (`rta_walk_op` is a separate Centon op with its own ledger; `rta_board_op` derives Joe's cell from the car). Adding one is a new rule (an op plus rows) and was not done.
- The key legend (`LEGEND` row of keys.pdl) is passed to the feed, but this window shows only the footer line; the keys are listed in the narration instead.

## State restore
The RTA ledgers and `rta_board.pdl` did not exist before the run; they were removed after it. `pchq_board_view.txt` unchanged (dfaff441...). Windows closed by their own `board_window.pid`.
