# Cursword-driven desktop 3D + piececraft in-scene desks — design (2026-08-29)

**Status: DESIGN ONLY, nothing built.** Direct instruction: "this is a bigger
pass, so lets write a specific document... this will be a long term
feature." Three real, connected pieces of work, captured together because
they share one mechanism (piececraft/board-viewer's real 2D/3D + camera
system) even though they land in two very different places (piececraft's
own setup flow, and the house desktop).

## 0. The three real pieces

1. **Remove piececraft-xyz's blocking pre-setup screen**, replace it with a
   real in-scene screen where files/desks can be picked/changed, rendering
   INSIDE the piececraft scene itself (not a separate gating modal).
2. **Port piececraft/board-viewer's 2D↔3D + camera-mode + z-level system
   onto the house desktop itself** - desktop entities (pals/tiles) gain a
   real 3D-extruded voxel view option, same visual language piececraft
   already uses for its own emoji-as-voxel entities.
3. **Cursword becomes the desktop's own real interact controller** for (2):
   a plain click (not right-click, not drag) arms it with a glowing neon
   blue "halo," starts a real key-recording session (Escape ends it),
   arrows move cursword, and POV/camera keys drive the 2D/3D + z-level +
   camera-angle switch for desktop entities.

## 1. Real precedent this design reuses (not inventing from scratch)

- **`&.widgits/board-viewer/ops/bv_menu_input.c`** - the real, live, already-
  working camera system. `1`-`4` = camera_mode (gated digit keys, same
  pattern board-viewer already enforces - see file's own `is_pov_key`
  handling ~line 703). `9` (`key_possess`) toggles xelector possession of
  the hero, with a real "reverse jump" back to the cursor's pre-possess
  position on release. Z-level movement is `x`/`z` (hero) vs `c`/`v`
  (camera) - two independent axes, confirmed live and documented in
  `PIECECRAFT_XYZ_DESIGN.md` §3 (resolved 2026-08-29, this same day).
  **This is the real, closest analog for what cursword's own key-recording
  session should drive** - not reinvented, ported.
- **Voxel/3D-extrusion pipeline**: `pieces/registry/emoji_assets/<hex>/
  voxels_16.csv`, real, shared, already generic (`get_voxel8_cached()`/
  `sample_voxel8_pixel()`, `&.widgits/board-viewer/ops/bv_render_3d.c`
  ~line 990) - the SAME real asset chtpm_rgb_render's own on-demand
  emoji-to-voxel pipeline already produces for 2D emoji rendering too, so
  a desktop entity that already renders as an emoji (every real pal/tile
  today) already HAS a real voxel asset sitting on disk, generated the
  same way. "3D extruded like the emojis in piececraft" is therefore a
  real, already-proven visual language, not a new one to invent.
- **Real house desk-persistence system** (just built this session, see
  `TILE-PLACEMENT-DESK-PERSISTENCE-GAP-2026-08-29.txt`) - `xyzfs/users/
  <uuid>/home/livedesk/sessions/<id>/desks/<name>.pdl`, real `DESK` rows,
  read back by `khtpm_taskbar_manager.c`'s `livedesk_spawn_desk()`. This is
  almost certainly the real data piece #1's in-scene files/desks screen
  should read/write directly, given it's the exact same "files/desks" the
  house already uses everywhere else - not a new, piececraft-specific
  desk concept.
- **`cursword` itself** - a real, existing desktop pal (`xyzfs/users/<uuid>
  /home/livedesk/pals/cursword/`, glyph 🗡️, real AI chat/history/ledger like
  every other pal - self/asa/ava/m8_redhorned). NOT the same thing as
  mutaclysm/piececraft's own `xelector`/cursword-as-block-cursor concept
  (`CURSWORD-SOUL-VISION.md`, 195 lines, checked - contains no camera/3D/
  z-level/halo content, confirmed this is genuinely new ground, not
  something already speculated there). This design REPURPOSES the real
  desktop pal as the desktop's own equivalent of piececraft's xelector -
  same pattern reused, wholesale, at a different layer (desktop instead of
  in-game board).
