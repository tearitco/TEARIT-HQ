# HOW-TO-RPG-PET (house copy of XO/22.rpg-pet/RPG-PET-SPRINT.md)

The full design, research (INT, xlector, POV keys, TSOTS corpus numbers), sprint order and pitfalls are in `/home/no/Desktop/github/work/XO/22.rpg-pet/RPG-PET-SPRINT.md`; the text below is the same document, kept in step.

Files: `toy.pdl` (toys menu), `button.sh` (launcher), `rpg-pet.xhtpm/.css` (window), `catalog.pdl` (shop), `rooms.pdl` (rooms + doors), `ops/rpg_pet.c` + `ops/build_rpg_pet.sh` (the op; verbs and resident mode are in its header comment), `state/` (gitignored: house.txt ledger, pet.txt, events.txt, flags, chat).

---

# 🏠🐾 rpg-pet — sprint plan and handoff (2026-10-10)

Written FIRST, before the house copy, so the sprint is organised in one place. The same content, with file paths checked against the repo, lives in
`44.xyz.01.00/@.apps/rpg-pet/HOW-TO-RPG-PET.md` (design + how-to) — keep the two in step.

## 0. The ask (owner, in order)
1. Remake the pet game as a NEW toy `rpg-pet` in the taskbar **toys** menu. **Leave the old pet-trainer untouched.**
2. **RPG Maker tile sets only.** Show an RPG Maker starting house and an RPG Maker sprite. Items from the RPG Maker sheets are bought and placed as decorations / functional items.
3. The screen is a **2D + 3D RPG screen with the pc-hq POV system**, including the "Chinese characters" view.
4. TSOTS has **~100 levels in 2D and 3D**: use them as the *diffusion* (training corpus) for how to create a house.
5. The houses use **the same doors and teleports as the other pet game**, "so it's a bit of a new thing".
6. Needs **INT (interact) mode**, the **xlector**, and the **1 2 3 4 / 0 POV keys** like the pc-hq window.
7. Needs **chat** and a **bottom tb** (footer) with view / window / hotbar, just like the other pet.
8. Document everything for the next agent: in the house AND here in XO.

## 1. DONE this session (verified)
| What | Evidence |
|---|---|
| (later in the session) Rooms + doors (`rooms.pdl`), pet walking by itself through doors with one-tile event steps (`state/events.txt`), INT arrows move the pet, Shop and Rooms dropdowns (Rooms moves the pet), C op `ops/rpg_pet.c` replaces the Python | events ledger shows STEP/DOOR rows while INT is off; renderer 6% CPU, daemon 0.2% |
| Toy `rpg-pet` exists: `@.apps/rpg-pet/` (`toy.pdl` makes it appear in the toys menu by file presence; the scan reads `<root>/<dir>/toy.pdl`) | window opened, 900x560 |
| House room drawn ONLY from RPG Maker MV sheets: Inside_A5 (ceiling, wall face, floor), Inside_B (furniture), Actor1 (the sprite, facing-down standing frame) | PNG of `state/scene.raw`, looked right |
| Shop catalog `catalog.pdl` (14 items, price, sheet, col, row, size, kind floor/wall, effect) | buy flow tested: 6 items bought, a 400c piano refused with 170 coins |
| Append-only ledger `state/house.txt` (BUY/PLACE/MOVE/REMOVE), state replayed each call; collision + wall rules; pet cell and the door cell reserved | ledger replay |
| **CPU:** the window idled at **40-47%** until the template class list got `fixed-size managed user-resizable` (pet-trainer has it). After: **6%**. | measured 8 s windows: 47% -> 6% (pet-trainer 10%) |

