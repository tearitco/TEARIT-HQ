# 203.gb-pokemon — Game Boy Color Pokémon MVP

Self-contained **Game Boy Color (GBC)** Pokémon–class overworld + wild battles in pure **C + freeglut**.

**Full RGB color** — not monochrome original Game Boy greenscale. Target feel: Pokémon Red/Blue/Yellow on GBC (colored tiles, mons, battle UI). Not a full National Dex.

## Quick start

```sh
cd 203.gb-pokemon
sh button.sh compile
sh button.sh run
```

Requires: `gcc`, freeglut (`-lglut -lGL -lGLU`), `DISPLAY`.

| Verb | Alias | Action |
|------|-------|--------|
| `compile` | `c` `build` | Build `./gb-pokemon` |
| `run` | `r` `start` | Run (compile if needed; cwd = package) |
| `kill` | `k` `stop` | Stop running instance |
| `help` | | Usage |

`FOREGROUND=1 sh button.sh run` — exec without pid file.

## How to play

1. **Title** — `↑/↓` + `Z`/`Enter`/`Space`: **NEW GAME** / **CONTINUE** / **QUIT**
2. **Starter** — `←/→` pick **LEAFY** (Grass), **EMBER** (Fire), or **BUBBLE** (Water) → `Z`
3. **Overworld** — arrows or **WASD** walk. Camera follows.
   - **Vivid green tall grass** = encounter zone → random wild (~18%)
   - **Pink/red PC tile** (`P`) = Pokecenter — full party heal
   - **F5** = save to `saves/slot0/`
4. **Battle**
   - `↑/↓` select **FIGHT** / **RUN**, `Z` confirm
   - Or press **`1` / `2`** for move slot 1 / 2 immediately
   - Type chart (lite): Grass > Water > Fire > Grass; Normal is neutral
   - HP bars: green / yellow / red
5. **Blackout** — if your mon faints, warp to start and heal
6. **`q`** quit · **Esc** back to title (from overworld)

## Visual bar (GBC)

| Element | Color |
|---------|--------|
| Grass / lawn | Bright green |
| Tall grass | Deep + lime green blades |
| Water | Ocean blue + cyan waves |
| Paths | Tan / brown dirt |
| Walls | Brick red |
| Pokecenter | Pink floor, red cross |
| Houses | Warm wood |
| Mons | Type/species body colors |
| Fight / Run | Red / blue menu accents |
| HP | Green → yellow → red |

Logical 160×144 screen ×4 scale (chunky pixels).

## Layout

```
203.gb-pokemon/
  PROMPT.md AMEND_GBC.md README.md ARCHITECTURE.md button.sh
  src/          main map mon battle save render
  data/         mons.pdl moves.pdl
  maps/pallet/  map.txt
  saves/slot0/  save.txt party.pdl (runtime)
```

## Tech notes

- **~60 fps** via `glutTimerFunc(16)` only — **no** `glutIdleFunc` spin; dirty redraw when idle
- House DNA: editable `mons.pdl` / `moves.pdl` / `map.txt`

*Game Boy Color edition*
