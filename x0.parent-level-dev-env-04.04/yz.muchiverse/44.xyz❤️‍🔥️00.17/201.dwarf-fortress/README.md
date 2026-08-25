# 201.dwarf-fortress

Self-contained **Dwarf Fortress–inspired fort sim** in pure **C + freeglut**.  
Not Bay12 feature parity — a playable embark: dig, cut, stockpile, build, craft.

## Quick start

```sh
cd 201.dwarf-fortress
sh button.sh compile
sh button.sh run
```

**Deps (Debian/Ubuntu):** `gcc freeglut3-dev libglu1-mesa-dev`  
**Binary:** `./dwarf_fortress` next to `button.sh`.

## How to play (MVP loop)

1. **Start paused** — look around with **Arrows** / **WAS** (W/A/S) or mouse.
2. Press **`d`**, move the cursor onto the **rock cliff** (west of embark), press **Enter** (or click-drag) to designate mining.
3. Press **`t`**, designate **trees** (green `T`) to cut.
4. Press **`b`** → **`3`** to place a **wood stockpile** on open floor; **`4`** for stone.
5. **Space** to **unpause** — dwarves (`@`) path to jobs, mine/cut, haul to stockpiles.
6. When you have **3 wood**, **`b`** → **`2`**, designate a **carpenter workshop** on floor.
7. Unpause until built, then **`b`** → **`5`** (bed) or **`6`** (chair) to craft.
8. **Ctrl+S** save / **Ctrl+L** load under `saves/default/`.

## Controls

| Input | Action |
|-------|--------|
| **Arrows** / **W A S** | Move cursor (Shift+Arrow = ×5) |
| **Mouse** | Move cursor; LMB drag designate; RMB cancel |
| **Space** | Pause / unpause |
| **d** | Dig designation mode |
| **t** | Cut-tree designation |
| **b** | Build menu (wall / workshop / stock / craft) |
| **1–6** | Build menu picks (in build menu) |
| **Enter** | Confirm designation / query select |
| **Esc** | Cancel to LOOK mode |
| **q** | Query mode (Enter selects unit) |
| **Tab** | Cycle selected dwarf + jump cursor |
| **[ ]** | Nudge camera X |
| **Ctrl+S** | Save → `saves/default/` |
| **Ctrl+L** | Load `saves/default/` |
| **Shift+Q** | Quit |

### Build menu (`b`)

| Key | Action |
|-----|--------|
| 1 | Construct wall (1 stone) |
| 2 | Carpenter workshop (3 wood) |
| 3 | Wood stockpile |
| 4 | Stone stockpile |
| 5 | Order bed (2 wood) |
| 6 | Order chair (1 wood) |

## Features (MVP)

- 48×48 embark: soil/rock walls, floor, trees, water pool  
- 5 dwarves: idle → BFS path → work jobs  
- Designations: dig, cut, stockpiles, wall, workshop  
- Items: wood, stone, bed, chair; haul to stockpiles  
- Workshop craft recipes: bed, chair  
- Side HUD: fort name, season/year, stocks, unit list, mode  
- Save/load text files under `saves/<name>/`  
- ~60 fps via `glutTimerFunc` — **no** `glutIdleFunc` busy loop; dirty redraw when paused/static  

## Layout

```text
201.dwarf-fortress/
  PROMPT.md
  README.md
  ARCHITECTURE.md
  button.sh
  dwarf_fortress      # after compile
  src/
    fort.h main.c map.* unit.* job.* render.* save.*
  saves/
```

## CLI

```sh
./dwarf_fortress --seed 99
./dwarf_fortress --save myslot
./dwarf_fortress --saves-root ./saves
```

## Kill stuck process

```sh
sh button.sh kill
```

See [ARCHITECTURE.md](./ARCHITECTURE.md) for module map and house CHTPM notes.
