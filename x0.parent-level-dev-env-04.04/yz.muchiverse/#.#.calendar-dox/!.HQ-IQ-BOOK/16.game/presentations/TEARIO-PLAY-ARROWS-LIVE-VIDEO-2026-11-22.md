# Teario-Play: the arrow keys move People1 and clear the level (video proof, 2026-11-22)

The video is `Teario-play-arrows-live-en-voice-ja-subs.mp4` in the owner's XO folder `31.Nov/nov-22/7.teario-play-live-video-en-voice-ja-subs/` (media stays out of the repo, per the presentations standard). 67 s, 1280x852, h264 + aac, English voice with Japanese subtitles (rule 3b), the narrator face Actor1 left of the words (rule 3c), the whole window in frame.

Labels: **RAN** = I ran it and saw the output. **NOT DONE** = a gap, said plainly.

## What the footage is (RAN, one continuous take, real window, no editing of the game)
A fresh game in the live Teario-Play window (`sh @.apps/teario-play/button.sh run`), recorded at about 4 frames a second with `dump_frame_png_op.+x` through `cpu_gate`, played ONLY through the engine's key relay (the same path a keyboard takes), nothing through the game's buttons except START.

| Scene | What you see | Real time |
|---|---|---|
| 1 | setup pop-up, `you play: p1`, 3D view | 5 s |
| 2 | START GAME: the game ticks by itself (`GAME | own_run | 1`) | 4 s |
| 3 | the cursor walks to the player with arrow keys (sped 2.5x) | 16 s |
| 4 | Enter: the HUD reads `poss People1` | 3 s |
| 5 and 6 | key 0 gives the flat 2D view; ONE right-arrow press: the sprite runs, three jump presses clear the gap and stomp two monsters | 25 s (the second half sped 1.5x) |
| 7 | pop-up: `the game is over, winner A`, `tick 543 OVER` | 3 s |
| 8 | the same finished game in 3D | 4.5 s |
| 9 | still of the Make window: the stamp bank still shows names only (NOT DONE, said in the voice) | 7 s |

## The ledger behind it (RAN, read after the take)
`new`, `join A human`, `start`, then only FOUR key rows: `299 input right`, `339 input jump`, `422 input jump`, `491 input jump`, and the end checkpoint `543 ... hash=be2f4706a95f030f;over`. `game_core replay` printed `REPLAY events=9 hash=be2f4706a95f030f` (equal to the checkpoint) and `check` printed `CHECK ok events=9`. `verify_clear` printed `CLEAR level=97d185088521224b;run=a5877010873fd4aa;ticks=543`. Player row: `pos=18 grounded=1 falls=0 score=200 stomps=2 hits=2`.

## How it was made (scratch scripts, not in the repo; a C op is the right home if this repeats)
1. `rec.sh`: loop `dump_frame_png_op.+x <window> NNNNN.png`, appending `n epoch_ms game_tick` to a log. 2. `drive.sh`: writes decimal key codes into the session relay (1000 to 1003 arrows, 13 Enter, 48 for `0`, 32 jump), marks each scene start, and presses jump by reading the desk rows (`events.txt`) of People1 and the monsters. 3. `build.sh`: cuts each scene by epoch, holds a scene as long as its narration plus 0.6 s, burns the Japanese subtitle with Noto Sans CJK on the Tear-it strip, overlays the Actor1 face, muxes the voice, concatenates. 4. Voice: the house Kokoro runner `@.apps/pet-trainer/ops/pet_tts_local.py --voice en-US-AnaNeural` (an existing model runner, I wrote no Python), one clip at a time niced; checked with `volumedetect`: each clip about -22.5 dB and each scene window of the final mp4 between -22.9 and -25.6 dB (no silent scene).

## Traps seen
- `COPYDIR` skip lists did not skip; the game was still "in progress" from earlier runs: archive it (`game_core ctl new_game <games dir>`), then press `human_next` until the label says `you play: p1`, then START.
- The popup covers the first columns of the level: scenes 5 and 6 are shot when the sprite has run past it. NOT FIXED.
- Only the 3D view shows the HUD (`poss People1`); the 2D view has none. Scene 4 is therefore shot in 3D, then key 0.
- Two Teario windows at once share one 3D daemon: one window at a time.

## NOT DONE
The Japanese-voice + English-subtitle twin (no Japanese Kokoro voice tried); a take of Make's own Play button (it does not open this window yet); stamp bank example pictures; the jump in mid-air is in the take but the popup hides the first columns.

Related: `TEARIO-HANDOFF-WHERE-WE-ARE-AND-WHAT-IS-NEXT-2026-11-22.md` section 11, decision D101, XO `nov-22/6.teario-play-live-proof-2026-11-22/` (photographs and page).
