# rpg-pet - handoff to the next agent (written 2026-10-10 by claude / Sonnet 5.5)

Read first: `HOW-TO-RPG-PET.md` (everything built, in date order), `ROADMAP.md`, the AGENTS.md house rules (commit only your own paths with an
explicit pathspec, never `git add -A`, never touch `xyzfs/users`, never claim done without a fresh build + run + evidence).
Owner rules that apply here: **C or pal, not Python** (and no new `.sh` for window logic); every item ends with a PNG the owner can see +
the click path; free models only; do not move the owner's windows; run heavy work with `nice -n 15 ionice -c3` (weak CPU machine).

Branch `claude`. Commits (newest first): `bbd14e6ab` (pc-hq flicker/size fix + xelector/hero box), `e09943f00` (sprite frames in the 3D export),
`bf6aa2eff` (3D recipe docs), `5e9f11bfb` (Esc, Play mode, export3d), `d95adc242`, `1a3e20961` (Harold), `aa3867b9a` (own pets/wallets/voices/overlays).
`main` is at `aa3867b9a` (fast-forwarded by `git push . claude:main` then `git push origin main`, never by checking out main in the live tree).
`claude` is pushed through `bbd14e6ab`; nothing newer is uncommitted that is mine.

## 1. What exists (all verified with evidence unless marked)
- One C op `ops/rpg_pet.c` (build `sh ops/build_rpg_pet.sh`, binary `ops/+x/rpg_pet.+x`): verb mode, cli_io mode, resident daemon, event-runner mode.
  Window = `rpg-pet.xhtpm` + `rpg-pet.css` on the shared `khtpm_core_render.+x`; launcher `button.sh run`.
- Cast: Harold (DB actor 1, the player/hero, first in the bottom bar) owns six DB monsters Blip Fang Gruk Draxa Gargo Bonz (db-hq ACTOR 19-24,
  SYSTEM `RpgPetParty`), own `entity_uid`/wallet id (`e` + 24 hex, identity only), own edge-tts voices speaking zh/ja/ko/en (`voices.pdl`).
- INT (interact) mode with roguelike turns; Play mode (green GO started / red STOP stopped, Player tab, Save/Load slot 1); Esc leaves INT; chat + bag overlays;
  outside room with day/night; clock in the top bar (lc_clock); shop in a dropdown, rooms dropdown teleports the view.
- `rpg_pet.+x export3d` writes every room as a pc-hq map book `@.apps/piececraft-hq/pieces/system/maps/rpg-pet/<room>/` (gitignored, generated) in the TSOTS format
  (map.txt glyphs, extrusion.pdl, cells.txt/cells.rgba 24 px atlas from RPG Maker tiles, events.txt) and the 16x24 sprite frames under
  `#.NNEST_ASSETS/tsots-characters/frames/`. **Verified:** pc-hq renders the bedroom in real 3D with textures; POV `f` and `1` work through the key relay.
- All 98 TSOTS desks have the full 3D set (map, layers, cells.*, extrusion.pdl 98/98; events.txt 51; parallax 46). No MV3D plugin exists anywhere.

## 2. Open problems, in the order I would fix them

### A. All six pets are stacked on ONE cell and do not move  (seen live 2026-10-10)
State: `state/pet_*.txt` all `room=bedroom x=10 y=3 dir=0 tx=-1 ty=-1`, `steps` constant over 12 s, `flag_running=1`, `flag_int=0`, `flag_ctl=pet`. Daemon pid alive
(`rpg_pet.+x <house> <pkg>`, ~0.7% CPU) but nothing advances. NOT debugged - cause unknown. Start here:
1. `ops/rpg_pet.c` daemon loop (search `next_ms`, `ai_step`, `run_on`, `was_run`): the 2026-10-10 Play-mode change added `if (in || !run_on()) continue;` and
   `clock_sync()`. Check the daemon sees `run_on()==1` (flag file `state/flag_running.txt`), that `next_ms[i]` is being reset and compared with the right
   clock (monotonic ms), and that `ai_step` is not returning 0 forever because the target picked is unreachable (`tx=-1` = no target; check the "pick a new target" branch).