Not done yet: the xlector (placement cursor), POV keys 1-4 (stored only), 3D, the glyph view, TSOTS-derived house layouts, the toys-menu click path (launch by `button.sh` only so far). **Known bugs:** the chat overlay and bag hotbar do not draw over the canvas (the footer view/window/hb buttons do) - compare with pet-trainer, which has the same rows; shop rows have no pictures yet (owner ask: a picture next to each item - the renderer's item `sprite=` attribute is the route, format not yet confirmed); walking is a one-tile jump per step with the 3-frame walk cycle, no pixel tween (owner asks whether to use the RPG Maker walk/animation ops: the frames are used, the movement timing from rpg_core.js is not).

## 2. Research: what the controls ARE (read from the repo, not guessed)
Sources: `@.apps/piececraft-hq/BOARD-CONTROLS.md`, `@.apps/pet-trainer/CAMERA-DESIGN.md`, memory `pchq-camera-pov-keys`, code `&.widgits/board-viewer/ops/bv_menu_input.c`.

**INT = Interact mode.** A toggle in the toolbar (`INT on/off`). While armed, the window forwards the keyboard and mouse to the engine; off, keys do nothing to the game.
- Every key except Enter/Esc goes to `interact_relay.txt` (`<code> <ms>` per line); Enter (13) and Esc (27) go to `keyboard/history.txt` only (the parser state machine). Writing both double-fed the camera ("double-arrow bug"). Esc leaves INT.
- pet-trainer implements the same arming (`pet_event.sh interact`, `interact_armed.txt`, vars `bv_h1`/`bv_h2` in the tab `relay=` attribute).

**The xlector** (the owner's spelling "xelector"): the board **cursor**, a piece that is not the hero. Arrows move it on the board plane; `z`/`x` move it down/up a z-level; `8` snaps it back to the hero. **Enter** on an entity's cell *possesses* it (if its `piece.pdl` has `possessable`, default yes); `9` releases, and `9` again re-possesses the last one (mutaclysm `choice.c` parity). Game verbs (space JUMP, g MINE, h BUILD, / END_TURN, . autotick, , tick speed) only fire while possessing.

**POV / camera keys** (resolved `keybinds.pdl` -> `arrow_config.txt` -> built-in default; all remappable):
| key | action |
|---|---|
| `0` | toggle 2D <-> 3D |
| `1` | first person | `2` third person (default `camera_mode`) | `3` free roam | `4` bird's eye. Pressing one switches to 3D. |
| `q` `e` | yaw left/right | `r` `t` pitch down/up | `c` `v` camera height down/up |
| `w` `a` `s` `d` | pan | `f` or `5` reset the camera to the mode default |
| backtick | always the **2D Chinese / ASCII terminal view** (`view_2d_style=ascii`) |
Control mapping follows the *player's* perspective (left key = left on screen), not the maths (lesson from the a/d strafe fix).

**"Chinese characters POV"** = the backtick view: the 2D map is drawn with one glyph per cell, the CJK ones rasterised by `&.widgits/board-viewer/ops/bv_cjk_glyph.c` (FreeType, Noto Sans CJK; first font path that opens wins). The TSOTS desks carry both pictures: `cells.png/.rgba` (tile atlas) and `map.txt` (one glyph per cell: `W` blocked, `~` water, `.` floor).

## 3. The corpus: TSOTS (the "diffusion")
- 94 maps on disk (`maps/tsots/map001..` plus `db/`), `game.pdl` lists 98 desks. Each desk: `layers.txt` (6 planes of real RMMV tile ids, row-major), `map.txt` (glyph plane), `cells.png/.rgba` + `cells.txt` (atlas for the 3D renderer, tile_px 24), `extrusion.pdl` (`EXTRUDE | W | 3` = walls are 3 high), `events.txt` (`x y r g b charset index dir pattern`), `ev/` (event pages), `parallax.*`, `bar.txt`.
- Original data: `/home/no/Desktop/🤖️🪤️🏠️/xv.*/RMMV_TSOTS.../www/data` (Map###.json, MapInfos, Tilesets). Painter: `&.widgits/tsots-map-convert/tsots_paint.py` (RMMV Tilemap algorithm incl. autotiles, registry `&.widgits/palettes/tilesets/tileset_registry.pdl`).
- Measured here: **36 of 94 maps use an interior tileset** (Inside / SF Inside / SF Inside mouse over); interior sizes **13x15 to 56x50**, median area 950 cells; the five small ones (<= 20x20) are `clone wing` 17x20, `clone room 2` 17x13, `clone bathrooms` 17x13, `Item Shop` 13x15, `RollerCoin Room` 17x13. Most-used interior tiles are floor tiles around ids 3200 (A2 autotile), 1544/1586/1537 (A5 floor/wall rows), 6032 (B layer furniture), 2528/2816 (A1/A2).
- "Diffusion" here means **statistics first, generation second**: learn from the 36 interiors (footprint size, wall thickness, where the door is, furniture density per square, which tiles sit next to each other) and sample a house from that. A first cut needs no neural net: a constraint/template sampler seeded by those counts, validated by the same collision rules the shop uses. A real diffusion model is a later, optional layer (same pattern as tomom: a plain rule is always the fallback).

## 4. Design: how the rpg-pet screen works (target)
**One scene model, three views.** The house is a `pieces/` board (the pc-hq model): tiles from `layers.txt`-style planes + entities (the pet, furniture with an `effect`, doors). Then:
- **2D RPG view** (default): what is drawn today, from the planes (v0 draws straight from `catalog.pdl` placements).
- **3D POV view** (keys 0-4): export the board in the TSOTS desk format (`layers.txt`, `cells.png/.rgba`, `extrusion.pdl`) and call the existing board-viewer 3D op (`bv_render_3d`, GPU daemon `--daemon`). Do NOT write a second renderer. Run it only while 3D and INT are on, `nice -n 15`, because the 3D daemon is heavy here.
- **Chinese-glyph view** (backtick): the `map.txt`-style glyph plane through `bv_cjk_glyph`.
The pet-trainer decision stands: **rooms stay side-on 2D by default**; 3D/POV is for the house-as-map, property and village. For rpg-pet the owner now wants the 2D/3D pc-hq screen, so make 3D an opt-in (`0`), 2D the default.

**INT + xlector inside rpg-pet.** Reuse pet-trainer's `interact` verb and `relay_poll` (it already stores camera keys into `state/camera.st`) and add the missing half: *something reads `camera.st` to draw*. The xlector is the placement cursor: arrows move it, Enter on an owned item places it (replacing "first free spot"), Enter on a placed item picks it up, `z`/`x` change floor when houses get stairs.

**Doors and teleports (the "new thing").** The other pet game has them (`rooms.pdl` doors, `world:` door events like `door home`, `village`, view switches). In rpg-pet a door is a **tile + an event**: the tile is the RPG Maker door sprite, the event is the pet-trainer door contract (touch -> change `view`/map, set INT per page). One shared contract file so both games read the same door rows. Reserved already: the door cell (bottom middle) is kept free by `check_place`.

**Chat, hotbar, footer** exactly like pet-trainer (`pet-trainer.xhtpm` is the template): `canvas-overlay-right` chat row (6 `chat_N` lines + a `cli_io`), `canvas-overlay-bottom` hotbar row (`hb_n_slots` slots, `hb_title`, show/hide), and the `<footer class="pchq-footer">` with toggle buttons: **view** (2D/3D/glyph), **window** (chat), **hb** (hotbar). Vars: `chat_visible`, `hb_visible`, `hb_n_slots`, `hb_N_text`.

## 5. Sprint order (each item ends with a visible before/after PNG, per the owner's evidence rule)
1. **Chat + hotbar + footer** in `rpg-pet.xhtpm` (copy the pet-trainer rows; verbs in `rpg_pet.py`). Proof: PNG + relay-typed message appears.
2. **INT toggle + xlector** (arrows/Enter place and pick up). Proof: relay keys, before/after PNG of a moved item. Remember relay testing can mask real focus bugs — owner hardware check.
3. **Scene data** (`pieces`/`layers.txt` planes) so 2D is drawn from data; shop writes the data.
4. **Door tile + door event + teleport** between two rooms using the pet-trainer door contract.
5. **TSOTS house stats** script (`ops/tsots_house_stats.py`): footprints, door positions, tile adjacency from the 36 interiors; commit the numbers as `data/house_stats.pdl`.
6. **House sampler** from those stats; harness `rpg_pet` (pal + cases) checks: every generated house passes `check_place`, has one door, fits the size band.
7. **3D export + POV keys 1-4/0/q-e/r-t/c-v/f** via board-viewer; CPU measured idle and in 3D.
8. **Backtick glyph view.**
9. Toys-menu click path verified by relay; commit; then item functions (`sleep/eat/read/play`) as events; purse -> chain bridge later (coins are paper today).

## 6. Pitfalls found this session (the "wisdom")
- A khtpm window with a `<canvas>` and without `fixed-size managed user-resizable` burned **40-47% CPU**; with them **6%**. Always measure: read `/proc/<pid>/stat` utime+stime over 8 s, after 6 s of settling. `perf`/`strace` are blocked (ptrace/paranoid) — bisect the template instead (remove an element, measure).
- The renderer repaints a canvas only when `canvas_raw` (the var NAME matters) changes size/mtime, plus a 1 Hz safety repaint. Write the `.raw` atomically and only when the picture changed.
- In `sh -c` rows and paths, the `&` of `&.hq-apps` is a shell operator: quote every house path.
- khtpm windows have no X name. Find by size: `xdotool search --onlyvisible --class ""` + `getwindowgeometry`. `ffmpeg x11grab` of the screen returned BLACK on this Wayland session; use the house's relay/`dump_frame_png_op` or compare state files instead (the dump op errored on the first try — not yet resolved).
- Never match processes with a full-command-line grep from a tool shell (it kills the shell): match `comm` + the window file name.
- Toys menu = `toy.pdl` file presence in a direct child of the house root, `@.apps`, `&.widgits` or `&.hq-apps`; label from `title`, launcher from `launch` (default `button.sh`).
- **The house writes C or pal, not Python** (owner, 2026-10-10: "were in c or pal, not python"). rpg-pet was first a Python v0; it was ported to `ops/rpg_pet.c` (stb_image from `&.widgits/_shared-lib`) and the Python deleted. Python is only acceptable for one-off converters (`tsots_*.py` precedent), never for window logic.
- A `<module>` is launched as `<house> <pkg>` and its `id=` attribute becomes a 4th argument, so a module with an id gets argc 4.
- `reset` must never delete a ledger (I wiped the owner's furniture once): archive it as `house.txt.<ts>.bak`.

## 7. Open questions for the owner
1. Is the old pet's pet (Rin etc.) the sprite of rpg-pet, or a new Actor sheet character per pet?
2. Same wallet/purse as the old pet game, or a fresh 500-coin purse (v0 is a fresh paper purse)?
3. Door contract: share `rooms.pdl` with pet-trainer, or copy it? (Recommend share, one file.)
4. First 3D target: the room in 3D, or the house + garden from above?
