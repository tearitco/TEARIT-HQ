# CRITIC_REPORT — 200.glut-craft

**Critic:** harsh triple-A visual / gameplay pass (freeglut + Minecraft mental reference)  
**Date:** 2026-07-28  
**Build tested:** `sh button.sh compile` → **OK** (`gcc -std=c11 -Wall -Wextra -O2`, clean)  
**Runtime:** `DISPLAY=:0`, `timeout 3 ./glut-craft` → exit **124** (ran full window lifetime; GL radeonsi/DRI3 up). Process starts.  
**CPU sample (idle in window, no input):** **~106–116% of one core**, ~89 MB RSS. Timer is *not* an unlimited idle spin, but you still melt a core redrawing the world every frame.

---

## Verdict: **NEEDS_WORK**

Not **FAIL** — this is a real, compileable, click-and-fly voxel toy with break/place, HUD, save files, and face lighting.  
Not **SHIP** under any honest “AAA / Minecraft-class / polished freeglut demo” bar. It is a **competent weekend student MVP** that still looks like solid-color GL cubes with a dark UI overlay. Claiming AAA would be marketing fraud.

MVP checklist from `PROMPT.md` is **mostly checked**. The *spirit* of the visual bar is **skimmed, not owned**. Performance and input polish are the sharp edges that keep this out of “ship it and stop apologizing.”

---

## Visual — **4.5 / 10**

### Why not lower
- Face lighting exists and is correct in the cheap classic sense: top **1.0**, ±X **0.8**, ±Z **0.7**, bottom **0.5** (`render.c`). Grass gets a greener top and dirtier sides. This is **not** unlit 1995 demoscene sludge.
- Linear fog toward `RENDER_RADIUS` gives a depth cue and softens the pop-in cliff a little.
- HUD is intentional chrome, not an afterthought: semi-transparent top-left panel, FPS + position, fly/capture state, status line, **crosshair** with a faint dark outline, bottom **hotbar** with slot chrome, color swatch, number, yellow selected stroke, block name under the bar, help strip.
- FOV **70°**, double-buffer + depth + back-face cull. Window 1280×720. Target block gets a wireframe cube outline.

### Why not higher (brutal)
- **Flat solid materials only.** Every block is a single RGB. No atlas, no biome tint variation, no AO, no smooth lighting, no specular, no sun direction (shades are axis-fixed, not light-vector). Next to Minecraft or even a half-decent freeglut tech demo with a texture atlas, this reads as **“colored LEGO.”** PROMPT allowed no PNG atlas; that excuses absence of textures, **not** absence of more lighting craft (AO, directional sun, sky gradient mesh).
- **Sky is `glClearColor` baby blue.** PROMPT allowed “sky color” OR gradient; you picked the laziest legal option. No horizon band, no sun disc, no skybox. Feels like a clear-color lab sample.
- **Immediate-mode brutality:** every exposed face is its **own** `glBegin(GL_QUADS)` / `glEnd()`. With `RENDER_RADIUS 48` you walk a ~97³ neighborhood, touch potentially hundreds of thousands of surface faces, and spam the driver. That is not a visual architecture; it is a denial-of-service against your own GPU/CPU. A polished freeglut demo batches or builds a display list / VBO once.
- **Leaves are fully opaque** for culling (`is_opaque` = not air) but **non-solid** for physics. Visually they look like solid green cheese blobs; no alpha, no cutout, no two-pass. Trees exist (stretch feature — fine) but look like Minecraft Classic 0.0.1 after a hangover.
- **Hotbar swatches are flat 2D rectangles** with a 4px “top highlight.” Readable, yes. “Polished like event-editor mock / AAA chrome,” no. No 3D item icon, no count stack, no slot bevel depth that sells weight.
- **Help strip collides spatially with the hotbar zone** (both bottom). Readable text, cluttered layout. Status string hardcodes `saves/default/` even when `--save` differs.
- **Selection outline** is a dark wire cube — OK for debug, weak for game feel (no break crack stages, no face-only highlight, no placement ghost).
- Compared to Minecraft: no hand, no bob, no particles, no block break animation, no water, no shadows, no day/night. Compared to a polished freeglut demo: no mesh cache, no nice sky, no post, no font beyond GLUT bitmap Helvetica.

**Bottom line:** clears the “not unlit junk” floor. Lives on that floor. Does not climb.

---

## Gameplay — **5 / 10**

