# Skybox Gradient Legend

Vertical 5-stop gradient rendered as the background before all 3D
geometry. Lets you tell up from down and by how much from color alone.

## Color Stops (top to bottom)

| Screen position | Color name    | RGB          | Meaning              |
|-----------------|---------------|--------------|----------------------|
| 0% (top)        | Deep indigo   | (0, 0, 50)   | Zenith / straight up |
| 25%             | Steel blue    | (40, 80, 150)| Upper sky            |
| 50% (center)    | Pale gray     | (150, 165, 180)| Horizon / level    |
| 75%             | Olive brown   | (80, 65, 35)  | Lower ground        |
| 100% (bottom)   | Dark earth    | (25, 15, 8)   | Nadir / straight down|

Between stops, colors are linearly interpolated.

## Reading Orientation

- **Screen mostly blue/indigo** → camera is pointed upward
- **Screen split blue-above / brown-below at center** → camera is level
- **Screen mostly brown/olive** → camera is pointed downward
- **Horizon line (gray) near top** → looking steeply down
- **Horizon line near bottom** → looking steeply up
- **Horizon at center** → pitch near 0, looking level

## How Tilt Maps to Color

With pitch = 0 (level), the horizon (pale gray) sits at the screen
center. As pitch increases (r key, looking down), the horizon slides
upward and more brown/nadir fills the screen. As pitch decreases (t
key, looking up), the horizon slides downward and more blue/zenith
fills the screen.

Approximate horizon position at common pitches:

| Pitch | Horizon position  | Dominant screen color |
|-------|-------------------|-----------------------|
| -45   | ~25% from top     | Deep blue / indigo    |
| -30   | ~33% from top     | Steel blue            |
| 0     | ~50% (center)     | Balanced blue/brown   |
| +30   | ~67% from top     | Olive brown           |
| +45   | ~75% from top     | Dark earth            |
| +89   | ~100% (bottom)    | All dark earth/nadir  |

## Implementation

`compose_rgb_frame.c` → `render_3d_view()`, inserted after view-radius
setup and before Pass 1 (floor quad rendering). The gradient fills the
entire framebuffer (fb_w × fb_h, RGBA32); geometry is then painted on
top, replacing gradient pixels where floors/walls are visible.
