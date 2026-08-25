# POV Camera System — Design Spec & Implementation Plan

## Core Separation of Concerns

**`move_player.c` is ONLY hero/cursor movement.** It handles:
- Arrow keys → move hero (when possessed) or xlector cursor (when interact_mode=1, unpossessed)
- `x` → move hero up one Z level
- `z` → move hero down one Z level
- Attack bumps (arrow into a monster tile)
- Transition checks (arrow into a transition tile)

That is its entire job. `move_player.c` does NOT handle camera controls.

**Camera controls are a separate concern.** The camera pan block currently sitting inside `move_player.c` (lines 467-511) is misplaced — it mixes hero movement state reads/writes with camera state reads/writes in the same short-lived process. Camera control keys (`w/a/s/d/q/e/r/t/c/v/f`) should be handled by a dedicated camera control op, or at minimum clearly separated from move_player.c's own logic.

### Hero movement vs camera — key distinction

| What | Keys | File | State it touches |
|------|------|------|-----------------|
| Hero/cursor movement | Arrow keys, x (Z up), z (Z down) | `move_player.c` | `pos_x`, `pos_y`, `map_id`, `facing`, `xlector_pos_x/y` |
| Camera control | w/a/s/d/q/e/r/t/c/v/f (mode-dependent) | **new camera op** (or choice.c) | `cam_pan_x/y/z`, `cam_yaw`, `cam_pitch`, `cam_z_level`, `camera_mode` |

The hero's Z level (`x`/`z` keys in move_player.c) and the camera's Z level (`c`/`v` keys in the camera op) are independent. The hero might be on Z level 2 while the camera hovers at Z level 5 looking down.

---

## Current State (bugs & gaps)

