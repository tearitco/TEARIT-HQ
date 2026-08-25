# SP-IRL — Tactical Monsters

Top-down tactics monster-battler in **C + freeglut** (FFT/Disgaea-style grid combat).

Evolved from a GBC Pokémon clone — now targeting real-time overworld + turn-based tactics battles on the same screen.

## Quick start

```sh
sh button.sh compile
sh button.sh run
```

Requires: `gcc`, freeglut (`-lglut -lGL -lGLU`), `DISPLAY`.

| Verb | Alias | Action |
|------|-------|--------|
| `compile` | `c` `build` | Build `./spirl` |
| `run` | `r` `start` | Run (compile if needed; cwd = package) |
| `kill` | `k` `stop` | Stop running instance |
| `help` | | Usage |

`FOREGROUND=1 sh button.sh run` — exec without pid file.

## How to play

1. **Title** — `↑/↓` + `Z`: **NEW GAME** / **CONTINUE** / **PVP BATTLE** / **QUIT**
2. **New Game** — pick starter, explore overworld, tall grass → wild battle
3. **PvP Battle** — tactics grid mode for debugging mechanics (WIP)
4. **`q`** quit · **Esc** back to title

## Layout

```
SP-IRL/
  PROMPT.md README.md ARCHITECTURE.md button.sh
  src/          main map mon battle save render
  data/         mons.pdl moves.pdl
  maps/pallet/  map.txt
  saves/slot0/  save.txt party.pdl (runtime)
```

## Tech notes

- **~60 fps** via `glutTimerFunc(16)` only — **no** `glutIdleFunc` spin
- Editable `mons.pdl` / `moves.pdl` / `map.txt`
