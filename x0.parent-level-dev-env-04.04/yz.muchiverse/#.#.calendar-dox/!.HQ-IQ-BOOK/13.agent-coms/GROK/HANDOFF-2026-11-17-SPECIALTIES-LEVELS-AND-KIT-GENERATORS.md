# Handoff to Grok, 2026-11-17: your specialties (levels, reusable harnesses, kit-file generators)

From Claude (Sonnet 5.5), after the owner said: "u fix the minimap if its a problem and hand it its own specialties like it suggested". Read after `HANDOFF-2026-11-16-WHERE-WE-ARE-AND-BACK-TO-LEVELS-AND-MAP-CONTROLS.md`. Labels: READ = in a file I opened, RAN = I ran it and saw it, DOC = my proposal, UNKNOWN = I did not check.

## 1. What I finished so you do not redo it (RAN, all on branch `claude`, commit `028e248d7` and earlier)
- Corner map click-to-move in 3D AND 2D (mode 0), free camera recentre, per-book setting `VIEW | minimap_click_possessed | ignore|release`, xelector limit = page size (not 256), first frame drawn at the window own size, a relaunch reopens the page you were on. As-built: `16.game/CENTROID-TEST-AND-LIVE-2026-11-06.md`, last section. **Do not take the 2D corner-map slice: it is done.**
- Harness `minimap_fit` is 70 checks; run it before and after anything that touches `bv_render_2d.c`, `bv_render_3d.c`, `bv_menu_input.c`.
- Engine fixes reach an open board only through a NEW session (pitfall 33), and a fresh session draws frame 1 at the wrong size unless the engine waits for the window size file (pitfall 35, fixed). Never judge a fix on a window you did not relaunch.

## 2. Your lane (your own proposal in the room, confirmed by the owner's instruction to me)
Centroid levels, and Minecraft-style, DF-style and CDDA-style clones. **Every one must be a reusable harness plus a generator that reads a kit file** (your words in the room). Rules that make it fit the house:
1. Data, not C per game (decision D62). The existing generator is `game_new_op <house> new <name> --title T --kind ledger|realtime|editor` (READ, `&.widgits/_shared-lib/harness/ops/game_new_op.c`, templates under `harness/templates/game/<kind>/MANIFEST.pdl`). A kit file is the same idea one level up: a `.pdl` that says what the level or clone contains and is read by ONE shared generator. UNKNOWN: whether a "kit file" format already exists; I found no file by that name. **Propose the format in a short doc and get the owner's yes before building** (design-choice test, `02-architecture/DESIGN-CHOICE-TEST-AND-DECISION-LOG.md`).
2. A level is a painted page: `cells.txt` header (width, height, tile_px 24, atlas_cols 16, atlas_tiles), cell = `floor,wall,...` with the atlas slot as the id, `cells.marker` growth triggers the reload (CENTROID doc, minimap sections). Pages up to 512 wide are what the corner map and clicks now handle.
3. A harness that can fail: pal + case `.pdl` + C ops, **no Python, no new `.sh`** (`&.widgits/_shared-lib/harness/README.md`). Negative-test every assertion once, and photograph (`dump_frame_png_op`, open the PNG).
4. Scratch house under `/tmp` for generation; never write into the live editor or `rmmv-www-img`; new levels only, never replace (owner rule).
5. Start heavy things with `cpu_gate`, one at a time; never loop `ffmpeg`/`awk` over pixels.
6. Big levels: `centroid:live` is 504 x 364 and was the stress test for the corner map; `vis_new_op` is NOT built (UNKNOWN whether you want it first).

## 3. Working agreement
- Branch: your own (`grok`), started from the `claude` tip in a scratch worktree; do not touch the live checkout or the owner window (relaunch only with the owner's yes).
- Files you may edit freely: new level data, kit generators and their harness cases. Files that are mine for now (shared with the minimap): `bv_render_3d.c`, `bv_render_2d.c`, `bv_menu_input.c`, `bv_minimap_fit.h`. If a level needs a change there, post the request in the room instead of editing.
- One line per step in the room (what you read or wrote and the path); no "done" without a fresh run and a picture.
- Old rooms are indexed in `13.agent-coms/COLAB-SESSIONS-INDEX-2026-11-17.md`; the 10-29 calibration session is the model for how to state what you did and did not observe.
