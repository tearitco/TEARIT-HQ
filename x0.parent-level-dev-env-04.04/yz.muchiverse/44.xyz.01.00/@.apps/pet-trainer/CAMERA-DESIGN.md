# Pet trainer: pc-hq camera and POV in INT mode (design, written 2026-10-09, NOT built)

Owner direction: "when in int mode pc-hq camera controls (pov, camera movement etc) should be present here, that's the biggest change we're gonna need."

## What exists today
- INT mode forwards keys to `state/interact_relay.txt` (`<code> <ms>`); `ops/pet_manager.c` `relay_poll` reads them.
- `camera_apply` already stores the pc-hq key set into `state/camera.st`: `mode` (2d/3d), `pov` 1-4, `yaw`, `pitch`, `height`. Codes come from `keybinds.pdl` (same names/codes as piececraft-hq).
- **Nothing reads `camera.st` to draw.** `ops/pet_scene.c` draws flat 2D scenes only. So the keys change a file and the picture never changes. This is the gap.

## Goal
In INT mode on any page (room, world, later the home map) the pc-hq camera keys visibly work:

| key | action | pc-hq name |
|---|---|---|
| arrows | walk (trainer in world, pet in room) | arrow_* |
| 0 | 2D <-> 3D | render_mode_toggle |
| 1 / 2 / 3 / 4 | POV: first person / third person / free roam / bird's eye | pov_mode_* |
| q / e | yaw left / right | yaw_* |
| r / t | pitch down / up | pitch_* |
| c / v | camera height down / up | cam_height_* |
| f | reset view | reset_view |

Control mapping follows the player's perspective (left key = left on screen), per the camera-control-intuition lesson. Esc leaves INT mode.

## Approach (reuse, do not invent)
1. **Scenes become data.** Room, living room, garden, village are described as boxes/tiles/sprites in a scene file (the pc-hq board model: pieces on a grid, `pieces/<id>`), not hard-coded `rect()` calls. `rooms.pdl` already holds doors and platforms; add furniture rows.
2. **2D = the current look**, drawn from that data (top-down for the village, side view for rooms).
3. **3D = the existing board-viewer renderer.** `&.widgits/board-viewer/ops/bv_render_3d.c` and the GPU daemon `bv_gpu_raymarch` already implement POV 1-4, yaw, pitch and height and are what pc-hq uses. The pet window should call it (op + file IPC, no header/link sharing) with the scene's board plus `camera.st`, and show the frame the way pc-hq does (`canvas_raw`).
4. Same window and same `camera.st` keys; `pet_manager` keeps `camera_apply`, and adds "render with the camera": 2D scene when `mode=2d`, 3D op when `mode=3d`.
5. The village becomes a real pc-hq board (book `pet_village`), which also delivers the RPG Maker tiles + sprite animation item.

## Open questions for the owner
- Rooms in 3D: a box with walls (like a doll house, free-roam camera) or a side-on diorama?
- Should the 3D pass be on by default or only after pressing 0 (recommended: 2D default, 0 to switch)?
- CPU: the 3D daemon is heavy on this machine (see board-viewer perf notes); run it only while mode=3d and INT is on, and `nice`d.

## Build order
1. Scene data format + 2D drawn from it (no visible change; check by PNG diff).
2. Board export so board-viewer can read a pet room/village.
3. 3D op call + camera.st wiring, POV 1-4, yaw/pitch/height.
4. Prove each key through the relay with before/after PNGs; CPU measured idle and in 3D.
5. Only then: platforms (jump on bed/desk), more rooms, borough map.

## Not covered here
Mic/STT, chat Enter bug, neighbours/borough: separate items.
