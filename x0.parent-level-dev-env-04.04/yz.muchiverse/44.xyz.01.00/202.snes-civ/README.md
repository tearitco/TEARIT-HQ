# 202.snes-civ

Self-contained **Civilization-class turn-based 4X** in pure **C + freeglut**, SNES / MicroProse-era feel (not Civ VI).

No mutaclysm / prisc dependency. One binary, one `button.sh`.

## Quick start

```sh
cd 202.snes-civ
sh button.sh compile
sh button.sh run
```

**Deps (Debian/Ubuntu):** `gcc freeglut3-dev libglu1-mesa-dev`  
**Binary:** `./snes-civ` next to `button.sh`.  
**DISPLAY** required for `run`.

| Verb | Alias | Action |
|------|-------|--------|
| `compile` | `c` `build` | Build `snes-civ` |
| `run` | `r` `start` | Compile if needed, run |
| `kill` | `k` `stop` | Stop running instance |
| `help` | | Usage |

```sh
FOREGROUND=1 sh button.sh run
sh button.sh run --seed 42
sh button.sh kill
```

## How to play

You are **Rome** (blue). Rivals: Egypt, Aztecs, Babylon.

1. **Explore** with Scout (`C`) and Warrior (`W`) — arrows or click destination.
2. **Found a city** — select Settler (`S`), stand on good land, press **`B`**.
3. **Produce units** — select city, **`[` / `]`** cycle Warrior / Settler / Scout. Shields accumulate each turn.
4. **End turn** — **`Space`** (or `E`). AI civs move, fight, settle.
5. **Combat** — move onto an enemy unit. Simple attack/defense rolls; capture empty cities.
6. **Victory** — eliminate all rival civs (units + cities). **Defeat** if Rome is wiped out.

### Controls

| Input | Action |
|-------|--------|
| **Arrows** | Move selected unit (1 tile; diagonals via click step) |
| **Click** | Select friendly unit/city, or step-move toward tile |
| **N** / **Tab** | Next player unit (prefers remaining MP) |
| **B** | Found city (Settler consumed) |
| **[** **]** | Cycle city production |
| **WASD** | Pan camera |
| **Space** / **E** | End turn |
| **Q** / **Esc** | Quit |

### Units

| Glyph | Unit | Moves | Role |
|-------|------|-------|------|
| S | Settler | 1 | Found cities (`B`) |
| W | Warrior | 1 | Fight / capture |
| C | Scout | 2 | Explore (wider reveal) |

### Terrain

Ocean (impassable), Plains, Forest, Hills, Mountain, Special (gold resource tile).  
Movement costs 1–3; last step may overspend remaining MP.

### Fog

Unexplored tiles stay dark. Units reveal a radius on move (scouts see farther).

## Layout

```text
202.snes-civ/
  PROMPT.md README.md ARCHITECTURE.md button.sh
  snes-civ              # after compile
  src/  main.c map.c game.c render.c civ.h
  saves/default/        # reserved for world.pdl bridge
```

## Features (MVP)

- 40×30 wrap-X map, heightmap continents  
- Units, cities, food/shields, production  
- 4 civs, greedy/random AI  
- Turn year clock (4000 BC →)  
- Combat + city capture  
- Shaded tiles, dark gold-chrome HUD, minimap, civ legend  
- `glutTimerFunc(16)` ~60 fps, **no** `glutIdleFunc` spin  

## Stubs / stretch (not full)

| Stub | Notes |
|------|--------|
| **Science / tech tree** | HUD counter only; no techs unlock yet |
| **Diplomacy** | No menus, treaties, or AI talk |
| **Save / load** | `saves/` reserved; optional `world.pdl` documented in ARCHITECTURE |
| **Ships / ocean** | Ocean impassable |
| **Combat odds UI** | Rolls happen; no percent preview |
| **Toroidal Y** | X wraps; Y is flat edges |
| **Stacking** | One unit per tile |

See [ARCHITECTURE.md](./ARCHITECTURE.md) for module map and CHTPM bridge notes.
