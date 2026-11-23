# In: ON + possession + keys, three screen-recorded proof videos (2026-11-22, follow-up to D117/D118)

XO: `31.Nov/nov-22/19.in-on-possession-proof-videos-2026-11-22/` (`vox_in_possession_proof.mp4` 90 s, `rta_in_possession_proof.mp4` 118 s, `teario_play_in_possession_proof.mp4` 80 s, four stills each, `*_snap.txt` ledger/xelector reads, `*_marks.txt` epoch marks). English voice (en-US-AnaNeural) + Japanese subtitles, Actor1 face left of the words, whole window in frame. Screen recordings: `dump_frame_png_op` loop (~2.5 frames/s, epoch log), cut by marks. Audio `volumedetect` mean/max: Voxel -23.6/-2.1 dB, RTA -24.6/-2.9 dB, Teario-Play -23.7/-0.9 dB (audible, not clipped).

## Human path used in all three (new launch each)
1. SETUP: launch via `cpu_gate`. 2. `relay_drive <window pid> click 60 48` on `In:`: label read `In: ON` in the opened frame. In Voxel and RTA the click possesses `hero_01` (HUD `poss hero_01`), so ONE `relay_drive key esc` releases it (In stays ON; `possessed_id=none`). In Teario-Play after a fresh game the click left `poss -`, no release needed. 3. KEYS (session relay decimal codes): arrows move the cursor onto the token, Enter (`poss People1` in the HUD). 4. KEYS only after that. 5. Esc once: `poss -`, `In: ON`; Esc again: `In: off`.

## Voxel (setup: launch only)
Frames opened: `In: off`, `pos 5,6,4 poss -` (before) ; `In: ON pos 5,6,4 poss hero_01`; `In: ON pos 4,3,4 poss People1`; walk `pos 6,3,4`; tower `pos 5,4,4`; 2D (key 0) green outline on the Joe sprite; end `In: off poss -`. Keys: right, right, down (MOVE row), `l` twice (MINE 6,2,4 dirt, DROP), left, `p` (PLACE cobble 5,3,4), `u` (MOVE z 6), `p` (PLACE 5,4,4), `k` (EAT bread), `0` 2D, `0` 3D. The cursor (xelector `pos_x,pos_y` and the HUD `pos`) followed Joe: 4,3 then 6,4 then 5,4 (`vox_snap.txt`).

## RTA board (setup: launch + four `rta_life_op part` rows: engine, tank 3 fuel, tires, steering, so the car can drive)
Frames opened: `In: off pos 16,7,1`; `In: ON pos 1,8,1 poss -` (cursor on its way); `In: ON pos 3,11,1 poss People1` (car at 3,11); `In: ON pos 3,10,1` (Joe on foot after exit/eat/sleep, turn 14, hunger 66); 2D with the green outline on Joe. Keys: `i` (ENTER), right arrow x3 (DRIVE 1,11 / 2,11 / 3,11, fuel 3 to 0; the car visibly moves along the road), `o` (EXIT), `k` (EAT 80 to 50), `n` (SLEEP 8, turn 14), `0`, `0`, `2` (3D). The cursor followed with a lag of about one step (xelector read 0,11 after the first drive, 1,11 after the second, 2,11 after the third, 3,11 after exit) and settled on the car cell; it is NOT instantaneous (D118 reads the desk row, which the board rewrites after the key).

## Teario-Play (setup: launch, `game_core ctl new_game`, `human_next` until `you play: p1`, `ctl start`; the Make-play handoff was not used)
Frames opened: setup pop-up with `In: off` (`pos 2,10,1`); `In: ON pos 1,10,1`; `poss People1 pos 0,10,1` (after left); 2D with the green outline on the sprite and monsters; end `In: off pos 4,10,1 poss -`. Keys: left x3, space (jump: `pos 0,9`), right, left, space, left x2, `0`, left x2, `0`, right x2; 13 `input` rows in the ledger (`snap`). Cursor followed: 1,10 to 0,10 to 0,7/0,9 (jump) to 2,10 to 4,10.

## Esc
All three: Esc 1 gave `possessed_id=none` with `In: ON`, Esc 2 gave `In: off` (snap files).

## Bugs / honest notes
- Not a bug but a trap: in Voxel/RTA the `In:` click possesses the hero first, so Enter on Joe needs the Esc release first. My first Voxel take skipped that and walked the hero, not Joe (ledger unchanged; discarded). In Teario-Play an Esc after the click (when nothing was possessed) turned `In:` off; discarded.
- Teario-Play: an earlier take started a game with `you play: none` (one `human_next` too many), so People1 was not seat-driven; discarded.
- Cursor follow in RTA lags one step; not a hang.
- 2.5 fps recordings look choppy; scenes whose action is shorter than the narration hold the last frame.
- NOT done: Teario-Play through Make's play handoff (not used), a level cleared in Teario-Play, Blueprintz scenes (not applicable to a key-play proof).

## State
Backed up before the first launch (tar + sha256 of state dirs, xelector pieces, `vox_ledger.txt`, `vox_board.pdl`, `pchq_board_view.txt`); restored after: all `sha256sum -c` OK, `pchq_board_view.txt` dfaff441... unchanged; `rta_life_ledger.txt` and `rta_board.pdl` (absent before) removed. Windows were closed by killing only the pids from each app's `board_window.pid`.
