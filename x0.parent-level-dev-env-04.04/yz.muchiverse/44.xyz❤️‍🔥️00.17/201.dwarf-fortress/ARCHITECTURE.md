# ARCHITECTURE — 201.dwarf-fortress

## Why freeglut-first

House stack (muta `gl_mirror`, prisc, CHTPM file loops) is powerful but heavy for agent iteration.  
This package is a **self-contained freeglut process** with **file-shaped fort state** so a later bridge is cheap.

| Concern | Today (dwarf_fortress) | Later house mapping |
|---------|------------------------|---------------------|
| Window / GL | freeglut single window | muta `gl_mirror` / `150.gl-canvas` |
| Frame loop | `glutTimerFunc` ~60 Hz | CHTPM tick + present |
| Fort state | RAM grids + `saves/<name>/*.txt` | pieces/ledger, optional PDL meta |
| Jobs / AI | in-process BFS + job table | headless `sim_step` service |
| HUD | orthographic bitmap panels | gl_mock / widget layer |

## Module map

```text
fort.h      shared constants + Fort / Tile / Dwarf / Job / Item
main.c      glutInit, input modes, pause, timer, save hotkeys
map.c       embark gen, walkability, items, designations
unit.c      spawn, BFS path, per-tick dwarf AI
job.c       designation→job sync, assign, complete (mine/cut/build/craft/haul)
render.c    colored tiles, glyphs, side chrome, message bar
save.c      saves/<name>/{meta,map,items,dwarves}.txt
```

**Data flow:** input sets designations / craft_order → `job_sync_from_designations` → `job_try_assign` → dwarf path/work → `job_complete` mutates map/items → `render_frame`.

## Map model

- Single z-level, `MAP_W×MAP_H` (48×48).  
- `Tile.terrain`: soil/rock walls, floor, water, tree, constructed wall, workshop.  
- `Tile.desig`: dig / cut / build overlays.  
- `Tile.stock_wood` / `stock_stone`: stockpile paint (persistent).  
- Loose `Item` stacks on cells; HUD counts aggregate wood/stone.

## Jobs & pathfinding

- Designations create jobs if missing (idempotent per cell/kind).  
- Haul jobs: item not on matching stockpile → nearest stockpile cell.  
- BFS 4-neighbor on walkable floors/trees/workshop; dig/cut targets use **adjacent** stand tiles.  
- Work duration: fixed tick budgets per job kind (not skill-based).

## Save format (file-mediated)

```text
saves/<name>/
  meta.txt      fort, seed, calendar, craft_order, format tag
  map.txt       W/H header + per-cell terrain,desig,stock flags
  items.txt     kind x y count lines
  dwarves.txt   name x y hunger thirst state
```

Jobs are **not** serialized (rebuilt from designations on load; dwarves reset to idle).  
Intentional: agents can edit `map.txt` / spawn items without job graph integrity issues.

## Frame throttle

- **No** `glutIdleFunc`.  
- `glutTimerFunc(16, …)` ≈ 60 Hz.  
- Sim advances every ~120 ms only while **unpaused**.  
- `glutPostRedisplay` when `dirty` / cursor / FPS HUD tick — paused static fort does not peg a core.

## Path to gl_mirror + CHTPM

1. Extract `map_*` / `unit_step_all` / `job_*` into a static lib.  
2. Keep `saves/` as the contract for external tools.  
3. Swap present path: freeglut → house canvas blit.  
4. Optional: multi-z as stacked `map_zN.txt` without changing job API.

## Stubs / non-goals (not in MVP)

| Stub | Notes |
|------|--------|
| Multi-z + stairs | 1 z only |
| Hunger/thirst effects | Counters increment; no death/food jobs |
| Migrants / animals / combat / moods | Absent |
| Magma / fluids flow | Static water tiles |
| Trade caravan | Absent |
| Classic ASCII toggle | Hybrid colored tiles + glyphs only |
| Channel / ramp / bridge | Dig wall→floor only |
| Manager / work orders UI | Craft via build menu 5/6 |
| Skills / labors screen | All dwarves do all jobs |

## Build

```sh
gcc -std=c11 -Wall -Wextra -O2 -I./src \
  -o dwarf_fortress \
  src/main.c src/map.c src/unit.c src/job.c src/render.c src/save.c \
  -lGL -lGLU -lglut -lm
```
