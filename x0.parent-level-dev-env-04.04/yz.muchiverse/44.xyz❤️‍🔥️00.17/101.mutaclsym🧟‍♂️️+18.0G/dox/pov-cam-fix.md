# POV Camera Fix — Reference Comparison & Root Cause

## Reference Engine: plugy3d-engine-2026_v19.0000

Path: `#.ref/plugy3d-engine-2026_v19.0000/`

Uses real OpenGL (PySide6 + GLU). Key properties:
- `gluPerspective(45, aspect, 0.1, 100.0)` for projection
- `glRotatef(pitch, 1, 0, 0)` then `glRotatef(yaw, 0, 1, 0)` for orbit camera
- Coordinate system: grid X → GL X, grid Y → GL Y (up), grid Z → GL Z (depth)
- **No negation hack** — OpenGL's standard right-handed coordinates match the grid directly

Reference camera modes:
| Mode | Name | Behavior |
|------|------|----------|
| 0 | Third Person | Behind + above target, `gluLookAt` |
| 1 | First Person | At eye height, looking forward |
| 2 | Free Camera | Orbit: pitch/yaw/zoom around target |

Reference defaults: `pitch=90.0` (looking at green/top face from above), `yaw=0.0` (looking toward +Z).

## Our Software Renderer: compose_rgb_frame.c

Custom projection in `project_3d()` (line 707):
```c
rx = wx - cam_x;
*out_x = screen_cx - (int)(rx * scale * persp);  // ← rx NEGATED
*out_y = screen_cy - (int)(y2 * scale * persp);  // ← y2 NEGATED
```

The **rx negation** was a deliberate fix for modes 1-3: the map's row-increases-DOWNWARD convention
creates a left-handed 2D plane. `yaw_rotate()` uses standard math convention (right-handed), so
without the negation, X appears mirrored vs ASCII. The negation corrected this for yaw-rotated views.

**But mode 4 (bird's eye) had yaw=0**, so no yaw rotation happened. The negation in project_3d
flipped BOTH axes relative to ASCII:
- X flipped: increasing col appeared LEFT instead of RIGHT
- Y flipped: increasing row appeared UP instead of DOWN

This was the root cause of mode 4 being "vertically and horizontally flipped from ASCII."

## The Fix (applied)

**Mode 4 yaw = 180°** (not 0°).

With yaw=180°, `yaw_rotate()` mirrors every tile coordinate around the camera center before
projection. The negation in `project_3d` then double-cancels:

```
screen_x = screen_cx - (-(col - cam)) * scale * persp = screen_cx + (col - cam) * scale * persp
```

- col > cam → screen_x > center → RIGHT ✓ (matches ASCII)
- row > cam → screen_y > center → DOWN ✓ (matches ASCII)

The ray marcher (`raymarch_walls_3d`) is also correct because its `dx_cam` computation is the
algebraic inverse of project_3d's formula, and the -180° yaw rotation on the direction vector
produces the matching negation.

## Mode 4: Bird's Eye / Board Game View

| Property | Value | Reason |
|----------|-------|--------|
| pitch | -90° | Camera looks straight down (-Y). cos(-90°)=0, sin(-90°)=-1 → y2=rz, z2=-ry. Floor at y=0, camera at y=12 → z2=12 (in front) |
| yaw | 180° | Mirrors world so project_3d's rx-negation produces correct ASCII-matching orientation |
| cam_y_off | 12.0 + cam_z_level * 2.0 | Height above the board |
| position | absolute (cam_pan_x, cam_pan_y) | Independent of hero position |
| render_radius | VIEW_3D_RADIUS * 2 | Wider view for board-game perspective |
| view direction | -Y (same as brown/bottom face in cube-legend) | Looking down at the board from above |

## Cube Legend Reference (^.cube-legend_v1.2.txt)

```
X-axis (red):   +X = right,   -X = left
Y-axis (green): +Y = up/top,  -Y = down/bottom (brown)
Z-axis (blue):  +Z = back (eyes/white),    -Z = front (dark)
```

Map API: `[z, y, x]` where z=layer(vertical), y=row, x=column.
Internal: `[x, y, z]` = `[column, row, layer]`.
3D rendering: col→X, height→Y, row→Z.

## What Changed

### compose_rgb_frame.c
1. **Line 1262**: `yaw = (camera_mode == 4) ? 180.0 : ...` (was 0.0)
2. **Case 4 block**: `yaw = 180.0;` (was 0.0) with explanatory comment
3. **pitch**: -90.0 (was 85.0 — nearly horizontal, broken)
4. **Debug cube**: 1×1×1 at (-0.5,-0.5,-0.5) with face colors per cube-legend, drawn as Pass 3 after wall raymarch

## Remaining Issues / Next Steps

1. **Verify live** — launch game, press '4', confirm bird's eye matches ASCII orientation
2. **Modes 1-3 may have related issues** — the same coordinate confusion could affect them.
   The reference uses `gluLookAt` with proper handedness; our renderer uses the yaw-rotate-then-negate
   approach. If modes 1-3 feel wrong, the fix pattern is the same: adjust yaw to cancel project_3d's
   negation for that specific view angle.
3. **Debug cube position** — with yaw=180°, the cube at (-0.5,-0.5,-0.5) gets mirrored to
   (0.5,-0.5,0.5) relative to camera. May need repositioning for useful axis verification.
4. **Mode 4 pan controls** — camera_control.c's w/a/s/d pan in mode 4. Need to verify these
   move the view correctly with the 180° yaw (pan directions may need inversion).
