# ARCHITECTURE — 202.snes-civ

## Intent

Agent-fast **Civilization-class** 4X in one freeglut process, SNES / CivNet-era density.  
File-shaped save path is reserved so a later house bridge (CHTPM / muta `gl_mirror`) does not require a format rewrite.

**Not** the CHTPM → rgb → `gl_mirror` path today. Same product lane as `200.glut-craft` and `201.rpg-maker-clone`.

## Module map

| File | Role |
|------|------|
| `src/civ.h` | Types: `Game`, `Tile`, `Unit`, `City`, `Civ`; public API |
| `src/map.c` | Value-noise fBm heightmap, wrap-X, move costs, reveal bits, tile colors |
| `src/game.c` | Init, selection, move/combat, cities, production, end-turn, AI |
| `src/render.c` | Shaded tiles, unit/city glyphs, HUD chrome, minimap |
| `src/main.c` | freeglut callbacks, timer, input → game API |

```text
input (keys/mouse)
    → game_*  (mutate Game)
    → dirty flag
timer 16ms
    → glutPostRedisplay if dirty / HUD fps tick
display
    → render_frame(Game)
```

## Sim rules (core)

### Map

- `MAP_W=40`, `MAP_H=30`
- X is toroidal (`map_wrap_x`); Y clamped
- Terrain: ocean, plains, forest, hills, mountain, special
- `Tile.explored` bitfield (bit 0 = player … bit 3 = civ3)
- Move costs: plains/special 1, forest/hills 2, mountain 3, ocean blocked

### Units

- Settler / Warrior / Scout — MP, HP, atk/def tables in `game.c`
- One unit per tile; combat when entering enemy tile
- Settler `B` → city, unit removed

### Cities

- Yield: food + shields from 3×3 neighborhood (ocean skipped)
- Pop eats 1 food/turn; surplus grows pop
- Shields bank toward production (`Warrior` 10 / `Settler` 20 / `Scout` 8)
- Gold = pop per city per turn (player + AI)

### Turns

1. Player acts until **End Turn**
2. Player cities process (food/prod)
3. Each AI: refresh MP → process cities → move/found/attack
4. Year advances; player MP refreshed; auto-select next unit
5. Wipe check → victory / defeat

### AI

Greedy/random: wander, attack adjacent hostiles, settle on decent land when not near own cities, biased production.

## Render strategy

- Orthographic 2D (`glOrtho` pixel space)
- Per-tile smooth shade (TL bright / BR dark) + elev cue + terrain decals
- Unexplored: near-black fog
- HUD: dark panels, gold accent lines, Helvetica GLUT bitmaps
- Minimap + civ legend
- No textures / VBO — immediate mode, map is small (≤1200 tiles)

## Frame throttle

**No `glutIdleFunc`.**  
`glutTimerFunc(16, …)` ≈ 60 Hz tick; `glutPostRedisplay` only when `dirty` or half-second FPS HUD refresh so idle CPU stays low.

## Optional save contract (stub)

```text
saves/<name>/
  world.pdl     # reserved: civ meta, year, gold, seed
  map.grid      # reserved: MAP_H lines × MAP_W terrain bytes/chars
  units.pdl     # reserved
  cities.pdl    # reserved
```

Not written by MVP binary yet. Documented so agents can wire:

1. dump `Game` → files after end turn  
2. load before `game_init` alternate path  
3. external tools read terrain without linking freeglut  

## Path to gl_mirror + CHTPM

| Concern | Today | Later house mapping |
|---------|-------|---------------------|
| Window / GL | freeglut single window | muta `gl_mirror` / `150.gl-canvas` |
| Frame loop | `glutTimerFunc` | CHTPM tick + present |
| World state | RAM `Game` | `saves/*/world.pdl` + grid files |
| Input | GLUT | house input bus |
| HUD | ortho bitmap chrome | same draw idea in widget layer |

Suggested bridge (not implemented):

1. Keep `game_*` pure (no GLUT includes in `game.c` / `map.c`) — already true.  
2. Snapshot `Game` to `saves/default/` on a key.  
3. Swap `render_frame` backend to house GL helpers.  
4. Drive `game_end_turn` from sim process; freeglut (or mirror) only presents.

## Non-goals / stubs

- Tech tree (science counter only)  
- Diplomacy UI  
- Naval units  
- Multiplayer  
- Full save/load  
- Combat odds panel  
- Mesh/texture atlas  

## Build

```sh
gcc -std=c11 -Wall -Wextra -O2 \
  -o snes-civ src/main.c src/map.c src/game.c src/render.c \
  -lGL -lGLU -lglut -lm
```

Or: `sh button.sh compile`.
