# Voxel (Minecraft) board in 2D and 3D, the video (2026-11-22; D114; handoff section 19)

Video: `XO/31.Nov/nov-22/16.minecraft-board-2d-3d-video-2026-11-22/minecraft_board_2d_3d.mp4` (119.5 s, 1600x890, English voice en-US-AnaNeural via `pet_tts_local.py`, Japanese subtitles, Actor1 face left of the words) and 22 stills in `stills/`. Built from REAL photographs of the two live windows (Voxel 1074x556, Blueprintz 1180x780, taken with `dump_frame_png_op`, each opened), composed into 10 narrated scenes (stills held while the voice speaks, not a screen recording: stated plainly).

## What it shows
Blueprintz book `vox` (six pages: eat, mine, move, place, sleep, wait) with the command row and ALL fields (mine: actor, tool, cell), Fire then Step ev, and the board BEFORE / AFTER in 3D and 2D: Mine x2 (pick on dirt is two swings: pit), Move (4,3,3), Place (cobble at 3,3,4: grey cube in 3D, grey tile in 2D), the popup buttons Up, Place, Up, Place (a two-block tower, Joe on top), Wait (hunger 9 to 10), Eat (hunger 10 to 1), Sleep (refused at noon, picture unchanged), the Wait and Eat buttons (turn 12, 13, bread 1). Final ledger: `stills/ledger_final.txt`.
Audio: volumedetect per scene, mean -22.5 to -23.1 dB, max -0.7 to -6.3 dB (no silent scene). Frames opened: 5 s, 25 s, 56 s, 60 s and the stills.

## Not shown / limits
Pit in 2D only when dug to stone (same dirt tile); the move step has no 2D photo; no bed so no successful sleep; the camera was set with keys (`f3aa`), measured per launch. Live state: Blueprintz state tar + sha256 restored identical (14 files); `pchq_board_view.txt` hash unchanged (dfaff441...); the live ledger `#.desktop/vox_ledger.txt` did not exist before and was left as the proof ledger (copy in the stills folder; delete to reset); `bv_render_3d.+x` rebuilt in place (old binary copy hash 17a4fb89...); `#.desktop/vox_board.pdl` left in place (gitignored).