- **`*.monads/*.livedesk-taskbar/ops/tp_desktop_window_rgb.c`** - every real
  desktop entity's own renderer/event-loop. Real, existing click dispatch
  (confirmed ~line 3312-3341): **left-click (button 1) currently means
  drag-to-move** (press arms `dragging=1`, release snaps to the 80px grid
  and writes `desktop_pos.txt`); right-click (button 3) opens the real,
  data-driven context menu. See §3 open question #1 - this is a REAL
  conflict piece #3 has to resolve, not a clean slate.

## 2. Piece 1 - piececraft's in-scene files/desks screen

Real, current, blocking flow (`PIECECRAFT-LOCAL-VERIFY-2026-08-29.md` §1-2,
live-verified this session): `ATLAS-EDITOR [new_game]` setup screen (world
type chooser) → `Confirm & Start` → `Enter Game` - two full nav actions,
gating, before anything else is visible. Direct user framing: this "doesn't
allow for easy debug."

Real, proposed replacement (not designed in detail yet - real open
questions in §3): a screen that renders WITHIN the already-running
piececraft scene (not a separate blocking modal before the scene exists at
all) where the real house `desks/<name>.pdl` files can be browsed/switched,
the same way the taskbar's own "user/file/desks" dropdown already works
(`khtpm_taskbar_manager.c` ~line 1281+, real precedent). Loading a desk
into piececraft's own world_01-equivalent state should reuse
`livedesk_spawn_desk()`'s real read-DESK-rows-and-spawn pattern conceptually,
even if piececraft's own world storage isn't literally the same file
format (its own real chunk/voxel storage, per `PIECECRAFT_XYZ_DESIGN.md`).

## 3. Piece 2 + 3 - cursword-driven desktop 2D/3D + camera

### 3a. The real interaction, as specified directly

- Plain click (button 1, NOT right-click, NOT a drag) on cursword arms it:
  a glowing neon blue "halo" circle renders around/under it as the real,
  visible armed-state indicator (same real principle as this session's own
  RMMV tiled-overlay-window "amber tint = armed" convention, or the
  Settings picker's `.pal-hint-armed` bright-yellow title - a proven house
  pattern: visible state, not a silent flag).
- While armed, real key capture begins (same real X11 `KeyPress`
  mechanism every other desktop entity window already uses,
  `tp_desktop_window_rgb.c`'s own event loop) and continues until real
  `Escape`.
- **Arrow keys** move cursword itself (real desktop-grid movement, likely
  the same 80px `GRID_CELL_PX` every entity already snaps to).
- **POV/camera keys** (exact keys TBD, likely the same `1`-`4`
  camera_mode convention board-viewer already proves, per §1) switch
  between:
  - **2D mode** (current, default desktop rendering - flat emoji sprite
    per entity, unchanged).
  - **3D extruded mode** - desktop entities render via the real voxel
    pipeline (§1), with real z-levels (entities gain a Z axis, camera
    angle becomes a real, chosen 3D view rather than a fixed top-down
    flat sprite).

### 3b. Real, unresolved open questions - genuinely undecided, not just unwritten

1. **Left-click already means "drag" for every desktop entity today**
   (`tp_desktop_window_rgb.c` ~line 3312, confirmed real, existing
   behavior). A plain click on cursword needs a real way to tell "this was
   a click (arm halo)" from "this was the start of a drag" - the standard
   real fix is a movement-threshold check (ButtonPress records start
   pos, ButtonRelease within some small pixel radius AND short time =
   click; otherwise treat as the existing drag). Needs an explicit
   decision on the threshold, and on whether cursword ALSO still supports
   normal dragging (probably yes, both should coexist) or gives up
   drag-to-move entirely in favor of arrow-key movement once armed.
2. **Scope of the 2D/3D switch**: does armed-cursword's camera-key press
   switch the WHOLE desktop's entities to 3D at once (a real, desktop-wide
   mode flag, likely simplest and most consistent with how a single
   camera/POV concept should work), or per-entity (each pal keeps its own
   independent 2D/3D state - more flexible, much harder to reason about
   visually, likely wrong)? Leaning desktop-wide, not decided.