2. Stacking: pets may step onto an occupied cell. `ai_step`/`blocked()` should treat other pets and Harold as blocking (or at least spread on spawn). Check the spawn/`reset`
   placement and the `meet()` relation code; six pets on (10,3) means they were all given the same cell or all walked to the same target.
3. Verify with: `ops/+x/rpg_pet.+x status` twice 10 s apart (cells change), then the window PNG (`dump_frame_png_op.+x 0x<winid> out.png`).
4. Remember the earlier rule: when INT is OFF the daemon walks pets by timers; when INT is ON only your acts advance turns (`end_turn()` steps every non-controlled pet).

### B. The pc-hq 3D camera follows nobody useful
pc-hq's camera follows ITS OWN xelector/hero pieces (`@.apps/piececraft-hq/pieces/xelector_01/state.txt` and `hero_01/state.txt`, currently pos 1,1,20 - the old chess
defaults, z=20 is the pc-hq floor level), not Harold or a pet. Not following anyone = the room appears off to one side; the cyan and green bands the owner sees are the
xelector (cyan) and hero (green/orange) cubes drawn right next to the camera. Fix plan:
1. On `export3d` (or when View opens), write Harold's (or the controlled entity's) cell into the xelector and hero state: `xelector_01/state.txt` keys
   `pos_x pos_y pos_z`, `hero_01/state.txt` same (and `chunk_x/y`). In maker maps the floor top is z=1 in the renderer's box coordinates; pc-hq's own pos_z for a painted desk
   is the page layer - check `bv_render_3d.c` (`maker_ground`, ~line 4373, and the camera anchor `anchor_h = g_xelector_z`, ~line 3192) before choosing a value.
   Look at how a TSOTS desk positions the hero when a book page opens (`pc_generate_chunk.c` spawn write, `desk_*` open code) and reuse it; do not invent a new place.
2. Do it through the host inbox/state, not by editing the renderer: the host inbox is `@.apps/piececraft-hq/pieces/system/widget_cmds/inbox.txt`
   (`CONFIRM_START_MAP:<book>`, `CONFIRM_SET_DESK:<desk>`; `ops/pc_menu_input.c` ~1079/1150 reads them). Check whether a "set xelector/hero position" command exists there
   (grep `xelector_state_path` ~line 1303) before writing the state files directly.
3. The renderer already got one fix (commit `bbd14e6ab`): for maker maps the xelector and hero are drawn with the SAME box as a map-event sprite (0.7 footprint, floor 1.0 to 2.6,
   GPU path in `render_one_frame`). NOT yet seen on screen because the cubes were off-room at (1,8,4). The CPU fallback ray path (~line 3895) still uses the old cube. After
   (1), take a PNG to confirm the cubes match the sprite size and level (owner request: "xelector and other cube should be same size and on same level as sprite").

### C. POV (1-4, 0, qweasdrtfzxcv) does not change anything IN the rpg-pet window
Keys 0-4 and the View button only store a flag/label (`flag_view`, `flag_pov`). The picture in the rpg-pet window is always the flat 2D room. pc-hq's own window DOES change
(verified). Plan (owner wants the pet screen to look like the pc-hq 3D view with sprites):
1. Keep rpg-pet as the producer of the book (`export3d`, re-run when state changes or on View open - cheap, three small rooms).
2. In the rpg-pet window canvas, show pc-hq's frame: the canvas element reads a raw RGBA file via `canvas_raw` (see `pchq_board_projector.c` for how pc-hq publishes
   `bv_session=`, `canvas_raw=`); the 3D frame is `<board-viewer session>/pieces/display/rgb_frame_3d_overlay.raw` (raw RGBA, no header, size = the producer-size file).
   The producer-size file is written ONLY by the pc-hq board window (see D) - a second consumer needs its own size handoff (do not reuse `pchq_board_view.txt`;
   the pet already writes `canvas_view.txt` next to its package) and its own bv_render_3d session/daemon, OR run pc-hq headless and mirror its frame at the pet's size.
   Decide with the owner; the second is cheaper but couples the pet window to the pc-hq engine session being alive.