| Issue | Location | Detail |
|-------|----------|--------|
| Camera controls inside move_player.c | `move_player.c:467-511` | Camera pan block is in the wrong file — should be a separate op |
| Mode 4 (bird's eye) missing entirely | `compose_rgb_frame.c:1231-1235` | Only cases 1/2/3 exist in the switch; no `case 4` |
| Mode 4 POV hotkey missing | `choice.c:866` | `is_pov_key` only checks `'1'`/`'2'`/`'3'`, not `'4'` |
| Key bindings don't match spec | `move_player.c:480-510` | Camera pan uses `w/a/s/d/x/z`; spec calls for `w/a/s/d/q/e/r/t/c/v/f` |
| No rotation (yaw/look) from user input | `compose_rgb_frame.c` | `render_3d_view()` has no yaw/pitch adjustment — `facing` only tracks hero movement, not camera look |
| No "refocus to origin" | — | No `f` key handler exists for camera |
| No pitch/yaw state | `hero/state.txt` | No `cam_yaw` or `cam_pitch` fields |
| Hero Z level not in move_player | — | `x`/`z` for hero Z level not implemented (only camera Z exists today) |

---

## Target Behavior

### POV Modes (numeric hotkeys)

Keys `'1'`–`'4'` switch `camera_mode` in `hero/state.txt`. Only active when `render_mode=1` (3D GL). Handled by `choice.c` (same file that already handles the `'1'`/`'2'`/`'3'` switch today).

| Key | camera_mode | Description |
|-----|-------------|-------------|
| `1` | 1 | **First person** — camera at hero eye level, facing hero's direction |
| `2` | 2 | **Third person** — camera slightly above, behind, and tilted down toward hero |
| `3` | 3 | **Free roam camera** — detachable, player-controlled position + rotation |
| `4` | 4 | **Bird's eye / board game view** — camera high above, looking straight down |

### Hero Movement (move_player.c, unchanged scope)

| Key | Action | Condition |
|-----|--------|-----------|
| Arrow Up/Down/Left/Right | Move hero or xlector cursor | Always (panel open = no-op) |
| `x` | Hero Z level +1 | `render_mode=1` only |
| `z` | Hero Z level −1 | `render_mode=1` only |

**These are the ONLY keys move_player.c handles.** Nothing else.

### Camera Controls (separate op)

All keys below only apply when `render_mode=1` (3D). These are NOT in move_player.c.

#### Mode 1 — First Person

Camera locked to hero position + facing. Only look-direction keys are meaningful:

| Key | Action |
|-----|--------|
| `q` | Rotate camera left (yaw, 10° increments) |
| `e` | Rotate camera right (yaw, 10° increments) |
| `r` | Look up (pitch up, 10° increments) |
| `t` | Look down (pitch down, 10° increments) |
| `c` | Camera Z level +1 |
| `v` | Camera Z level −1 |
| `f` | Refocus — reset pitch/yaw to match hero facing |

`w/a/s/d` → no-ops (camera not free to move in 1st/3rd person).

#### Mode 2 — Third Person

Identical controls to Mode 1. Camera stays behind/above hero; `q/e/r/t/c/v/f` adjust the look angle. No positional freedom.

#### Mode 3 — Free Roam Camera

Full positional and rotational freedom. All keys active:

| Key | Action |
|-----|--------|
| `w` | Move camera forward (zoom in) |
| `s` | Move camera backward (zoom out) |
| `a` | Strafe camera left |
| `d` | Strafe camera right |
| `q` | Rotate camera left (yaw, 10° increments) |
| `e` | Rotate camera right (yaw, 10° increments) |
| `r` | Look up (pitch up, 10° increments) |
| `t` | Look down (pitch down, 10° increments) |
| `c` | Camera Z level +1 |
| `v` | Camera Z level −1 |
| `f` | Refocus — snap back to hero pos + default pitch/yaw |

#### Mode 4 — Bird's Eye / Board Game View

Camera high above, looking straight down. Positional panning only (rotation meaningless):

| Key | Action |
|-----|--------|
| `w` | Pan camera up (northward on map) |
| `s` | Pan camera down (southward on map) |
| `a` | Pan camera left (westward on map) |
| `d` | Pan camera right (eastward on map) |
| `c` | Camera Z level +1 (higher = see more) |
| `v` | Camera Z level −1 (lower = see less detail) |
| `f` | Refocus — center camera on hero's position |

`q/e/r/t` → no-ops (top-down has no rotation).

---

## State in `hero/state.txt`

### Existing fields (unchanged)

```
pos_x=18          # hero position (move_player.c)
pos_y=10          # hero position (move_player.c)
facing=1002       # last arrow direction (move_player.c)
camera_mode=2     # 1/2/3/4 (choice.c)
cam_pan_x=0.0     # camera position offset X (camera op)
cam_pan_y=0.0     # camera position offset Y (camera op)
cam_pan_z=0.0     # camera position offset Z (camera op)
```

### New fields to add

```
cam_yaw=0.0       # camera yaw in degrees (camera op)
cam_pitch=6.0     # camera pitch in degrees (camera op)
cam_z_level=0     # camera vertical Z level offset (camera op)
hero_z=0          # hero's own Z level (move_player.c, x/z keys)
```

- **cam_yaw**: user-adjustable via `q`/`e` in modes 1/2/3. Always 0 in mode 4.
- **cam_pitch**: user-adjustable via `r`/`t` in modes 1/2/3. Fixed at ~85° in mode 4.
- **cam_z_level**: camera height offset, `c`/`v` in all modes.
- **hero_z**: hero's own vertical level, `x`/`z` in move_player.c. Independent of camera.

---

## Implementation Plan

### Step 1: Create a new camera control op

**New file: `ops/camera_control.c`**

Same pattern as every other op: self-contained, no shared headers, one binary, reads/writes `hero/state.txt`.

Handles: `w/a/s/d/q/e/r/t/c/v/f` when `render_mode=1`.
Does NOT touch: `pos_x`, `pos_y`, `facing`, `xlector_pos_x/y`, `map_id` (move_player.c's job).

Reads from state.txt: `camera_mode`, `render_mode`, `facing`, `cam_pan_x/y/z`, `cam_yaw`, `cam_pitch`, `cam_z_level`, `pos_x`, `pos_y`.
Writes to state.txt: `cam_pan_x/y/z`, `cam_yaw`, `cam_pitch`, `cam_z_level` (and `camera_mode` if refocus resets it).

### Step 2: Remove camera pan block from move_player.c

**File: `ops/move_player.c:467-511`**

Delete the entire `if (render_mode == 1 && camera_mode == 3 && ...)` block. move_player.c should only handle arrow keys and `x`/`z` for hero Z level.

Add `x`/`z` hero Z level handling in move_player.c:
```
GATE: render_mode == 1 && (key == 'x' || key == 'z')
  x → hero_z += 1
  z → hero_z -= 1
  write hero_z back to state.txt
```

### Step 3: Wire camera_control.c into the PAL dispatch

**File: `pal/main_loop.pal`** (or equivalent dispatch script)

Add `camera_control x2` after `move_player x2` so both run on every tick. Both are self-filtering (move_player ignores non-arrows, camera_control ignores non-camera keys) — no conflict.

### Step 4: Add `cam_yaw`, `cam_pitch`, `cam_z_level`, `hero_z` to state.txt

Add the four new fields with defaults to `hero/state.txt`.

### Step 5: Expand `is_pov_key` to include `'4'`

**File: `ops/choice.c:866`**
```
// before:
int is_pov_key = (render_mode == 1 && (key == '1' || key == '2' || key == '3'));
// after:
int is_pov_key = (render_mode == 1 && (key == '1' || key == '2' || key == '3' || key == '4'));
```

choice.c must also read/write the new camera state fields so it doesn't clobber them on write-back.

### Step 6: Update `render_3d_view()` camera math

**File: `ops/compose_rgb_frame.c:1193-1305`**

#### New signature

```c
static void render_3d_view(unsigned char *fb, int fb_w, int fb_h,
    char grid[MAX_MAP_H][MAX_MAP_W + 1], int rows, int map_w,
    int px, int py, int camera_mode, int facing,
    double cam_pan_x, double cam_pan_y, double cam_pan_z,
    double cam_yaw, double cam_pitch, int cam_z_level);
```

#### Mode presets use new state

```c
switch (camera_mode) {
    case 1: /* first person */
        pitch = cam_pitch;
        cam_y_off = 0.9 + cam_z_level * 1.0;
        cam_z_off = 0.0;
        yaw = cam_yaw;
        break;
    case 2: /* third person */
        pitch = 10.0 + (cam_pitch - 6.0);
        cam_y_off = 1.6 + cam_z_level * 1.0;
        cam_z_off = 2.5;
        yaw = cam_yaw;
        break;
    case 3: /* free roam */
        pitch = cam_pitch;
        cam_y_off = 0.9 + cam_z_level * 1.0;
        cam_z_off = 0.0;
        yaw = cam_yaw;
        break;
    case 4: /* bird's eye */
        pitch = 85.0;
        cam_y_off = 12.0 + cam_z_level * 2.0;
        cam_z_off = 0.0;
        yaw = 0.0;
        /* absolute position, not hero-relative */
        cam_x = cam_pan_x;
        cam_z = cam_pan_y;
        break;
}
```

Mode 4 bypasses the hero-relative base_x/base_z + yaw_rotate() pipeline — camera at absolute map coordinates, looking straight down. Expand render radius for mode 4 (e.g. `VIEW_3D_RADIUS * 2`).

### Step 7: HUD footer update

**File: `ops/compose_rgb_frame.c` `build_action_footer()`**

Show camera key hints in the interact-mode footer based on `camera_mode`:

```
Mode 1/2: "[1-4] POV [q/e] Turn [r/t] Look [c/v] Z [f] Reset"
Mode 3:   "[1-4] POV [wasd] Move [q/e] Turn [r/t] Look [c/v] Z [f] Reset"
Mode 4:   "[1-4] POV [wasd] Pan [c/v] Z [f] Center"
```

---

## Data Flow

```
keyboard_input.c
  → writes key to history.txt
  → prisc+x dispatches to (all in parallel, all self-filtering):
      move_player.c    — arrows only (hero/cursor movement, x/z for hero Z level)
      choice.c         — digits, Enter, Escape, 'i', 'e', '0', '1'-'4' (mode switching)
      camera_control.c — w/a/s/d/q/e/r/t/c/v/f (camera movement/rotation, render_mode=1 only)
  → orchestrator triggers compose_rgb_frame.c
      → reads ALL state from hero/state.txt
      → render_3d_view() applies mode preset + user camera adjustments
      → writes rgb_frame.raw
  → gl_mirror.c blits rgb_frame.raw to GL window
```

No shared headers. No in-memory state passing. Every process is short-lived, reads state from disk, writes result to disk. This is the governing constraint.

---

## Files to Modify / Create

| File | Changes |
|------|---------|
| **`ops/camera_control.c`** | **NEW** — dedicated camera control op for w/a/s/d/q/e/r/t/c/v/f |
| `ops/move_player.c` | Remove camera pan block (lines 467-511); add hero Z level via x/z |
| `ops/choice.c` | Add `'4'` to `is_pov_key`; read/write new camera state fields |
| `ops/compose_rgb_frame.c` | Add `case 4` to camera switch; accept cam_yaw/cam_pitch/cam_z_level in render_3d_view(); mode-dependent radius; update footer |
| `pal/main_loop.pal` | Wire `camera_control x2` into dispatch |
| `hero/state.txt` | Add `cam_yaw=0.0`, `cam_pitch=6.0`, `cam_z_level=0`, `hero_z=0` |
