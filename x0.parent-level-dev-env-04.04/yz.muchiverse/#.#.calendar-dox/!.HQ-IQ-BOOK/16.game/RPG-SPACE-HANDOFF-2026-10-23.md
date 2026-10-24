# rpg-space handoff, written 2026-10-23 for the week of 2026-10-26

Branch `claude`. Everything below is committed. Nothing here is "done" unless it says what was run. "NOT done" lists are as important as the built lists.

Read first: `RPG-DUST-PETSLIKE-BOARD-DESIGN.md` (this folder) sections 11 to 16, and `XO/30.rpg-dust-work/RPG-DUST-PROGRESS-REPORT.md`.

## 1. What exists now

**rpg-space** (`@.apps/rpg-space/`) is a renamed copy of rpg-dust, kept so rpg-dust stays the reference. It is a pets-like board with a hovering ship.

| Piece | State | Evidence this session |
|---|---|---|
| Four ship rooms (bridge, quarters, hold, engine), doors on the inside, per-room decoration | built | all six crossings walked, both ways |
| Planets (Maker World, Cata Prime, Cubia, Hellrock) and No planet (deep space) | built, flat 16x16 sheets | teleport via `fire planet <id>` |
| Ship hovers a fixed 8 levels above any planet; planet stays whole under the hull | built today | `worldscope` elevation + side screenshot of Hellrock, solid lava under hull |
| Engine lights per desk (`lights.pdl`: AMBIENT, LIGHT rows) | built | 6 light checks pass; unlit render byte-identical to the pre-lights shader |
| Event voxels outlined orange while In: is off (`hotspots.txt`, doors + the console) | built, partial | doors complete; the console outline is fragments |
| Left bar scrolls when it does not fit (class `ov-scroll`) | built | live in a 1260x556 window |
| Hero wanders on his own when In: is off | built today | still 12 s with In: on, 4 cells in 20 s after Esc |
| Shared skin library `petslike_skin_lib.c` | built | rpg-edit-hq output byte-identical (27 files) |
| Orbit physics op `dust_orbit` (two suns, Kepler period) | built | 27 checks; not yet shown moving on a minimap |

## 2. The engine changes made today (read before touching the board engine)

- **Sunk surfaces.** In a desk's `extrusion.pdl`, `EXTRUDE | <glyph> | -N` (N 1..16) now sinks that glyph's top N levels below the floor top. Zero and positive values behave as before.
- **A second solid run under a column.** `cells.txt` tokens 4 to 6 per cell, `F,Wl,D,U,UD,US`: tile slot U, hung UD levels below the floor level, top US levels below the floor top. Needed because a column is otherwise one contiguous run, so a hull floating over a planet could not keep the planet's centre under it. GPU path only. `BvGpuScene.under` carries the slots; the shader reads a second block of rows of `u_cells`.
- **Shader tweak.** The top face of a sunk column keeps its own tile (the rock tile only applies to side faces).
- Gate used: build the shader from before the lights commit (`git show f42386b3b^:...`), render the same scene with `bv_gpu_light_test`, `cmp` the two raw files. Passing means the plain picture did not change.

## 3. Tools built today (use them, do not re-derive)

`&.widgits/board-viewer/tools/`, build with `sh build_<name>.sh`, binaries land in `tools/+x/`:

- **worldscope** `worldscope.+x <desk_dir> cell|row|col|exposed|warn ...` prints a desk the way the engine builds it (side elevation, '#' solid, '=' second run, '_' hole). Applies the engine's column rules offline, so a geometry question needs no screenshot. `desk_dir` holds `map.txt`, `cells.txt`, `extrusion.pdl` (export with `rpg_space.+x export3d`, files are under `rpg-space-pchq/pieces/system/maps/rpg-space/<room>/`).
- **bv_keys** `bv_keys.+x <session_dir> <text>` presses keys in an engine session (camera q e r t c v, z x level, f reset). `\n` is Enter, `\e` is Escape. The session dir is `bv_session=` in `@.apps/rpg-space/board/state/ui.txt`.
- **int_trace.txt** in the app's `state/`: one line per engage or leave key the daemon sends (`engage-13` / `leave-esc`). Added for the unreproduced stray-Enter case below.

## 4. Open work, in the order I would take it

