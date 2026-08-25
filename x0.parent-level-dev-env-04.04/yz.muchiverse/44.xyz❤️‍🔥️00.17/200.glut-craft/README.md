# 200.glut-craft

Self-contained **Minecraft-class voxel sandbox** in pure **C + freeglut + GLU**.  
No mutaclysm / prisc dependency. One binary, one `button.sh`.

## Quick start

```sh
cd 200.glut-craft
sh button.sh compile
sh button.sh run
```

**Deps (Debian/Ubuntu):** `gcc freeglut3-dev libglu1-mesa-dev`  
**Binary:** `./glut-craft` next to `button.sh`.

## Controls

| Input | Action |
|-------|--------|
| **Click** | Capture mouse |
| **Esc** | Release mouse |
| **WASD** | Move |
| **Mouse** | Look |
| **Space** | Jump (walk) / up (fly) |
| **Shift** / **C** | Down (fly) |
| **F** | Toggle fly / gravity |
| **LMB** | Break block |
| **RMB** | Place selected hotbar block |
| **1–9** | Hotbar select |
| **Ctrl+S** | Save → `saves/default/` |
| **Ctrl+L** | Load `saves/default/` |
| **Q** | Quit |

## Features (MVP)

- 128×64×128 voxel world, heightmap noise terrain (grass/dirt/stone/sand) + sparse trees  
- First-person camera, collision, gravity / fly  
- Break / place with raycast + target outline  
- Hotbar HUD, crosshair, FPS + position  
- Face lighting (top bright / sides mid / bottom dark), distance fog  
- Save / load raw block dump under `saves/<name>/`  
- Render throttle ~60 fps via `glutTimerFunc` (no idle spin)

## Layout

```text
200.glut-craft/
  PROMPT.md
  README.md
  ARCHITECTURE.md
  button.sh
  glut-craft          # after compile
  src/
    main.c world.* player.* render.* inv.*
  saves/              # runtime worlds
```

## CLI

```sh
./glut-craft --seed 99
./glut-craft --save myslot
./glut-craft --saves-root ./saves
```

## Kill stuck process

```sh
sh button.sh kill
```

See [ARCHITECTURE.md](./ARCHITECTURE.md) for house GL / CHTPM mapping notes.