3. **Where does the "desktop-wide 3D scene" actually render?** Today,
   every desktop entity is its OWN independent X11 window
   (`tp_desktop_window_rgb.c`, one process per entity, confirmed via this
   session's own `ps aux` output showing one process per tile/pal). A real
   3D view with a shared camera angle/z-level likely needs either (a) a
   NEW, separate compositor-style process that owns the real 3D
   raymarch/render pass across ALL entities at once (closest analog:
   `bv_render_3d.c`'s own per-session raymarch, but board-viewer only ever
   renders ONE focused project's board, not N independent desktop windows
   at arbitrary screen positions) or (b) each entity's own existing window
   independently re-renders itself in "3D mode" using its own real
   position + a SHARED camera-state file (angle/z-level, e.g. under
   `#.desktop/`) that all of them poll - closer to the existing
   one-process-per-entity architecture, no new compositor process needed,
   but each window's own 3D render would need real depth-sorting/
   occlusion logic to look correct next to its (still 2D-window-shaped)
   neighbors. **Not decided - this is probably the single largest real
   architectural fork in the whole design**, and needs a real answer
   before any code starts.
4. **Does the halo replace cursword's own emoji sprite, or render as an
   overlay/ring around it?** Overlay/ring is the more consistent choice
   (matches the "amber tint window" and "bright title text" armed-state
   precedents in §3a, which never replace the underlying content) but not
   confirmed.
5. **Where does the key-recording session's own real state live** (a
   `cursword_armed.txt`-style file under `#.desktop/`, matching this
   session's own `rmmv_armed.txt`/`debug_watch_enabled.txt` convention;
   or something scoped inside cursword's own pal dir)? Not decided - needs
   to be visible/pollable by whatever ends up owning the desktop-wide 3D
   render (see open question #3).
6. **Real key list for POV/camera** - board-viewer's own `1`-`4` are
   already meaningful/used elsewhere on the desktop (likely house-wide
   digit-dispatch conventions exist for other things) - needs a real,
   confirmed-non-colliding key set, not just borrowed wholesale from
   board-viewer without checking.

## 4. Suggested next step

Once this doc is reviewed: resolve open question #3 first (the real
architectural fork) since it determines almost everything else's shape,
then #1/#2/#4/#5/#6 can likely be settled together in one focused pass.
Piece 1 (piececraft's own setup-screen replacement) is comparatively
self-contained and could start independently of pieces 2/3 resolving,
since it doesn't depend on the desktop-3D architecture question at all.

## 5. REAL UPDATE (2026-08-30) - today's board-window menu ask, and how it connects

Direct instruction today, connecting straight into Piece 1/2 above: add a
board-window menu with a "File" header (load/change level) and a "Desk"
header (change map within a level) - i.e. exactly Piece 1's "in-scene
files/desks screen," now scoped concretely to the khtpm board window
built this same day (see `PIECECRAFT-HQ-BOARD-KHTPM-CONVERSION-2026-08-30.md`).

Real facts checked today, filling in Piece 1's "not designed in detail
yet":

- **Desk `.pdl` format today** (`DESK | name | pal_path | px_x | px_y |
  col | row | emoji | z_index`) is flat 2D icon-placement - no grid,
  terrain, entities, or z-*levels* (game layers). Cannot hold a full
  piececraft board as-is.
- **Piececraft's own real board storage today**: `pieces/system/chunks/
  chunk_<x>_<y>/chunk_<x>_<y>_z<N>.txt` (flat per-z-level terrain grids)
  + `pieces/world_01/{animals.txt, phymoji_entities.txt, state.txt}`
  (entities/state). Only one world (`world_01`) exists on disk; the real
  `select_world.chtpm`/`${world_list}` multi-world mechanism already
  exists in the layout but is currently unused with just one world.
- Direct instruction on the format-unification question (hybrid,
  either works): a piececraft level should be **loadable/saveable in a
  format compatible with the general desk system**, so a piececraft
  board can eventually be viewed as a desk (and vice versa - Piece 2/3's
  desktop-3D work), while still being able to hold everything a board
  needs (tiles, entities, z-levels, events). Real, open design
  question to resolve before building: does a desk row become a
  pointer to a separate real board file (extending the existing chunk
  format with a name/save-slot concept), or does the `.pdl` format
  itself grow a new row family that can carry full board data inline?
  Direct instruction: **design the shared format first, build second** -
  no save/load code should land before this is actually decided on paper.

## 6. REAL PROPOSAL (2026-08-30) - concrete hybrid format, awaiting sign-off

Per "hybrid, either should work" - a proposal that gets both properties
(desk-compatible AND able to hold a full board) without cramming grid
data into flat `.pdl` rows:

- **A board's heavy data stays in its own real directory**, reusing
  piececraft's EXISTING chunk-file convention verbatim (no new terrain
  format to invent): `desks/boards/<board_name>/chunk_<x>_<y>_z<N>.txt`
  + `entities.txt` + `state.txt` (same real shape as today's
  `pieces/system/chunks/` + `pieces/world_01/`, just relocated under
  the real house `desks/` tree instead of living inside one project).
- **A new, real `.pdl` row type sits in the SAME desk files DESK rows
  already live in** (or a sibling `<name>.board.pdl` in the same real
  `desks/` dir, discoverable by the same directory sweep):
  `BOARD | <board_name> | desks/boards/<board_name> | <cols> | <rows> |
  <z_min> | <z_max> | <emoji_thumbnail>` - a lightweight pointer/
  manifest row, same real shape as a DESK row (name, path, a few
  dimensions, a thumbnail glyph), not the grid itself.
- **Piececraft's new "File" menu** = list real `.pdl` files that
  contain BOARD rows (i.e. real desks that have at least one board) -
  picking one loads that context.
- **Piececraft's new "Desk" menu** = list the BOARD rows WITHIN the
  currently-loaded file - picking one loads that specific board's
  chunk/entity data into the live session (world_01-equivalent state).
- A regular (non-piececraft) desktop desk can freely mix ordinary DESK
  rows (pals/tiles) and BOARD rows (piececraft levels) in the same
  file, since they're just different row types in the same real
  pipe-delimited format the parser already reads line-by-line.

**Not yet started - awaiting explicit confirmation this is the right
shape** before any save/load code is written, per "design the shared
format first, build second."

## 7. REAL CONSENSUS RECORD (2026-08-30) - grilled and confirmed, still no code

Direct instruction: "grill me till u are sure there is no deviation and
record the results in plan first; this is a big deal." Below is what was
actually confirmed, point by point - nothing here is built yet.

1. **Hierarchy confirmed**: "File" picks a **level** (the outer
   container/whole game-world folder). "Desk" picks a **map** within
   that already-loaded level (one board/chunk-set). Two-tier, nested -
   not independent, not the same thing at different zoom.

2. **Only File and Desk are new nav rows.** Direct instruction: "file
   and desk is enough." Editor is NOT a new nav row - it already routes
   through the real, existing `&.widgits/palettes/pallets.pdl`
   `piececraft` category (`PICKER=minecraft`, block-palette picker) that
   "View Editor (opens separate GL window)" already uses today. Nothing
   new needed there.
   - Real correction/addition, confirmed: that `minecraft` picker's
     real intended tile source is `#.NNEST_ASSETS/mc_extracted_csvs_8x8`
     (confirmed on disk - real per-block CSV subfolders, e.g.
     `bedrock/`, `bricks/`, `cactus_top/`). Direct instruction: "were
     just using emojis for now cuz we haven't done that yet" - the real
     Minecraft-tile integration for this picker is itself a separate,
     not-yet-done piece of work; piececraft's boards currently render
     with emoji tiles as the real placeholder. Not part of the File/
     Desk scope here - recorded so it isn't confused with it later.
   - Real, explicit note for later (documented now even though deferred,
     per direct instruction: "we may add more things later like
     db/plugins {but mapped to that specific file/map, not the desktop
     global}"): any future per-level metadata (a database, plugins, etc)
     must be scoped to the specific loaded file/map, NOT a
     desktop-wide/global palettes category - a real constraint on how
     any future palettes integration for this must be wired, so it
     isn't accidentally built desktop-global by default the way the
     earlier (reverted) wrong-scope attempt was.

3. **Real default fixtures, both formats, both live in piececraft-hq's
   own copy folder** (not the shared user-level desks tree - these are
   ship-with-the-app fixtures/test data, not a live user's own desk).
   Direct instruction: "there should be a default in the piececraft-hq
   copy folder, it should be in legacy format and the correct format
   (.pdl?) and be called default-legacy & default-pdl." Concretely:
   - `default-legacy` = the CURRENT real chunk_0_0 + world_01 data,
     copied as-is, untouched format, under piececraft-hq's own tree.
   - `default-pdl` = the SAME default map, authored/converted into the
     real new §6 format (a BOARD row + `desks/boards/<name>/` directory
     reusing the chunk-file convention), also under piececraft-hq's own
     tree. This is a real, deliberate CONVERSION of the actual default
     content into the new format now, not a deferred wrap-only shim -
     supersedes the earlier §6 "wrap existing default as-is" framing
     for the DEFAULT specifically (still means brand-new saves always
     use the §6 format going forward).

4. **Events are confirmed new scope, not built now.** No existing
   event/trigger system tied to piececraft boards was found - this
   stays a real, separate, later design item, not a blocker for File/
   Desk work now.

5. **Nav mechanics for File/Desk** (not separately re-confirmed this
   round, carried over from the earlier answer this session): real
   numbered rows in `board_viewer.chtpm`'s own legacy nav list (same
   `[>]`/`[ ]` convention Interact Mode already uses), each opening a
   real picker/submenu of levels (File) or maps-within-the-current-
   level (Desk) - not a separate header/menu-bar UI construct.

## 7b. Real menu-pattern grounding (2026-08-30) - checked against known debt

Direct instruction: "we can use the same menu type. we dont have to
reinvent anything." Checked the real, existing taskbar menu mechanism
(`yz.muchiverse/#.#.✅️.cal-user-sum/1.^V-hq/TASKBAR-MENU-ARCHITECTURE.md`,
a real, live doc - NOT lost, just one directory level up from where I
first searched) before copying anything:

- `livedesk_build_file_menu()`/`livedesk_build_desk_menu()` (the
  taskbar's own File/Desk) are **C-hardcoded** - a real, documented,
  CONFIRMED-WRONG regression (§"Standing refactor debt", 2026-08-24
  update: "user confirmed none of the cells' builders are supposed to
  be C-hardcoded"). These are NOT the pattern to copy.
- `livedesk_build_hq_menu()`/`livedesk_build_palettes_menu()` ARE the
  real, correct pattern - they read `hq_menu_N_label/cmd` /
  `palettes_menu_N_label/cmd` rows from `#.desktop/livedesk_taskbar.pdl`
  live, at cell-open time, no recompile needed for a new row.
- Direct instruction, given this: "we may do them similar at first but
  may need to refactor into layouts after, that was supposed to be done
  already" - build piececraft's File/Desk on the CORRECT
  (data-read-at-open-time) shape from the start, not the deprecated
  hardcoded-C one, even though it's a different engine/process
  entirely (legacy `chtpm_parser_pal`, not `khtpm_taskbar_manager.c`).
- Real, already-aligned mechanism to build on: `select_world.chtpm`'s
  own `${world_list}` (populated by `select_world_module.pal` scanning
  a directory at render time) is ALREADY this same correct shape - data
  read live, not a hardcoded branch list. File/Desk should extend this
  exact mechanism (list levels/maps by scanning real directories/BOARD
  rows at menu-open time), not invent a parallel hardcoded list.

## 7c. REAL PROGRESS (2026-08-30) - taskbar's own File/Desk fixed first

Direct instruction: "maybe we should fix tb file/dek first then?"
Done, committed (`89fb13c2`): `livedesk_build_file_menu()`/
`livedesk_build_desk_menu()` converted from C-hardcoded to the correct
PDL-driven pattern (`file_menu_N_label/_cmd`, `desk_menu_action_N_label
/_cmd` rows in `#.desktop/livedesk_taskbar.pdl`), matching
`livedesk_build_hq_menu()`/`_palettes_menu()` exactly. Desk's real
desk-name list stays a directory scan (genuinely data, not a
violation). Live-verified via `nav.sh` - both cells still work, now
reading real config instead of compiled-in strings. This closes the
`TASKBAR-MENU-ARCHITECTURE.md` debt item for these two specific cells
(others - user/player/db/pals/toys/clock/ai - remain unconverted,
out of scope for this pass).

## 8. Real next steps (still no code until this section itself is acted on)

1. Create `default-legacy` (copy chunk_0_0 + world_01 verbatim) and
   `default-pdl` (author the real §6 BOARD-row + directory equivalent)
   under piececraft-hq's own tree.
2. Add real "File" and "Desk" nav rows to `board_viewer.chtpm`'s own
   layout/pal script, each opening a real picker listing what §7.3's
   fixtures (and any future saves) provide.
3. Wire "Desk" selection to actually swap the live session's active
   chunk/entity data to the picked map's files.
4. Everything else in this doc (desktop 3D, cursword controller, events)
   stays explicitly deferred, unchanged from §3b/§7.4 above.
