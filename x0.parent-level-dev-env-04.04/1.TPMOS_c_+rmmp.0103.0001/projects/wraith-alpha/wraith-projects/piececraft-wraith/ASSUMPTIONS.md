# Piececraft Wraith Assumptions

Reference:
- `projects/piececraft-3d`

Assumptions being tested:
- Wraith should reuse Piececraft's map/tile files, not invent a separate voxel-first world format.
- `${game_map}` is the payload surface.
- `Control_Map` is a project UI control with `target_surface=game_map`.
- camera state uses the Piececraft/GL-OS fields: `camera_mode`, `cam_x`, `cam_y`, `cam_z`, `cam_pitch`, `cam_yaw`, `cam_roll`.
- tile height comes from `assets/tiles/*.tile.txt` `extrude=`.

Current limits:
- RGB presenter still draws a preview/pseudo-height map, not true Piececraft camera geometry.
- no project-owned input op exists yet for Piececraft Wraith camera/map controls.

Next correction target:
- add `ops/+x/wraith_project_input.+x` for Piececraft-style `CAMERA_MOVE`, `CAMERA_MODE`, selected-cell movement, and map edit commands.
