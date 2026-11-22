# Teario: brick and coin placed in Make, played in Play with keys only (video proof, 2026-11-22)

Video: `Teario-brick-coin-make-to-play-en-voice-ja-subs.mp4` in XO `31.Nov/nov-22/10.teario-brick-coin-make-to-play-video-2026-11-22/` (67.5 s, 1280x890, h264 + aac, English voice, Japanese subtitles, narrator Actor1 left of the words). The photograph that proves the 2D fix is `2d-after-brick-broke-and-coin-taken.png` in the same folder.

## What you see (RAN, one continuous take, real windows)
Make window (stamp bank with pictures, game panel pinned right) -> the brick stamp placed above the start and the coin on the path (the game's own `ctl pick_*` and `ctl place` buttons, the cursor walked with arrow keys) -> `ctl play` (the Make `play` row, then the Play window opens on exactly that level within about 4 s) -> cursor to the player with arrow keys -> Enter (`poss People1` in 3D) -> key 0 (2D) -> ONLY keys: jump (32) then right (1001) -> the brick breaks, the coin is taken, the player reaches the flag.

## Evidence after the take
Ledger rows: `EVENT|327|bank|brick|p1|2,8|value=5;score=5` and `EVENT|356|bank|coin|p1|5,10|value=10;score=15`; `verify_clear` printed `CLEAR level=331e481a707bf86d;run=f02e2395e69647f1;ticks=422`; final panel `the game is over, winner Maker`, `p1 Maker (you): 17/19`.
**The 2D fix (D107) seen live:** in the final 2D photograph the cell at (2,8) is empty (the brick is gone) and so is the coin cell; before the fix the brick stayed drawn until the game ended.
Audio: every scene window of the mp4 has sound (volumedetect means between -22.7 and -26.8 dB; scenes 2 and 3 are short lines over a longer shot).

## How it was made (scratch scripts, not in the repo)
Same recipe as `TEARIO-PLAY-ARROWS-LIVE-VIDEO-2026-11-22.md`: a recorder dumping the Teario window (Make 1444x969 or Play 1074x556, found by size) through `dump_frame_png_op.+x` under `cpu_gate`, a controller writing key codes to the engine relay, scenes cut by epoch, voice from the house Kokoro runner `pet_tts_local.py`. Live Teario state was backed up before and restored after (sha256 of every file compared: 0 differences).

## NOT DONE
Japanese-voice + English-subtitle twin; a brick that holds a coin; teleport; the mid-air jump is covered by the ledger, not shown.
