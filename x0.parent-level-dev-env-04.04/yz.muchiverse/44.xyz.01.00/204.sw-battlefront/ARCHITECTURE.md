# ARCHITECTURE — 204.sw-battlefront

```
main.c   GLUT loop, input, mode switch
gen.c    FBM noise, height, procedural textures
gfx.c    GLSL programs, sky, terrain, ships, FX, lists
sim.c    Ship defs, combat, AI, posts, freeplay economy
ui.c     Menu + combat HUD
sw.h     Shared types
```

## Loop
`glutTimerFunc(16)` → input → `sim_update` → dirty/always redisplay in play.

## Modes
- Menu owns ship/difficulty selection
- `sim_start_mode` rebuilds entities, posts, planet seed
- Freeplay `P` cycles `enum Planet`

## Rendering
- Optional GLSL (GLEW); fixed-function fallback paths exist partially
- Terrain: CPU height samples, chunked triangle strips around player
- Instancing: nested loops + `glCallList` (trees, rocks, asteroids)
- Weapons: additive blended laser lines + explosion cubes
