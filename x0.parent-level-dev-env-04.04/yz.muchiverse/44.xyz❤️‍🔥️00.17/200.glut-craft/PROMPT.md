# PROMPT — 200.glut-craft (self-contained Minecraft-class voxel game)

**House:** `44.xyz…`  
**Role:** Build a **self-contained**, agent-friendly Minecraft-like game using **pure freeglut + C** (same visual quality bar as `&.widgits/event-editor/gl_mock` RMMV mock — clean HUD, readable text, polished chrome).  

**Compromise (best of both worlds):**
- **NOT** full CHTPM/prisc stack on day one (slow for agents; easy to break).  
- **YES** freeglut primary (looks good, one process, easy compile).  
- **YES** house-aligned **file data** where it helps (world save folders, config pdl-ish).  
- **YES** `ARCHITECTURE.md` section: how this maps later to muta `gl_mirror` + file-mediated state.

---

## Goal product (feature-rich, shippable MVP → stretch)

### Must ship (MVP — do these first, fully working)
1. **3D first-person** voxel world (perspective GL, mouse look + WASD).  
2. **Infinite-ish or large fixed world** (e.g. 128×64×128 or chunked 16³ × N chunks).  
3. **Terrain generation** (heightmap noise: dirt/grass/stone layers, air above).  
4. **Break / place** blocks (LMB break, RMB place) with selected hotbar block.  
5. **Hotbar** (1–9) + on-screen HUD (crosshair, selected block, FPS, position).  
6. **Collision** solid blocks; gravity or fly toggle (`F` fly).  
7. **Save / load world** to `saves/<name>/` (simple format: size + block bytes or chunk files).  
8. **`button.sh`**: `compile | run | kill | help` — **POSIX `sh`**, no bash arrays.  
9. **Look:** dark sky gradient or sky color, block face shading (not flat unlit cubes), readable GLUT bitmap HUD text, inventory bar at bottom (polished like event-editor mock).  

### Stretch (add if MVP solid)
- Crafting 2×2 → planks, sticks, torch  
- Simple enemies or animals (chase / wander)  
- Day/night cycle + ambient light  
- Water translucent layer  
- Multi-chunk streaming load/unload  
- Tree generation  

### Non-goals (do not boil ocean)
- Full Minecraft multiplayer, redstone, Nether, Java parity  
- Full CHTPM/prisc loop inside this package (document only)  
- Real texture atlas PNGs unless trivial (prefer solid colors + lighting first)

---

## Layout (create this tree)

```text
200.glut-craft/
  PROMPT.md              # this file
  ARCHITECTURE.md        # how it maps to house GL/CHTPM later
  README.md              # how to run
  button.sh              # compile | run | kill | help  (POSIX sh)
  src/
    main.c               # glut setup, loop
    world.c / world.h    # gen, get/set, save/load
    player.c / player.h  # camera, collision, input
    render.c / render.h  # draw voxels, HUD
    inv.c / inv.h        # hotbar
  pieces/ or data/       # optional defaults
  saves/                 # runtime worlds (gitignore-ish ok)
```

Single binary: `glut-craft` or `ops/+x/glut_craft.+x` — pick one simple path: `./glut-craft` next to button.sh.

---

## Controls (document in HUD + README)

| Key | Action |
|-----|--------|
| WASD | Move |
| Mouse | Look (capture on click; Esc release) |
| Space / Shift | Up / down (fly) or jump |
| F | Toggle fly |
| LMB / RMB | Break / place |
| 1–9 | Hotbar |
| Ctrl+S | Save |
| Ctrl+L | Load last / default |
| Esc | Release mouse / menu |
| Q | Quit (or Ctrl+C) |

---

## Visual quality bar (triple-A relative to GLUT)

- Face lighting: top brighter, sides medium, bottom dark  
- Clear crosshair center  
- Hotbar: slots with selected highlight, block color swatch + number  
- Position + FPS top-left  
- Smooth camera (no jitter)  
- Reasonable FOV (~70°)  

If it looks like a 1995 unlit demoscene cube, **fail the visual bar**.

---

## Acceptance for “agent done”

- [ ] `sh button.sh compile` succeeds on Linux freeglut  
- [ ] `sh button.sh run` opens window; walk, look, break, place  
- [ ] Save creates files under `saves/` and load restores  
- [ ] HUD readable; hotbar works  
- [ ] No runaway CPU when idle (glutIdleFunc with timer throttle ~60fps)  
- [ ] README + ARCHITECTURE.md written  
- [ ] No bash-only syntax in button.sh  

---

## Agent rules

- Prefer **one coherent C codebase**, few files, compile with one gcc line + `-lGL -lGLU -lglut -lm`.  
- Do not depend on mutaclysm system/ or prisc.  
- Do not spawn infinite sleep processes.  
- If incomplete, still leave compileable MVP over broken stretch features.  

*End PROMPT.md — glut-craft*
