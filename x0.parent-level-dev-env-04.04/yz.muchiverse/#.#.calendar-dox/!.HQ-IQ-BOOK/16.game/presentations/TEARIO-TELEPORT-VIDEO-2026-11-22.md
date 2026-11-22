# Teario: a teleport pair placed in Make, used in Play with one key, and its Blueprintz page (video proof, 2026-11-22; D111)

Video: `Teario-teleport-make-to-play-en-voice-ja-subs.mp4` in XO `31.Nov/nov-22/14.teario-teleport-video-2026-11-22/` (88.8 s, 1280x890, h264 + aac, English voice, Japanese subtitles, Actor1 left of the words, whole window in frame). Photographs in the same folder: `2d-before-teleport-tick334-player-near-start.png`, `2d-after-teleport-tick340-player-at-exit.png` (the 2D picture after the teleport), `make-teleport-pair-placed-stamp-bank.png`, `make-stamp-bank-with-teleport.png`, `blueprintz-tm_teleport-page-step-ev-ran.png`.

## What you see (RAN, real windows)
1. Make: the stamp bank has a ninth stamp, teleport, with a picture. 2. The teleport stamp is picked and placed at (5,10) with the game's own `ctl pick_tp` / `ctl place`, the cursor walked with arrow keys. 3. The second tile at (11,10); panel `tp at 11,10, 17 edits`. 4. `ctl play`: the Play window opens on that level. 5. Cursor walked to the player with arrow keys; Enter possesses People1; key 0 (2D). 6. ONE key, right (1001): the player is moved from the first tile to the second (frame at tick 334 near the start, tick 340 at the exit; the panel tick lags the picture by about two ticks). The video plays this part at 0.45 speed. 7. Blueprintz: the `tm_teleport` page of the teario book.

## Evidence after the take
Ledger: `EVENT|325|p1|input|right|2|ok`, `EVENT|342|bank|teleport|p1|5,10>11,10|fee=0;score=0`; `verify_clear` printed `CLEAR level=67d3b8559eb142ad;run=5c56156626b9f0bf;ticks=372`: 372 - 325 = 47 ticks, the number the harness case `teario_teleport` pins for the same level (76 ticks without the two tiles). Final panel: `the game is over, winner Maker`, `p1 Maker (you): 17/19`.
Audio: volumedetect mean per scene between -22.9 and -26.3 dB (10 scenes, all with sound).

## Blueprintz scene (REAL window, 1180x780, `sh &.hq-apps/blueprintz/button.sh run`)
Method of `RTA-BLUEPRINTZ-SLEEP-PART-VIDEO-2026-11-22.md`: `relay_drive` gentle mode, `tapid` taps. Shown: the event list of the teario book with `tm_teleport` selected, page `page_1  trigger: ui`, command row `1 rule_teleport  scene=tm_teleport/scene.pdl`, the field `scene = tm_teleport/scene.pdl` (the only field of this command), then Pause, Fire, Step ev; the list row changes `waiting` -> `queued` -> `ran` and the status line reads `step-event: stepped event .../tm_teleport`.
**Limit, said exactly:** the Games dropdown lists only 24 rows (21 games, 3 books) and cuts off `teario (book)` and `rta (book)`; keys Down x24 could not reach them. The book was therefore loaded with the SAME queue command the menu row sends (`LOADBOOK:teario` appended to `state/request.txt`), not by a click on the row; every later step is a real tap. The window showed `loaded 13 event(s) of teario`. A first take tapped while the dropdown was still open (it selected tm_brick, nothing was fired); it was discarded and Esc closed the menu once.
The rule's own output (`TELEPORTED 1 SCORE 0 FROM 1,1 TO 4,1 HERO 1052,312 ...`) is NOT drawn by Blueprintz; it is in the harness case `rule_teleport`.

## Safety and restore
Teario state (`teario-make/state`, `teario-play/state`, 76 files) was backed up with tar before and restored after: sha256 of every file compared, 0 differences. Blueprintz `state/` and `cli_io_state.txt` (14 files) restored the same way, 0 differences; the `master_ledger.txt` the run left in the event folder was removed. Only processes this session started were ended (Play window and driver, two engines, the Blueprintz renderer). The Make window had already closed itself when Play opened.

## How it was made (scratch, not in the repo)
Same recipe as `TEARIO-BRICK-COIN-MAKE-TO-PLAY-VIDEO-2026-11-22.md`: recorder dumping the window with `dump_frame_png_op` under `cpu_gate` (about 5 frames a second), key codes appended to the session relay, scenes cut by epoch marks, voice from `pet_tts_local.py` (en-US-AnaNeural), ffmpeg build. One subtitle was corrected after viewing a frame (scene 3, "further on").

## NOT DONE
Japanese-voice twin; explicit destination rows; enemies using teleports.
