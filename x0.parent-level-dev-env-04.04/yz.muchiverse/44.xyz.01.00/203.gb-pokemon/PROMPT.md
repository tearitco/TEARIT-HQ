# PROMPT — 203.gb-pokemon (Game Boy **Color** Pokémon–class clone)

**AMENDED:** This is **GBC (Game Boy Color)**, **not** original 4-shade monochrome GB.

**Self-contained freeglut + C.** Target: **Pokémon Red/Blue/Yellow on GBC** — full color overworld + wild battles + starter. Not full National Dex / Gen 3+.

## Compromise architecture
- Pure freeglut, agent-fast.
- House DNA where easy: `maps/*/map.txt`, `party.pdl`, `pokedex` as simple tables.
- **POSIX `sh` button.sh**: compile | run | kill | help

## Visual bar — **GBC color** (critical amendment)
- **Full RGB color** freeglut tiles and UI — **not** monochrome GB greens/grays only.
- Think GBC Pokémon: green grass, blue water, red/blue/yellow buildings, colorful mons, colored HP bars (green/yellow/red), type-colored battle accents.
- Chunkier pixels OK (scale 2–3× logical tiles) but **palette must be vivid GBC**, not grayscale.
- Battle UI: colored boxes, white/black text on colored panels, clear Fight/Run.

## MVP (must work)
1. **Overworld**: top-down tile map (grass, path, water, wall, tall grass, door) — **in color**.
2. **Player** walk 4-dir, collision, camera follow (or fixed room).
3. **Tall grass** → random wild encounter.
4. **Battle scene**: player mon vs wild; Fight / Run; damage formula simple (level, atk, def).
5. **Starter** choose (3 types, 3 colors); party of 1–2.
6. **HP bars** colored; type chart lite (3 types RPS).
7. **NPC** optional; heal at “Pokecenter” tile.
8. **Save** party + position to `saves/slot0/`.
9. ~60fps `glutTimerFunc(16)`, no `glutIdleFunc` spin; prefer dirty redraw when idle.

## Stretch
- More mons, items, badges, multi-map transfer, simple animation frames

## Layout
```
203.gb-pokemon/
  PROMPT.md README.md ARCHITECTURE.md button.sh
  src/
  data/ mons.pdl moves.pdl
  maps/ pallet/ map.txt
  saves/
```

## Acceptance
- `sh button.sh compile && sh button.sh run`
- Walk into grass → battle → win/run → continue; save works
- **Must look like GBC color**, not monochrome GB

## Agent rules
- No Desktop-wide `find` — stay inside project dir
- POSIX button.sh only

*End PROMPT — GBC Pokémon*
