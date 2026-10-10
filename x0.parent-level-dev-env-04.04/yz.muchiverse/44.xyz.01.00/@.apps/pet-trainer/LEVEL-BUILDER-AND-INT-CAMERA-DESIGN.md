# Pet house: build bar, room items, and real-map levels with a working INT camera (brainstorm + design, 2026-10-10)

Status: DESIGN. Nothing here is built. Extends `CAMERA-DESIGN.md` (2026-10-09, same gap) and `PET-VILLAGE-GAME-DESIGN-2026-10-09.md`. Written by claude from the owner's message and the code as it is today.

## 1. Owner asks (2026-10-10)
1. "another bar that can toggle visible or in bottom tb."
2. "modify the map by adding items to rooms from a menu of items to add to empty rooms."
3. "'INT on' does not yet change the camera POVs. We want to build the level in a real map, and use the Doom / TSOTS levels as inspiration for how to use tiles and design a 3D level."

## 2. What exists (read, not assumed)
- **INT keys go nowhere visible.** `interact_relay.txt` -> `pet_manager` `camera_apply` writes `state/camera.st` (mode 2d/3d, pov 1-4, yaw, pitch, height). **Nothing reads it**: `ops/pet_scene.c` draws flat 2D scenes from hard-coded `rect()` calls. This is why INT on changes nothing.
- **Rooms are code, not data.** `rooms.pdl` lists rooms, doors (teleport events) and platforms, but each room's *look* is a scene id (0 bedroom, 1 living, 2 garden, 4 roof) drawn inside `pet_scene.c`. A new or empty room has nothing to draw.
- **The mini map** is `home.pdl` (`CELL | room | x | y | kind`), toggled with key 5.
- **Bars today:** top tab row (`<tabbar>`, INT/book/page/clock/Menu/Player), left sidebar (nav), and the bottom row of party tabs (`canvas-overlay-bottom`, the hotbar). Generic overlay classes `canvas-overlay-right/bottom/ov-top` exist in `khtpm_core_render.c`. A bar is just a `<row>` of `<item>`s fed by `ui.txt` (`n_x`, `x_N_label/verb/arg`), the same way `nav_N` and `menu_N` rows already work.
- **Doom book** (placeholder rooms): a *desk* = a text grid with a glyph legend (W wall, f floor, D door, S start, E/I monsters, K key, H health, A ammo, X exit), `extrusion.pdl` for wall height, `events.pdl` rows (`trigger=player-touch cmds=...`), atlas `cells.txt`+`cells.rgba`. 1 cell = 32 map units in the converter plan.
- **TSOTS** (RPG Maker levels): layered tiles from an atlas, parallax, characters as 16x24 frames, events as pages (`tsots_book.py`, `_layers.py`, `_event_pages.py`).
- **The 3D engine exists:** `board-viewer` `bv_render_3d` (CPU) + `bv_gpu_raymarch` daemon (~17 fps live) already implement POV 1-4, yaw, pitch, height for pc-hq boards. Heavy on this machine: run only when `mode=3d` and INT is on, niced.
- **Owner ruling 2026-10-09:** "side-ways view is fine" for rooms. The new ask (build levels in a real map, POV that works) widens that; see question 1.

## 3. The core idea: one map, many views
A pet room (and later the property and village) becomes a **desk in a pc-hq book `pet_home`**, exactly like a Doom or TSOTS level:

```
 maps/pet_home/
   game.pdl                 book info (File menu entry)
   <room>/map.txt           grid, glyph per cell  (e.g. 16 x 10 for a room)
   <room>/extrusion.pdl     wall/furniture heights (3D)
   <room>/events.pdl        doors = teleports (already exist), furniture use, ASIC miner, bed...
   atlas: cells.txt + cells.rgba   floor/wall/furniture tiles
```
- **2D side view** = the current look, now *drawn from the map* (a render mode, not hard-coded scenes). Keeps the owner's side-on rooms.
- **2D top-down** = the village/property look (TSOTS style).
- **3D** = `bv_render_3d` on the same desk. INT on + camera keys finally change what you see.
- **Furniture and items are events** (Doom things model): a glyph in the grid plus an `events.pdl` row. Billboard sprite or extruded box, the renderer already supports both.
- **Doors stay what they are** (on-touch teleport events from `rooms.pdl`); they just become map cells.

Lessons borrowed:
| From | Take |
|---|---|
| Doom | tiny glyph legend per level; walls via extrusion height; things = events; one cell = a fixed world size; level state in `state.pdl`, rules in `tunables.pdl` |
| TSOTS | layered tile atlas (floor / wall / object layers), parallax backdrop (the sky/moon already drawn), event pages with conditions, 16x24 character frames |
| House | everything is a file; ledger rows for every placement; renderer stays generic (zero per-app C in `khtpm_core_render.c`) |

