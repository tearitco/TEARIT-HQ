# 007-goldeye — Development Log

## Overview
A voxel-based GoldenEye 007 split-screen deathmatch in C/OpenGL (freeglut).
Open island terrain with 4 biomes, water, mountains, trees, tanks, and helicopters.
**100×100 map, Y up to 28**. Islands surrounded by water.

## Status (2026-07-28)

### Controls
| Key | Action |
|-----|--------|
| **↑ / ↓** | Forward / Back |
| **← / →** | Strafe |
| **A / D** | Turn |
| **J** | Jump |
| **PgUp / PgDn** | Look up / down |
| **F** | Fire |
| **E** | Enter / exit tank or helicopter |
| **Space** | Sprint |
| **Esc** | Menu |
| **P** | Pause (debug/cheats placeholder) |
| **1** | First-person camera |
| **2** | Third-person camera |
| **S** | Reset camera (yaw/pitch) |
| **Ctrl+C** | Quit |

### Features
- **Bigger world**: 100×100×28 voxel map (was 56×16×56)
- **Terrain**: Heightmap-generated mountains, valleys, hills using multi-octave noise
- **4 biomes** (Plains=green, Desert=sand, Snowy=white, Forest=green+trees)
- **Island**: Map surrounded by water (4-block border), valleys below water level fill with water
- **Water rendering**: Blue blocks with back-face culling disabled
- **Trees**: Forest biome has procedural trees (trunk + leaf canopy)
- **Player 1 defense**: Starts with 150 HP (vs 100 for AI), respawns with 150 HP
- **Farther spawn**: Players spread 2 apart in spawn index, face inward
- **Camera toggle**: `1`=first person (default), `2`=third person (behind/above)
- **Camera reset**: `S` resets yaw/pitch to zero
- **K/D display**: HUD shows Kills/Deaths ratio for each player
- **Movement fix**: Turn now applies before forward vector calculation (instant direction update)
- **Buildings**: Still placed on flat terrain areas (stairs/windows/roofs)
- **Vehicles**: 2 tanks + 2 helicopters with T/H labels on minimap
- **Weapons**: 3 pickup types (Pistol, Auto, Shotgun) + fists, shown as barrel/grip/muzzle models
- **Humanoid players**: Legs + torso + head using scaled cubes
- **Minimap**: Shows terrain colors per biome, vehicle labels, gun pickups, player positions
- **Damage overlay**: Red vignette when HP < 30, solid red when dead
- **Split-screen**: 2-4 players with dynamic layout
- **AI**: 3 AI opponents with pursuit, vehicle boarding, and combat
- **PID file**: `button.sh kill` or Ctrl+C

### Done
- 4-player split-screen (1 human + 3 AI)
- Procedural outdoor + multistory buildings with stairs/windows/roofs
- 40 weapon pickups in random outdoor and building locations
- Hitscan melee combat (FISTS)
- Projectile bullets with damage + vehicle protection
- AI pathfinding, combat, and vehicle boarding
- Frag limit (10 kills) with winner overlay
- Vehicles: tanks (2) and helicopters (2) with full driving/flying
- Humanoid player models (legs + torso + head)
- Weapon pickup models (barrel + grip + muzzle tip)
- Death/damage overlay (red vignette increasing with damage)
- Pause menu with placeholder debug/cheats
- PID file — `sh button.sh kill` or Ctrl+C
- Heightmap terrain with noise-based generation
- 4 biomes (plains/desert/snow/forest) with block colors
- Island water border + valley water filling
- Trees in forest biome
- Camera modes: first (1) / third (2) person
- Camera reset (S key)
- K/D ratio on HUD
- Player 1: 150 HP + 150 HP on respawn

### Known Issues
- No mouse support (intentional)
- No sound
- No networking (local split-screen only)
- AI is simple (direct pursuit, no flanking)
- Menu limited (no options/settings screen yet)
- No crosshair dot (just cross lines)
- Buildings may clip terrain on steep slopes
- Tree leaves are just green cubes (ROOF block)

### To-Do / Ideas
- [ ] Add crosshair dot
- [ ] Weapon HUD icons
- [ ] Damage numbers / hit markers
- [ ] Sound effects (OpenAL / SDL_mixer)
- [ ] Health packs / armor pickups
- [ ] More weapons (golden gun, mines)
- [ ] Vehicle weapon crosshair (tank cannon, heli missiles)
- [ ] Better AI (use cover, dodge, retreat)
- [ ] Map selector (multiple levels)
- [ ] Split-screen config (vertical/horizontal)
- [ ] Settings menu (frag limit, time limit, etc.)
- [ ] Screen shake on damage
- [ ] Kill feed / death messages
- [ ] Better vehicle models (turret, rotor blades animation)
- [ ] Particle effects (explosions, debris)
- [ ] Underwater / swimming
- [ ] Day/night cycle
