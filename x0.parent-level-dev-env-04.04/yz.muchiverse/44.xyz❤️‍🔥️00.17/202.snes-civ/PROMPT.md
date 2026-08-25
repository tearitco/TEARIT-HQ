# PROMPT — 202.snes-civ (SNES Civilization clone)

**Self-contained freeglut + C.** Target: playable Civilization-class turn-based 4X **inspired by SNES Civilization** (CivNet / MicroProse-era feel), not full Civ VI.

## Compromise architecture
- Pure freeglut (like glut-craft / rpg gl_mock) — agent-fast.
- Optional house DNA: save as `saves/*/world.pdl` + map grid files; document CHTPM bridge in ARCHITECTURE.md.
- **POSIX `sh` button.sh**: compile | run | kill | help

## MVP (must work)
1. **Map**: toroidal or flat 40×30 tiles (ocean, plains, forest, hills, mountain, special).
2. **Units**: settler, warrior, scout — move on arrow keys / click; movement points per turn.
3. **Cities**: build with settler; produce food/shields; build units.
4. **Turns**: End Turn advances all AI civs (simple random/greedy).
5. **Fog of war** optional stretch; at least unexplored dark.
6. **Multiple civs** (player + 2–3 AI): colors, names.
7. **HUD**: year, gold, science stub, selected unit, mini status — **readable GLUT text**, polished dark UI chrome.
8. **Look**: top-down tiles with face/elevation shading (not unlit blobs); city markers; unit glyphs.
9. ~60fps timer (`glutTimerFunc(16)`), **no glutIdleFunc spin**, dirty redraw when possible.

## Stretch
- Tech tree lite, diplomacy menu stub, combat odds, save/load

## Layout
```
202.snes-civ/
  PROMPT.md README.md ARCHITECTURE.md button.sh
  src/*.c *.h
  saves/
```

## Acceptance
- `sh button.sh compile && sh button.sh run`
- Can found a city, produce a unit, end turn, fight or explore
- Idle CPU not pegging a core

## Visual bar
Polished freeglut HUD like event-editor mock quality — not homework-colored rectangles only.

*End PROMPT*