## 4. The extra bar (ask 1)
A **Build bar**: a row of buttons (item categories + a selected-item preview) that can be shown or hidden.
- **Where:** a generic `canvas-overlay-*` row inside the pet window, toggled by a verb `bar_build on|off` (state `bar_build=` in `ui.txt`, `<repeat>` like `menu_N`), plus a button in the top tab row. Cheap, no renderer change.
- **Or** a cell in the **bottom taskbar (tb)**: needs the taskbar's cell/menu registration (not read yet; the pc-hq hotbar is the precedent).
- Content: categories (Furniture, Machines, Plants, Decor), items of the chosen category, remove tool, rotate. Keys 1-9 pick slots, like the hotbar.

## 5. Adding items to empty rooms (ask 2)
- **Data:** `furniture.pdl`: `ITEM | kind | category | w | h | cost | glyph | sprite | events`. Placed items per room: `rooms/<room>/items.pdl` (`PLACE | kind | cx | cy | rot | ts | by`), append-only; removal is another row.
- **Verbs** (all `pet_event.sh`, all ledger rows): `place_item <room> <kind> <cx> <cy>`, `remove_item`, `move_item`. Rules: only in cells that are floor and empty; the room must be one the pet owns or is visiting with permission; costs coins (later chain cones) via the economy `spend` path.
- **Empty rooms:** a room with no scene yet (new rooms built in the town economy) opens as bare floor/walls from its map and is furnished from the bar.
- **Menu flow:** Build bar -> pick category -> pick item -> click a floor cell in the canvas (relay mouse event) -> `place_item`. A ghost preview shows validity.
- **Ties to the chain plan:** an **ASIC miner** is a furniture item with slots per room (`CHAIN-ECONOMY-DESIGN.md`): placing one is how a pet adds mining capacity, wear and maintenance are events on it. Auction/NFT items (grown food, decor) can be placed too.

## 6. INT on actually changing the camera (ask 3)
1. Scene from data (section 3) is the prerequisite: the renderer needs a map to look at.
2. `pet_manager` already stores `camera.st`; add "render with the camera": `mode=2d` -> software scene; `mode=3d` -> call `bv_render_3d` (or the GPU daemon) for the active desk, nice'd, into the same `scene.raw` (660x430), same atomic write.
3. POV: 1 first person (the pet's eyes), 2 third person (follow cam), 3 free roam, 4 bird's eye, per the table in `CAMERA-DESIGN.md`; yaw/pitch/height keys as pc-hq.
4. CPU rules: 3D only while INT is on AND `mode=3d`; frame rate capped; fall back to 2D under load.

## 7. Build order (each step ends with a PNG + click path, per the owner's evidence priority)
| # | Step | Proof |
|---|---|---|
| 1 | `pet_home` book: convert the 4 existing rooms to desks (map + atlas + doors) and make `pet_scene` draw a room from its map (2D side view identical to today) | before/after PNG per room, harness: scene from map matches golden |
| 2 | `furniture.pdl`, `items.pdl`, verbs `place_item/remove_item/move_item` + harness | harness cases, ledger rows |
| 3 | Build bar (toggle verb + overlay row + tab button), place by click | PNG of bar, click path |
| 4 | 3D: `mode=3d` calls the board renderer for the active desk; POV/yaw/pitch/height keys change the picture | 4 POV PNGs of the same room |
| 5 | Empty/new rooms open as bare maps; furnish them from the bar | PNG sequence |
| 6 | Village/property as top-down desks; level editing in 3D (drop items with the camera) | PNG + click path |
| 7 | Doom/TSOTS-style level tooling: import a TSOTS or Doom desk as a pet room/dungeon, pets walk the real levels | PNG |

Lane rules: claude owns the renderer/camera; data lanes may write maps and events; do not touch `bv_menu_input.c` unless doing placer/camera work.

## 8. Questions for the owner
1. **Rooms in 3D?** You said side-on is fine for rooms, and now want POV to work. Recommended: both from one map (side view stays default; INT + `0` switches to real 3D). OK?
2. **Build bar home:** inside the pet window (fast, no renderer change) or a cell in the bottom taskbar (needs taskbar work)? Recommended: pet window first, taskbar cell later.
3. **Scale:** how big is a room (cells)? Suggested 16x10 for rooms, 40x30 village (matches the village design).
4. **Who may furnish:** only your own pet's rooms, or visited rooms with permission?
5. **First 8 items** for the menu (suggest: bed, table, chair, plant, lamp, shelf, ASIC miner, rug)?
6. **Level source:** hand-drawn pet rooms first, or import a TSOTS/Doom level as the first real 3D test map?
