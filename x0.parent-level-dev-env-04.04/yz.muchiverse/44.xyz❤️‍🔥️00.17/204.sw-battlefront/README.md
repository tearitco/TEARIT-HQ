# 204.sw-battlefront — Star Wars Battlefront* house clone

Three modes in one freeglut binary: **Supremacy**, **Deathmatch**, **Freeplay**.
GLSL shaders, generative textures, display-list ship meshes + instanced foliage/asteroids.

## Run
```sh
sh button.sh compile
sh button.sh run
# stuck / CPU:
sh button.sh kill
```

## Modes
| Mode | Play |
|------|------|
| **Supremacy** | Capture Alpha–Echo posts. Holding more posts drains enemy tickets. Team AI. |
| **Deathmatch** | Deep-space dogfight. First to **25** kills. Kill streak **buffs** (dmg + shield). |
| **Freeplay** | NMS/GTA-ish slice: multi-planet, on-foot + ships, mine, build, O2, lightsaber. |

## Ships
Interceptor · Fighter · Bomber · Freighter · Speeder — cycle with `[` `]`

## Controls
| Input | Action |
|-------|--------|
| Mouse | Look (click window to capture) |
| WASD | Move / fly |
| Shift | Boost (ship) / sprint (foot) |
| Space / Ctrl | Climb / dive (ship) |
| LMB | Fire blasters / saber |
| RMB | Bomber rockets |
| E | Enter / exit ship |
| Tab | Cycle on-foot weapons |
| F | Mine resources (Freeplay) |
| 1–5 | Build turret / shield / outpost / farm / mine |
| P | Cycle planet (Freeplay) |
| R | Field repair |
| Q / Esc | Menu |
| Enter | Launch selected mode |

## Tech
- OpenGL + **GLEW** shaders (lit hull, sky/atmosphere, laser additive)
- Procedural **FBM** heightfields per planet
- Generative **noise / star / hull panel** textures
- **Display lists** for ships & props; grid **instancing** of trees/rocks/asteroids
- `glutTimerFunc(16)` only — no idle spin

## Honest scope
This is a high-polish vertical slice, not licensed Battlefront or a full NMS. No multiplayer netcode, no full progressive story, no photogrammetry assets — all generative / mesh-primitive AAA *presentation* within freeglut.
