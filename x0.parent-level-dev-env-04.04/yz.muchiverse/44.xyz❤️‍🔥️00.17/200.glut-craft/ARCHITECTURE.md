# ARCHITECTURE — 200.glut-craft

## Why freeglut-first

House stack (muta `gl_mirror`, prisc, CHTPM file loops) is powerful but heavy for agent iteration.  
This package is a **self-contained freeglut process** that still keeps **file-shaped world state** so a later bridge is cheap.

| Concern | Today (glut-craft) | Later house mapping |
|---------|--------------------|---------------------|
| Window / GL | freeglut single window | muta `gl_mirror` / `150.gl-canvas` style surface |
| Frame loop | `glutTimerFunc` ~60 Hz | CHTPM tick + present, or prisc frame pump |
| World state | `uint8_t[]` in RAM + `saves/<name>/world.bin` | pieces/ledger files, chunk folders, optional PDL meta |
| Input | GLUT keyboard/mouse | house input bus / event-editor patterns |
| HUD | orthographic GLUT bitmap | same draw idea in gl_mock / widget layer |
| Process model | one `./glut-craft` | still one visual client; sim can split out |

## Module map

```text
main.c      glutInit, callbacks, timer, save hotkeys
world.c     gen (value-noise fbm), get/set, save/load
player.c    yaw/pitch camera, AABB collision, DDA-ish raycast
render.c    face-culled cubes + face shades + fog + HUD chrome
inv.c       9-slot hotbar of block ids
```

**Data flow:** input → `Player` → collision against `World` → raycast target → break/place mutates `World` → `render_world` / `render_hud`.

## Save format (file-mediated)

```text
saves/<name>/
  world.bin   # WORLD_W*H*D raw uint8 block ids, X + W*(Y + H*Z)
  meta.txt    # name, seed, dimensions, format tag
```

This is intentionally boring so a future house tool can:

1. mmap / copy `world.bin` into a sim process  
2. rewrite meta as pdl-ish key/value  
3. stream chunk files (`cx_cy_cz.bin`) without changing the game API surface (`world_get` / `world_set`)

## Render strategy

- Fixed world 128×64×128 (~1M cells, ~1 MiB)  
- Draw only blocks within `RENDER_RADIUS` of the player  
- Skip internal faces (neighbor opaque)  
- Classic face lighting multipliers: top 1.0, X 0.8, Z 0.7, bottom 0.5  
- Linear fog toward radius for depth cue  

**Not yet:** mesh caching / VBO, texture atlas, multi-chunk streaming (stretch).

## Frame throttle

No `glutIdleFunc` busy loop.  
`glutTimerFunc(16, …)` schedules logic + `glutPostRedisplay` ≈ 60 fps, so idle CPU stays low.

## Controls / game rules (sim core)

Player is an eye point + AABB (half-width 0.3, height 1.7, eye 1.62).  
Walk: gravity + jump. Fly: free 3-axis move.  
Break/place: step ray up to reach 6; place on previous air cell.

These rules can move into a headless `sim_step(dt)` later; GLUT only presents.

## Path to gl_mirror + CHTPM

Suggested bridge (document only — **not implemented here**):

1. **Extract** `world_*` + `player_update` into a tiny static lib used by glut-craft.  
2. **Write** periodic snapshots (already `world_save`) on a timer for external tools.  
3. **Swap** present path: freeglut display → texture blit into house canvas, or reimplement `render_*` against house GL helpers.  
4. **Keep** `saves/` as the contract so agents and pets can read terrain without linking this binary.

## Non-goals (remain stubs / absent)

- Multiplayer, redstone, dimensions  
- Crafting UI (stretch)  
- Real PNG texture atlas  
- Full CHTPM loop inside this folder  

## Build

Single gcc line (see `button.sh`):

```sh
gcc -std=c11 -Wall -Wextra -O2 -I./src \
  -o glut-craft src/main.c src/world.c src/player.c src/render.c src/inv.c \
  -lGL -lGLU -lglut -lm
```