### What works (mechanically)
- First-person look (click capture, Esc release, warp-to-center).
- WASD move, fly toggle `F`, jump in walk mode, break LMB / place RMB with ray reach ~6 and adjacent place cell.
- Hotbar 1–9 creative palette (grass/dirt/stone/wood/sand/cobble/leaves/planks).
- AABB collision with axis separation; soft world bounds; spawn on surface after gen/load.
- Terrain: value-noise FBM heightmap, stone/dirt/grass/sand layers, sparse trees, bedrock-ish y=0.
- Save/load: `saves/<name>/world.bin` (1 MiB raw) + `meta.txt` — boring and correct. Pre-existing `saves/default/` loads (seed 99 in meta).
- Creative infinite blocks: fine for MVP sandbox.

### What is rough or wrong
- **Default `flying = 1`.** Minecraft-class first contact is feet on grass and gravity fear, not spectator drone. Starting in fly is a tutor mode cop-out that also hides walk/jump bugs from the player (and the developer).
- **Shift-to-descend is a kludge.** GLUT does not give you a clean “Shift held” stream; `g_shift_down` is only refreshed on key events, then forced onto `keys['c']`. `p->special[0]` is a comment-shaped lie (Shift is not special key 0). Holding Shift alone while already moving is unreliable. README promises Shift/C; only **C** is trustworthy.
- **Leaves have no collision** (`solid_at` / `world_is_solid` skip them). You can walk through canopy like a ghost. Inconsistent with fully opaque leaf rendering.
- **Place-inside-self guard is a fuzzy sphere**, not a real AABB-vs-block test. You will still get weird embeds or false refusals at edges.
- **Instant creative break/place** — no mining time, no tool, no inventory counts, no drop entities. Acceptable for MVP; empty as a *game*.
- **Raycast is fixed step 0.05**, not grid DDA. Works until it doesn’t (grazing corners, thin misses). Fine for toy reach; not robust.
- **No feedback loop of progression:** no crafting (stretch), no survival pressure, no sound, no damage, no animals. After two minutes of placing cobble towers you have finished the product.
- **Fixed 128×64×128** with hard soft-clamps — “infinite-ish” is marketing; you hit invisible walls. Fog hides the void somewhat; edge still feels like a diorama in a fish tank.
- **CPU at ~100%+** while “idling” in the window: the sim is cheap; the **draw** is not. Acceptance said throttle ~60 fps / no runaway idle. You avoided `glutIdleFunc` spin (good) but still **runaway render cost**. On weaker machines this will drop below 60 and feel sticky. That *is* gameplay.

**Bottom line:** the verbs exist (walk/fly/look/break/place/save). The *feel* is prototype. Input edge cases and creative-default fly keep it from feeling intentional.

---

## Architecture / agent-friendliness — **7.5 / 10**

### Strong
- **Layout matches PROMPT** exactly: `main / world / player / render / inv`, docs trio, POSIX `button.sh`, single binary `./glut-craft`.
- **`button.sh` is real POSIX `sh`:** `case`, no bash arrays, `compile|run|kill|help`, `set -e`, `DIR` via portable `dirname`. Agent-friendly.
- **One gcc line** with `-lm -lGL -lGLU -lglut`. Clean under `-Wall -Wextra`.
- **ARCHITECTURE.md** is actually useful: freeglut today → file-mediated `world.bin` later, module map, non-goals, bridge sketch without implementing house stack. That is the right compromise.
- **Save format is boring on purpose** (`raw_u8_xyz`) — agents can mmap and inspect without a decoder cult.
- **No muta/prisc dependency.** Self-contained. Correct call for iteration speed.
- Headers are small and readable. Block IDs centralized in `world.h`. `block_color` / `block_name` shared by HUD and voxels.

### Weak / agent landmines
- **Render path will punish the next agent** who adds water/AO without first adding meshing. There is **zero** mesh cache, chunk mesh, or dirty-flag rebuild. Touching `render.c` without a batching plan is a performance trap.
- **`inv.*` is a 9-byte joke module** — fine for MVP, but the name promises inventory systems that are not there (no stacks, no backpack, no pickups).
- **Shift / modifier handling split across `main.c` and `player.c`** with a dead `special[0]` path — next agent will “fix fly down” three times.
- **Status strings and save name** not fully parameterized (UX lie for multi-slot).
- **No automated headless test** for gen/save/load (would need a tiny `--headless` or unit harness). Agents must open a window to trust behavior.
- **Trees + soft leaves** are stretch features embedded without a design note in code about collision policy.

