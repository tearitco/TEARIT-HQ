# Wraith Ed

`wraith-ed` is the internal Wraith game-editor probe.

Purpose:
- pull `op-ed` editor concepts into Wraith without hardcoding them in Wraith Alpha.
- use `piececraft-wraith` / `piececraft-3d` map conventions for `${game_map}`.
- exercise heavier project-owned functionality: map loading, glyph placement, game save loading, PAL event creation, and export receipts.

Current first pass:
- `session/wraith_body.txt` is the ASCII audit body.
- `session/scene.objects.pdl` exports editor panels, project-local controls, and a `${game_map}` tile map model.
- `ops/+x/wraith_project_input.+x` consumes `session/history.txt` while `is_map_control=1`.
- WASD moves the editor cursor.
- `P` or Space places the active glyph.
- `G` cycles the active glyph.
- `L` reloads the active map from `games/test-game-01/maps/map_0001_z0.txt`.
- `S` writes a save snapshot under `games/test-game-01/saves/`.
- `E` writes a PAL event draft under `pal/events/`.
- `X` writes an export manifest under `exports/`.
- `ESC` is handled by Wraith Alpha and exits map/interact mode.

Reference standards:
- `projects/op-ed`: sovereign game folders, map/script/save layout, PAL event-builder direction.
- `projects/piececraft-3d`: tile registry, map z-levels, camera/map editing semantics.
- `projects/wraith-alpha/wraith-projects/chtmgl-wraith`: widget/panel/control scene roles.

Do not move editor behavior into `wraith-alpha_manager.c`.