3. POV keys: with INT on the renderer forwards keys to `state/interact_relay.txt` (codes 1000-1003 arrows, others bare ASCII). The op already receives 0-4 there; forward the
   camera keys (0 1 2 3 4 5 q e r t w a s d c v f z x) as bare decimal codes to the board-viewer session relay
   `&.widgits/board-viewer/pieces/sessions/<id>/pieces/apps/player_app/interact_relay.txt` (found with `ledger_peers.+x`/ps; reference: `BOARD-CONTROLS.md` in the XO reference folder).
4. `0` = 2D<->3D toggle, `1-4` = first/third/free/bird's-eye and they switch to 3D, `` ` `` = Chinese/ASCII glyph view (always 2D). `f`/`5` reset camera, `z x` move the xelector level.
5. The 3D renderer is heavy on this machine: only run the 3D daemon while View is 3D (see "board-viewer 3D perf" in memory).

### D. Things learned the hard way (all documented in HOW-TO too)
- **The 3D frame does not update by itself.** After `CONFIRM_START_MAP`/`CONFIRM_SET_DESK` the canvas stays on the old map until a key is appended to the board-viewer session
  `interact_relay.txt` (e.g. `102` = f). It looked like a failed export for 30 s. Events are cached per desk path: after re-exporting switch desk away and back.
