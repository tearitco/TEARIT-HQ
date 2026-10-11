# eden-viewer-hq (first slice, read only)

A window that shows the current user's Eden game: day, running or finished, the farm as tiles (house, chickens, garden plots with state and growth), the people (hp, hunger, coin, grain) and a ticker of the last ten history rows. It never writes the game.

- Launch: `sh open_eden_viewer_hq.sh <house_root>` (shape of csv-lab-hq). Backend: `ops/eden_viewer_manager.c` (build with `ops/build_eden_viewer_manager.sh`), published file `eden_viewer_ui.txt`.
- Source: `<house>/<current_xyzfs>/home/livedesk/eden_game/game/conductor/` (`variables.txt` for day and running, `status.txt` for people and tiles, the tail of `eden_history.txt` for the ticker). `current_xyzfs` is read from the login file; `EDEN_GAME_DIR` overrides it for the harness. Harness hook: `eden_viewer_manager --dump <conductor_dir>`.
- Honest limits: people and tiles are as of the last Status row (press Status on the Eden button to refresh); the tiles are text cells, not sprites yet; no pause/resume rows, no click, no movement. Next slices: sprites per tile, move markers, Pause/Resume rows, the pc-hq mirror (see MACHINE-USERS-SCHOOLS-AND-FARM-ANIMATION-PLAN section 5).
