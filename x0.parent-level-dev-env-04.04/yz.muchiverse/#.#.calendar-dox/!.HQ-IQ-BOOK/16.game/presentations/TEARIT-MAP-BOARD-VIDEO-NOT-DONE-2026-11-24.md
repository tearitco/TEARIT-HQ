# Tearit seeded map, the board video (not done, 2026-11-24)

The clip is not made. This file is the bar, so the next session does not invent a second one.

Follow `RTA-BOARD-2D-3D-VIDEO-2026-11-22.md`. A real board window, `dump_frame_png_op`, key 0 for 2D and 3D of the same desk. English voice `en-US-AnaNeural` through `pet_tts_local.py`. Japanese subtitles. Actor1 face left of the words. `volumedetect` mean and max on the scene, not silence. Open the frames.

What the picture has to show: the 16×16 seed from `tearit_map_op`, water and land, one mineral mark, and the line "Same seed, same land. The water stays water. One mineral is already in the ground."

Do not count `map.ppm` or `bv_gpu_desk_render` as this video. Do not open the live rpg-edit editor to take it. One board at a time. Do not change the camera.

A board left on "Map Loading..." has not been given a book. The grok note `2026-10-29-direction-3d-editor-and-test-loop.md` section 8 and pitfall `HOUSE_CODE_PITFALLS` (the centroid relaunch): write `CONFIRM_START_MAP:<book>` alone into `pieces/system/widget_cmds/inbox.txt`, wait until `pieces/world_01/state.txt` says `map_id=<book>` (up to 40 s), wait 6 s, then write `CONFIRM_SET_DESK:<page>`. Do not put both lines in one write. On 2026-11-24 the scratch board stayed on loading because those lines were never sent. After they were sent, `map_id=tearit-map-board`, `desk_id=board`, and the engine log said `Desk 'board' loaded.`

Where it will go when it exists: `XO/31.Nov/nov-24/grok-24/1.seed-map/`.