- **`#.desktop/pchq_board_view.txt` belongs to the pc-hq board only.** It used to be written by every canvas window (rpg-pet, pet-trainer), so it flipped between sizes and the 3D picture
  flickered wide/narrow and stayed small. Fixed in `khtpm_core_render.c` (`kh_is_pchq_board()`), and the board now writes its own size even though it is user-resizable. A window keeps its OLD binary until
  relaunched: after rebuilding the renderer (`ops/build_core_render.sh`, run by ABSOLUTE path - it cd's with an unquoted `&`) relaunch every canvas window. Verified stable `2101 948`.
- Esc arrives as the line `KEY_PRESSED: 27` in `state/keyboard/history.txt`; arrows are bare `1000..1003` lines in `interact_relay.txt`; the renderer forwards keys only while the window is focused.
- A `canvas-overlay-*` row with no real `<item>` child is silently dropped by the renderer (that is why chat/bag did not open once).
- `bv_render_3d.c` / `bv_gpu_raymarch.c` have OTHER agents' uncommitted hunks. Commit only your hunks (`git diff -U0 > full.patch`, keep your hunks, `git apply --cached --unidiff-zero`, then `git commit` WITHOUT a pathspec),
  because a pathspec commit takes the whole working file.
- Kill the 3D daemon with `pkill -x bv_render_3d` (exact comm, never `pkill -f`); it respawns on the next frame request with the new binary. Build board-viewer: `sh '&.widgits/board-viewer/scripts/build.sh'` by absolute path.
- Process matching: by `comm` and the window file name; screenshots: `&.widgits/_shared-lib/ops/+x/dump_frame_png_op.+x 0x<hexid> out.png` (x11grab is black here). Windows are found by size, not name.

### E. CPU (checked 2026-10-10, owner asked)
`top` showed four khtpm_core_render windows at 46-65% and a chrome at 100% while pc-hq 3D + rpg-pet + pet-trainer + exchange were open. The rpg-pet window alone idles ~6% with `fixed-size managed user-resizable`.
pc-hq's resident GPU raymarch is the big one; before you add a 3D view to the pet window, measure (`top -b -n 2 -d 3 -o %CPU`) and gate the daemon on View==3D. Exchange-hq was at 50% in that sample - not mine, not investigated.

### F. Later (owner asks, not started)
Shop pictures next to each item; smooth pixel walking tween (RPG Maker js/animation ops - ask the owner for them); village; TSOTS house sampler; own room per pet; pets' DB profiles for the old pet-trainer
animals still describe animals; window UI language switch for zh/ja/ko (`lang/<code>.pdl`); chain wallet bridge for pet wallets (CHAIN-ECONOMY-DESIGN); Harold-owns-monsters purse/lease rule.

## 3. Commands you will need
```
HOUSE=/home/no/Desktop/github/work/NNEST-12.00/x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00
sh "$HOUSE/@.apps/rpg-pet/ops/build_rpg_pet.sh"                 # build
$HOUSE/@.apps/rpg-pet/ops/+x/rpg_pet.+x status|play|stop|int|export3d|select harold|goto outside
sh $HOUSE/@.apps/rpg-pet/button.sh run                          # launch the window (kill the old one first, by comm + file name)
sh $HOUSE/@.apps/piececraft-hq/open_pchq_board.sh $HOUSE        # pc-hq
printf 'CONFIRM_START_MAP:rpg-pet\n'  >> $HOUSE/@.apps/piececraft-hq/pieces/system/widget_cmds/inbox.txt
printf 'CONFIRM_SET_DESK:bedroom\n'   >> $HOUSE/@.apps/piececraft-hq/pieces/system/widget_cmds/inbox.txt
echo 102 >> "<board-viewer session>/pieces/apps/player_app/interact_relay.txt"   # f; forces a re-render
```
Copies of this handoff: XO `22.rpg-pet/`, HQ-IQ-BOOK `08-roadmap/design-docs/RPG-PET-HANDOFF-2026-10-10.md`.

## REQUESTED (not built): minimap of the current room + thumbnails of the other rooms (owner, 2026-10-10)
Owner: "on the top right we want to show a minimap of current map, but also other maps of other rooms (in the space to right where nothing is currently being rendered). The way pc-hq has hud, we can do the same."
- **Where:** the dark area to the right of the 624 px room inside the canvas (top-right corner first). Current room as the main minimap, the other rooms as smaller thumbnails below/beside it, the room the view follows highlighted, pets as coloured dots, Harold as a distinct dot, doors marked.
- **How pc-hq does it (reuse the idea, not the code):** `BV-HUD-TEXT-OVERLAY-AND-MINIMAP.md` (HQ-IQ-BOOK 08-roadmap/design-docs) + `bv_render_3d.c` `bv_draw_hud()` / `bv_draw_minimap()` (~lines 2076-2360): small coloured blocks, about 8 px per cell, top-right by default, corner and size configurable in a `.pdl`, one on/off toggle per HUD element in the menu toolbar, defaults in the `.pdl`.
- **Plan for rpg-pet:** the op already paints the room itself into `state/scene.raw` (the canvas). Add a `hud.pdl` (`HUD | minimap | corner=top-right | cell_px=8 | on`, `HUD | thumbs | ...`), draw the minimaps in the same scene buffer after the room (floor/wall/furniture as flat colours from the same map data `export3d` already walks: glyph per cell W/D/T/f/.), and add Menu toggles (`toggle minimap`, `toggle thumbs`) with flags like `flag_chat`. Clicking a thumbnail = the existing `goto ROOM` verb (the rooms dropdown already does this). Only rewrite when the picture changes (the canvas repaints on file change).
- **Fit check:** the canvas is 916 wide with the room at 624, so about 290 px are free on the right; at 8 px per cell a 13x8 room is 104x64, so the current room plus two thumbnails at 4 px fit. If rooms are added, wrap thumbnails in a grid.
- **Also useful later:** show the same minimap in the 3D view inside the rpg-pet window (pc-hq already draws one in 3D; the existing minimap follows the xelector).