1. **Helm / flight mode (requested, not started).** Interacting with the bridge console should make the arrow keys move the ship instead of the xelector until Esc. Needs: a helm event page for the console hotspot (bridge cell 8,4), a `state/flag_helm.txt`, key routing in the daemon, a ship position in the ledger. Design in sections 13 and 13b of the design doc (one shared space desk, planets as islands). Ask the owner whether to build a minimal version on the bridge first or the full shared space desk.
2. **Ship height "5 rows in deep space vs 3 above planets" (reported, NOT reproduced).** In a 1260x556 window the hull measures identical for none/cata/hell in pixels, and `worldscope exposed 8 13 0 1` gives 5 solid levels on the south face in both. Need the owner's two screenshots plus window size. Before that, rebuild the engine and look again: the cause may have been the old 1-level hover, now 8.
3. **Console outline is fragments.** The console is a solid block in a row of blocks in front of the wall. Needs a taller or wider box, or moving the console.
4. **Ship and planets as entities with own maps and bottom-bar entries** (design section 15): footer entries first, then the voxel model entity, then stop merging the planet into the ship desk, then a harness.
5. **Galaxy as the BOOK of the header, rooms as its pages** (owner: mirror the LyfeDesk taskbar when in doubt). Was built in `5e7759a81` and reverted in `858b89ba5` as a precaution after two hard resets. Re-apply on a clean base with CPU logged to disk.
6. Orbits visible on the minimap (engine reload without resetting the view is the unsolved part).
7. Small: right bar (chat) scroll, thumb drag, check a window big enough that no bar shows, outline people and `events.txt` events.

## 5. Unexplained problems (do not assume they are fixed)

- **Two hard machine resets** at 17:20:29 and 17:30:13 on 2026-10-23, no cause logged, within minutes of launching the galaxy header. Header reverted; cause unproven.
- **`bv_state.txt` lost three lines** in the engine session (`focused_project_id`, `focused_project_root`, `emoji_mode`), leaving only `selector_x/y`. The engine then says "No project focused yet" and ignores every key while In: still shows ON. Restoring the lines fixed it live (no restart). Looks like two writers overwriting each other; the owner was testing at the same time as I was. Check this file first if controls die. Saved copy of the broken file: `/tmp/claude-1000/bv_state.broken` (may be gone).
- **Stray Enter after Esc** seen once: INT went to 0 but an Enter re-engaged the engine. Not reproduced in five clean trials. `int_trace.txt` will show who sent it.
- **Xelector drifts above 17** (`pos_z` 19 seen). The rpg skin is meant to pin it at 17 when In: turns on; the pin did not pull it back. Looks like Harold is "not snapped" to it when this happens (cells match, the box floats).

## 6. Gotchas that cost time this session

- **A rebuilt engine only shows after the whole session ends.** The engine session of a previous launch is reused, so end everything (button.sh run-widget, `bv_render_3d --daemon`, prisc) before relaunching. A relaunch after a partial kill can hang at "loading". Match the cmdline AND the `PCHQ_PKG` env when killing; the engine daemon's cmdline is relative.
- **Redraw nudge is the plain line `126`** appended to `<bv_session>/pieces/apps/player_app/interact_relay.txt`, not `KEY_PRESSED: 126`. Force a desk reload by touching `maps/<book>/<room>/cells.rgba`.
- **Two key files, two consumers.** The camera/xelector reads `interact_relay.txt` (bare decimal); the In: mode state machine reads `pieces/keyboard/history.txt` (`KEY_PRESSED: N`). Enter (13) is an ordinary key inside In: mode (it means possess at the xelector); only Esc (27, to the keyboard history) leaves it. A real X Esc in the window is handled in `khtpm_core_render.c` `handle_key`; the per-pid relay file does not take the same path.
- **Do not drive the window while the owner is testing**, and do not steal focus with `xdotool windowfocus` without saying so.
- **Quote paths with `&` and `@`.** An unquoted `&.widgits/...` backgrounds the command; a `git add` did exactly that today and committed nothing. Always `git commit -- '<path>' ...` with every path quoted, and read `git show --stat` afterwards.
- **`grep ' error'` is not a build check.** Use `grep -E 'error:|^OK'` and look at the binary's mtime.
- Unclosed `/* ... */` in my own C edits bit twice. The compiler's complaint can be far from the cause.
- Do not touch `xyzfs/users`, do not push `user/*` branches, do not `git add -A`.

## 7. How to reproduce the state (from `44.xyz.01.00/`)

```
sh '&.widgits/board-viewer/tools/build_worldscope.sh'; sh '&.widgits/board-viewer/tools/build_bv_keys.sh'
sh @.apps/rpg-space/ops/build_rpg_space.sh
sh @.apps/rpg-space/button.sh                       # the window (1260x556 default)
@.apps/rpg-space/ops/+x/rpg_space.+x fire planet hell      # none | cata | hell | maker ...
@.apps/rpg-space/ops/+x/rpg_space.+x export3d
'&.widgits/board-viewer/tools/+x/worldscope.+x' <desk_dir> col 8 0 15
```

## 8. Commits of 2026-10-23 (all on `claude`)

`f42386b3b` engine lights, `dbeef56c0` ship lamps, `5e7759a81` galaxy header (reverted by `858b89ba5`), `b8b1f4bad` left-bar scroll, `f2e3b6210` design section 15, `387ff06f7` orange event outlines, `b5730bc4e` sunk surfaces and under-run plus the two tools, `5dc3bb84c` hero wander plus `int_trace.txt`.

## 9. Not rebuilt

df-hq, cdda-hq, mcl-hq and the other petslike skins compile with the shared core change (checked with `-lm`) but were not rebuilt or relaunched. They pick up hero wander and the trace on their next build.
