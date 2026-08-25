# Emoji Studio Wraith Assumptions

Reference:
- `projects/emoji-studio/session/test_emoji.csv`
- `projects/emoji-studio/ops/emoji-xtract.c`

Assumptions being tested:
- RGBA CSV is a separate 2D-to-3D source path from Piececraft maps and cube z-slices.
- alpha controls occupancy/visibility.
- RGB controls material color.
- extrusion belongs only inside `${game_map}` payload surfaces.
- resolution controls how many CSV rows form the square image grid.

Current limits:
- RGB presenter draws a flat extruded preview, not depth-sorted 3D columns through camera state.
- resolution picker is static; no project-owned input op changes `emoji_resolution` yet.

Next correction target:
- add `ops/+x/wraith_project_input.+x` for resolution switching and camera controls, then emit scene labels with updated source/resolution.