Still: for an agent dropped cold into the folder, **this is easy to compile, easy to find, easy to extend the sim**. Hard to extend the *look* without a render rewrite. Score stays high for structure, not for graphics architecture.

---

## Must-fix before claiming AAA

1. **Stop redrawing the planet with per-face `glBegin`.** Chunk meshes or at least a rebuilt vertex array / display list when blocks change. Without this you will never clear a performance bar or free CPU for sim/AI.
2. **Directional lighting + ambient occlusion (even 0–3 neighbor AO) + sky gradient (or simple sky dome).** Face multipliers alone are “homework complete,” not AAA.
3. **Default to walk/gravity;** fly is a toggle for power users, not the title screen.
4. **Fix vertical fly input:** reliable crouch/down (C always; Shift via sticky modifier tracking on every timer tick if you must). Delete the `special[0]` cargo cult.
5. **Leaves policy:** either translucent/cutout + no collision (and don’t occlude like stone) **or** solid opaque leaves. Pick one and render accordingly.
6. **Real place collision** = proposed block AABB vs player AABB, not a 0.7 magic distance.
7. **Hotbar / HUD layout:** separate help from hotbar; don’t hardcode default save path in status; optional 3D block preview in slot.
8. **Prove idle CPU** under the acceptance text: timer throttle + cheap draw so a parked window is not a space heater.
9. **Block break/place juice** (even minimal): ghost block on RMB aim, face highlight, tiny break flash — otherwise it never feels like a game, only a voxel editor.
10. **Do not claim “Minecraft-class”** until at least one of: textures, day/night, or non-creative loop. Right now it is **Minecraft-shaped**, not Minecraft-class.

---

## Nice-to-have

- Texture atlas (even 16×16 nearest) — single biggest visual jump under freeglut.
- Day/night + ambient light curve.
- Water (alpha + no collision / swim).
- 2×2 crafting stretch from PROMPT.
- Sound (place/break/step) — freeglut won’t save you; OpenAL or a beep is still juice.
- True DDA raycast; multi-chunk streaming as world grows.
- Hand/item in view model.
- `--headless-gen` / save verify for CI agents.
- Mesh rebuild only for dirty chunks; frustum cull before face tests.
- FOV/sensitivity config file (pdl-ish, house-aligned).

---

## What already works (fair)

| Area | Status |
|------|--------|
| `sh button.sh compile` | Clean success, warnings-free with `-Wall -Wextra` |
| Window / GL bring-up | Starts under real DISPLAY; double buffer + depth |
| Timer loop ~16 ms | No `glutIdleFunc` busy-wait loop (architecture correct; cost still high) |
| World size | 128×64×128, ~1 MiB, allocated once |
| Terrain gen | FBM heightmap, layers, beach sand, trees |
| Movement | WASD, look, fly toggle, jump+gravity path exists |
| Break / place | Ray target, outline, hotbar block id |
| HUD | FPS, pos, crosshair, hotbar select, help, status |
| Face lighting + fog | Present; passes “not unlit cubes” floor |
| Save / load | `world.bin` + `meta.txt`; load-on-boot path works |
| Docs | README controls, ARCHITECTURE house bridge, PROMPT alignment |
| Dependency surface | Pure C + freeglut/GLU; agent-compileable |
| CLI | `--seed`, `--save`, `--saves-root`, `--help` |

No compile fix was required for this review. No game rewrite performed (per critic mandate).

---

## Scorecard (summary)

| Axis | Score | One-liner |
|------|------:|-----------|
| Visual | **4.5** | Lit solids + HUD chrome; still colored air-quotes Minecraft |
| Gameplay | **5** | Verbs work; feel is prototype; fly-default + Shift mess |
| Architecture / agents | **7.5** | Clean package and docs; render model is a future crime scene |
| **Verdict** | **NEEDS_WORK** | MVP-shaped freeglut sandbox — do not stamp AAA |

---

## Critic’s last word

You built the **skeleton** of a voxel game and dressed it in the **minimum legal outfit** from the prompt (face shades, hotbar, fog, timer). That is honest engineering for day-one freeglut. It is also **miles** from Minecraft and a notch below what a prideful freeglut demo looks like when someone cares about batching and sky.

Ship it as: *“agent-friendly voxel lab / house GL bridge seed.”*  
Do **not** ship it as: *“Minecraft-class AAA freeglut game.”*

Next agent: **mesh the world or stop.** Everything else is lipstick on an immediate-mode pig.
