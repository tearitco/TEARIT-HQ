# ARCHITECTURE — 203.gb-pokemon (Game Boy Color)

## Why freeglut-first

House stack (muta, gl_mirror, CHTPM) is heavy for agent iteration.  
This package is a **self-contained freeglut process** with **file-shaped** mon/map/save state so a later bridge stays cheap.

| Concern | Today | Later house mapping |
|---------|--------|---------------------|
| Window / GL | freeglut single window | muta `gl_mirror` / `150.gl-canvas` |
| Frame loop | `glutTimerFunc(16)` ~60 Hz | CHTPM tick + present |
| Species / moves | `data/*.pdl` tables | ledger / PDL pets |
| Map | `maps/*/map.txt` | same text maps as RPG clone DNA |
| Party / save | `saves/slot0/save.txt` + `party.pdl` | slot folders, agent-readable |
| Battle | in-process state machine | optional headless `battle_step` |

## Module map

```text
src/gb.h       types, modes, API
src/main.c     glutInit, timer, input routing, overworld step + encounter
src/map.c      map.txt load, walkable, tile query
src/mon.c      mons.pdl / moves.pdl load, stats, type mult, damage
src/battle.c   wild battle phases: intro → menu → fight/run → win/lose
src/render.c   GBC full-color palette, tiles, battle boxes, HP bars
src/save.c     saves/slot0/save.txt + party.pdl
```

**Data flow:** input → mode handler → map step / battle_input → mutate `Player`/`Battle` → dirty flag → `render_frame`.

## Modes

```text
TITLE → STARTER → OVERWORLD ⇄ BATTLE
                 ↘ MSG (modal) ↗
CONTINUE loads save → OVERWORLD
```

## Frame throttle

**No `glutIdleFunc`.**  
`glutTimerFunc(TARGET_MS=16, …)` runs logic; `glutPostRedisplay` only when `g.need_redraw`.  
Idle CPU stays low (same pattern as `200.glut-craft`).

## Battle formula (MVP)

```text
base = ((2L/5+2) * power * Atk / Def) / 50 + 2
STAB 1.5 if move type == species type (non-Normal)
type mult: super 2×, resist ½× (Grass/Fire/Water RPS)
random 85–100%
damage = max(1, …)
```

Phases: `INTRO` → `MENU` (Fight/Run) → player damage → enemy damage → win/lose/run.

## Save format

```text
saves/slot0/
  save.txt    # format, map path, pos, party mon lines
  party.pdl   # human/agent-readable party summary
```

Relative paths only; binary must be run with cwd = package root (`button.sh run` does this).

## Visual bar

- Logical **160×144** (GB) × **scale 4** → 640×576 window  
- 4-tone green palette + type accent colors on mon blobs  
- Chunky 16px tiles; battle chrome as nested boxes  

## Path to house stack (document only)

1. Keep `map_*` / `mon_*` / `battle_*` as a static lib.  
2. Snapshot `saves/slot0/` on timer for external tools.  
3. Swap present path: freeglut → texture blit into house canvas.  
4. Maps and PDL stay the contract for agents/pets.

## Non-goals (stubs)

- Capture, shops, bag, multi-floor interiors  
- Trainer AI / multi-mon switch mid-battle  
- Audio, link cable, full Pokédex UI  
