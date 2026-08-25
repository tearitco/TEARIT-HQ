# Legend Cube + Player POV Debug Reference

## World Coordinate System

The world is a 2D tile grid (col, row) with height as a 3rd axis.

```
     X+ (red)     col increases →  RIGHT in 2D
     X- (blue)    col decreases ←  LEFT in 2D
     Y+ (green)   height ↑         UP (not visible in 2D ASCII)
     Y- (brown)   height ↓         DOWN (not visible in 2D ASCII)
     Z- (dark)    row decreases ↑  UP/NORTH in 2D
     Z+ (white)   row increases ↓  DOWN/SOUTH in 2D
```

**Z axis is row-increases-DOWNWARD** (left-handed 2D plane). This is
the root cause of the `project_3d()` rx-negation fix (line 734) - the
standard yaw_rotate() math assumes right-handed conventions.

## Debug Cube

Position: world (-0.5, -0.5, -0.5) to (0, 0, 0)
Pivot: hero's (col, row) position (modes 1/2/3) or cam_pan_x/y (mode 4)

Cube faces (after yaw rotation):
| Face | Color | Meaning |
|------|-------|---------|
| Y+ top | green (0,180,0) | UP |
| Y- bottom | brown (139,90,43) | DOWN |
| Z+ back | white (240,240,240) | row+ (SOUTH in 2D) |
| Z- front | dark gray (30,30,30) | row- (NORTH in 2D) |
| X+ right | red (200,0,0) | col+ (EAST in 2D) |
| X- left | blue (0,0,200) | col- (WEST in 2D) |

## Camera Modes - Controls

### Mode 1: First Person (key `1`)
- **Camera LOCKED** at hero eye level (cam_y=0.9, cam_z_off=0)
- **No panning** - camera stays at hero position
- **No Z level change** - c/v are disabled
- Look controls only:
  - `q` / `e` — rotate yaw left/right
  - `r` / `t` — pitch up/down
  - `f` — reset to default (yaw=0°, pitch=6°, looking south)

### Mode 2: Third Person (key `2`)
- **Camera LOCKED** behind+above hero (cam_y=1.6, cam_z_off=2.5)
- Camera sits 2.5 units behind hero, slightly elevated, looking down
- **No panning** - camera stays behind hero
- **No Z level change** - c/v are disabled
- Look controls only:
  - `q` / `e` — rotate yaw left/right
  - `r` / `t` — pitch up/down
  - `f` — reset to default (yaw=0°, pitch=6°, looking south-down)

### Mode 3: Free Roam (key `3`)
- **Camera free** - starts at bird's eye, then fully user-controlled
- All controls:
  - `w` / `a` / `s` / `d` — pan camera
  - `q` / `e` — rotate yaw left/right
  - `r` / `t` — pitch up/down
  - `c` / `v` — Z level up/down
  - `f` — reset to bird's eye (yaw=180°, pitch=-90°, pan=0,0)

### Mode 4: Bird's Eye (key `4`)
- **Top-down view** looking straight down
- `w` / `a` / `s` / `d` — pan camera on map
- `c` / `v` — Z level up/down
- `f` — center on hero position
- q/e/r/t are no-ops

## facing_to_yaw() Mapping

Arrow keys → yaw degrees (for modes 1/2 default orientation):

```
ARROW_UP    (1002) → 180°   looks toward row- (NORTH in 2D)
ARROW_DOWN  (1003) →   0°   looks toward row+ (SOUTH in 2D)
ARROW_LEFT  (1000) →  90°   looks toward col- (WEST in 2D)
ARROW_RIGHT (1001) → 270°   looks toward col+ (EAST in 2D)
```

**Note:** These were originally inverted (UP=0°, DOWN=180°) which made
the camera face the hero's BACK. Fixed live - see compose_rgb_frame.c
line 759-772 for the full writeup.

## hero/state.txt Defaults

```
facing=1001        (ARROW_RIGHT → yaw=270°, looking EAST in 2D)
cam_yaw=180.00      (overridden by q/e keys in modes 1-3)
cam_pitch=6.00     (overridden by r/t keys in modes 1-3)
```

## Mode 1 (First Person) Default View

- Camera at hero eye level: cam_y=0.9, cam_z_off=0
- **yaw = cam_yaw** (starts at 180° if untouched, or whatever q/e set)
- With default cam_yaw=180°, camera looks NORTH in 2D (toward smaller rows, Z- dark face)
- The debug cube at (-0.5,-0.5,-0.5) is NORTH-WEST of the hero → BEHIND the camera
- **You won't see the cube in mode 1 default** unless you rotate with q/e

## Mode 2 (Third Person) Default View

- Camera pulled back: cam_y=1.6, cam_z_off=2.5 (behind hero)
- **yaw = cam_yaw** (same as mode 1)
- With default cam_yaw=180°, camera looks NORTH, pulled 2.5 units further NORTH
- Debug cube is still NORTH-WEST → behind the camera
- **Won't see the cube in mode 2 default** unless you rotate

## Mode 3 (Free Roam) Default View

- Starts at bird's eye: cam_y=12.0, cam_z_off=0
- **yaw = cam_yaw**, **pitch = cam_pitch** (reads from state.txt)
- When first activated: choice.c sets cam_yaw=180°, cam_pitch=-90° (bird's eye)
- After that, all controls work freely

## What You See at Default (cam_yaw=180°, cam_pitch=6°)

```
Mode 1: Camera at hero (18,3), looking SOUTH (yaw=0°)
        Cube at (-0.5,-0.5) is NORTH-WEST → NOT visible
Mode 2: Camera behind hero (18,5.5), looking SOUTH (yaw=0°)
        Cube at (-0.5,-0.5) is NORTH-WEST → NOT visible
Mode 3: Camera at (18,12), looking straight down (pitch=-90°, yaw=180°)
        Cube is visible below (bird's eye)
```

To see the cube in modes 1/2: press `e` (yaw+=10°) repeatedly or `q`
(yaw-=10°) until you face WEST (yaw≈90°) or NORTH (yaw≈180°).
