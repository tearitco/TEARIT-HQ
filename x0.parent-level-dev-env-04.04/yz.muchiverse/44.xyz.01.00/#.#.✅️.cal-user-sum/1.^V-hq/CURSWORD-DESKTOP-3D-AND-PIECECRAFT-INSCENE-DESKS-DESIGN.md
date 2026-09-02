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
(`yz.muchiverse/#.#.calendar-dox/1.^V-hq/TASKBAR-MENU-ARCHITECTURE.md`,
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

1. ✅ DONE (2026-08-30) - Created `@.apps/piececraft-hq/defaults/
   default-legacy/` (verbatim copy of `chunk_0_0` + `world_01`, parity
   confirmed via real `diff -rq`, zero differences) and `defaults/
   default-pdl/` (`default.pdl` - a real `BOARD` manifest row - pointing
   at `desks/boards/default/`, holding the same real chunk/entity
   files relocated per §6's convention). See `defaults/README.md`.
2. Add real "File" and "Desk" nav rows to `board_viewer.chtpm`'s own
   layout/pal script, each opening a real picker listing what §7.3's
   fixtures (and any future saves) provide.
3. Wire "Desk" selection to actually swap the live session's active
   chunk/entity data to the picked map's files.
4. Everything else in this doc (desktop 3D, cursword controller, events)
   stays explicitly deferred, unchanged from §3b/§7.4 above.

## 9. REAL DECISION RECORD (2026-08-30) - open question #3 resolved,
   A/B kept as a documented fallback, remaining open questions

Direct instruction: "definately note a/b optionality in docs incase we
ever run into problems with a."

**Open question #3 (§3b) is now decided: Option A.** Each desktop
entity's own existing window (`tp_desktop_window_rgb.c`) re-renders
itself in 3D mode independently, driven by a new shared camera-state
file under `#.desktop/` - no new compositor process. Real reasoning,
direct from the user: since every desktop entity is already its own
separate, transparent-background X11 window, there is no true unified
"scene" for a compositor to blend in the first place - inter-entity
depth is already just normal WM window-stacking, not real 3D
compositing between neighbors, so Option B's real payoff (centralized,
correct cross-entity depth-sorting) doesn't actually apply here.
Practical case for A: builds on the already-proven per-entity render
loop, no new process to design/debug, and does NOT foreclose B later -
the shared camera-state file is the same real mechanism either way, so
a future compositor (if the transparent-independent-windows framing
ever turns out wrong in practice) could be built against the exact
same file with zero rework of the per-entity side.

**Option B stays documented here as the explicit fallback**, per
direct instruction - if Option A runs into a real problem (e.g. the
"windows just look independent, that's fine" assumption turns out
visually wrong once actually built, or real z-fighting/occlusion
complaints show up in practice), the real pivot is Option B from §3b
item 3 above: one new compositor process, closest analog
`bv_render_3d.c`, owning a real unified raymarch pass across all
armed/3D-mode entities at once. Re-read §3b item 3 in full before
attempting that pivot - it already documents the real reason Option B
needs a genuinely new process (board-viewer's own raymarcher only ever
renders one project's board, not N independent desktop windows at
arbitrary screen positions).

**Other open questions from §3b - resolved by precedent, not asked,
since the reasoning is confident:**

- **#2 (desktop-wide vs per-entity scope) - RESOLVED, downstream of the
  Option A choice itself**: since all entities poll the SAME shared
  camera-state file, the switch is necessarily desktop-wide by
  construction - there's no real way to make it "per-entity" without
  contradicting the shared-file design just chosen. No separate
  decision needed here.
- **#4 (halo replace vs overlay) - adopting the doc's own leaning**:
  overlay/ring around cursword's existing emoji sprite, never
  replacing it - matches the "amber tint window"/".pal-hint-armed
  bright title" precedents already cited in §3a, which never destroy
  the underlying content either.
- **#5 (armed-state file location) - adopting the doc's own cited
  convention**: `#.desktop/cursword_armed.txt` (boolean-ish, matches
  `rmmv_armed.txt`) for the arm/disarm flag, plus a new
  `#.desktop/desktop_camera_mode.txt` (or similar name) for the real
  shared 2D/3D + z-level/angle state every entity's own window polls -
  both real, house-standard "small state file under #.desktop/" shape,
  nothing new invented.
- **#6 (real, non-colliding camera key set) - RESOLVED, downstream of
  the doc's own §3a spec, not a new finding**: since real key capture
  only begins once cursword is genuinely ARMED (§3a: "while armed, real
  key capture begins... continues until real Escape"), this is the
  exact same real dual-mode principle already proven and documented for
  board-viewer's own `active_index==-1` model (`!.HOUSE_STDS.md` §A.9)
  - armed-mode owns 100% of keyboard input exclusively, so board-
  viewer's own real `1`-`4` camera_mode keys can be reused verbatim
  with zero real collision risk, regardless of what those same physical
  keys mean anywhere else on the UNARMED desktop. No new key list
  needed - reuse `1`-`4` as-is.

**Real, remaining open question - genuinely need your call, not
resolvable by precedent:**

1. **Click-vs-drag threshold, exact values.** §3b item 1's own
   "standard real fix" (ButtonPress records start pos; ButtonRelease
   within a small pixel radius AND short time = a real click/arm,
   otherwise the existing drag continues) has no house precedent to
   copy exact numbers from - a proposed default, not yet confirmed:
   **5px movement AND under 300ms** counts as a click. Confirm, or give
   real numbers to use instead.
2. **Does arming cursword disable normal dragging, or do both coexist?**
   §3b item 1's own framing leans "probably yes, both should coexist"
   but says this explicitly needs a real decision. If both coexist: a
   real ButtonPress on an ALREADY-armed cursword needs its own real
   rule too (does it re-arm/no-op, or does it start a drag like today,
   temporarily suspending armed key-capture until release?) - not
   specified anywhere yet, needs an explicit answer alongside the
   coexistence question itself.

Not proceeding with any code until these two are answered, per this
doc's own established discipline.

## 10. REAL ANSWERS (2026-08-30) - both open questions from §9 resolved,
    plus a real new feature added to scope: click-to-place movement

Direct instruction, both confirmed:

1. **Click-vs-drag threshold: the proposed 5px/300ms default is
   confirmed.** No different numbers given - use as specified in §9
   item 1.
2. **Dragging and arming coexist, confirmed** ("i think dragging it is
   fine too"). Real behavior for a ButtonPress on an already-armed
   cursword still needs the same real rule §9 item 2 flagged (does it
   start a drag, temporarily suspending key-capture until release, or
   something else) - not separately re-asked this round, so the
   original proposed answer stands as the real default to build:
   a ButtonPress+drag on an armed cursword starts a REAL drag exactly
   like today's existing behavior, suspending armed key-capture for
   the duration (release re-arms/resumes key-capture, still armed
   throughout - dragging doesn't disarm it).

**Real, new feature added to scope, direct instruction**: in addition
to the standard click-to-arm + arrow-key movement above, ALSO add a
real **click-to-place** movement mode - once armed (halo visible), the
NEXT click anywhere on the desktop moves cursword directly to that
click's location (same real feel as placing a tile/token on a board,
not a drag). Direct reasoning: "we do want that cursword click and
place functionality anyways... so maybe both?" - both real movement
styles (arrow-key nudge + click-to-place) are wanted, not a choice
between them.

**Real, house-standard way to keep this changeable while it's being
tuned** (direct instruction: "we could add it in a pdl as optionally
changeable till we figure out what actually works best in practice"):
a new key in `#.desktop/hq_ui.pdl` (same real home as `click_two_step`/
`opacity` - a house-wide, live-editable UI toggle, not buried in
cursword's own pal-scoped config), proposed name
`cursword_move_mode = click_place` (default) with `arrow_only` as the
real fallback value if click-to-place turns out to feel wrong in
practice - read the same way `click_two_step` already is (loaded once
at startup via each real consumer's own `*_load_click_two_step()`-
shaped function, no rebuild needed to change it, matches the "changed
marker" live-reload pattern from `dc759f3c` if it ever needs to change
value while a cursword window is already open, not just at next
launch). Both real movement styles (arrow nudge, click-to-place) stay
available as real code regardless of the PDL value - the setting picks
which one is ACTIVE while armed, not which one exists.

Real, still-open detail this doesn't yet answer (flag before building
click-to-place specifically, not blocking arm/halo/arrow-nudge work):
does click-to-place snap to the same real `GRID_CELL_PX` grid every
desktop entity already uses for normal placement (matches "like
placing a tile" framing most literally), or move to the exact raw
pixel coordinate clicked? Leaning grid-snap given the "like placing a
tile" wording, not yet explicitly confirmed.

## 11. Real git-workflow note (2026-08-30) - for opencode, since we
    share one git tree with no branch isolation

Direct user question, worth a permanent, shared answer since we both
work here: should Sonnet and opencode use separate branches to avoid
losing work? **No - staying on one shared branch (current, ongoing
practice) is the right call, not an oversight to fix.** Real reasoning:
today's actual incident (a Sonnet-authored doc edit landing under an
opencode commit, since a broad `git add` swept up both sets of
changes) cost nothing real - `git commit` is purely additive, nothing
was lost, just a commit-message/authorship mix-up. A real separate-
branch-per-agent workflow trades that mild, harmless mix-up for real,
ongoing overhead (merge coordination, rebasing, conflict resolution)
that doesn't match how lightly our work actually overlaps file-by-
file. The real risk worth actually avoiding, by either of us:
destructive git commands run without checking `git status` first -
`git reset --hard`, `git checkout -- <file>` (silently discards the
OTHER agent's uncommitted edits to that file), or `git push --force`/
`git commit --amend` on anything already pushed (rewrites shared
history for real). Plain `git add`/`git commit`/`git push`, as often
as either of us wants, stays fine and is the safer habit, not a risk -
less uncommitted work sitting in the shared tree at any moment is
real, active loss-prevention, not the other way around.

## 12. REAL PROGRESS LOG (2026-08-30) - arm/halo/movement/camera-key
    plumbing built and live-verified, still following §8's own
    "no code ahead of the enabling mechanism" discipline

Everything below is real, built, live-verified (screenshot/pixel-
sampling and xdotool-driven testing), and committed - not a plan:

- **Arm/disarm + halo** (`tp_desktop_window_rgb.c`): a real click
  (§9/§10's 5px/300ms threshold) toggles `g_cursword_armed`, written
  to `#.desktop/cursword_armed.txt`. Armed state draws a real, gap-
  free halo ring around cursword's own sprite via a NEW
  `cursword_update_shape()` that unions a ring into the window's own
  X11 Shape Extension mask (`ShapeUnion`, not `ShapeSet`) - the
  window's existing shape (sprite silhouette only, from
  `build_shape_mask()`) was clipping anything drawn outside it before
  this fix. Halo color: yellow/gold (`0xFFD400`), direct instruction
  (originally built neon-blue per the doc's own original wording
  above - since overridden).
- **Real "stingy" keyboard capture while armed**: arming takes a real
  `XGrabKeyboard` (not just relying on WM focus, which turned out
  unreliable) so every key press anywhere lands on cursword's window
  until Escape/disarm/placed releases it - direct report ("it should
  be very stingy with focus till esc is pressed").
- **Arrow-key nudge + click-to-place, coexisting** (§10): arrow keys
  move cursword one `GRID_CELL_PX` while armed; click-to-place grabs
  the pointer (`XGrabPointer`) on arm so the next click anywhere
  snaps cursword to that grid cell and auto-disarms. New
  `#.desktop/hq_ui.pdl` key `cursword_move_mode` (`click_place`
  default / `arrow_only`) picks which is active, per §10's own spec.
- **Cursword is always open, pinned at its own home spot** - real, new
  requirement beyond this doc's original scope, direct instruction
  ("cursword is an entity that should always be open... its the
  users assistant. 1rst entity"): `livedesk_ensure_cursword()`
  (`khtpm_taskbar_manager.c`) checks the live registry on every
  taskbar-manager start and every desk (re)spawn, relaunching
  cursword if it's ever found missing; exempted from both the normal
  close-all sweep and the `/proc` stray-kill sweep, so a desk switch
  or reset never closes it. Its spawn position is PINNED to `(0,0)`
  (top-left corner) on every fresh spawn, unlike every other entity's
  remembered last position - direct instruction ("it should always
  start in the upper top left, where it used to auto start").
- **§9 item #6's own real camera-mode key reuse, built**: while
  armed, keys `1`-`4` write a new desktop-wide
  `#.desktop/desktop_camera_mode.txt` with board-viewer's own exact
  real semantics (1=first person, 2=third person, 3=free roam,
  4=bird's eye - matches `bv_menu_input.c`'s own real key handling
  verbatim). This is control-side plumbing ONLY - no entity actually
  re-renders in 3D yet, that real per-entity raymarch/compose work
  (§3b, §8 item 4) stays explicitly deferred, same discipline as
  always. Live-verified: all four digits write the file correctly and
  log a real `CURSWORD_CAMERA_<N>_<NAME>` history row each.

**UPDATE (2026-08-30) - §8 item 4 is now started, real first slice
built and live-verified.** Direct instruction: "we need to do the big
important bulk of this now, then can polish the rough edges later...
we can start with camera 3/4 topdown only, if that would make it
easier?"

- `load_camera_mode()` polls `desktop_camera_mode.txt` every frame,
  desktop-wide by construction per §9 item #2's own resolution -
  EVERY entity's own window (this one shared binary), not gated to
  cursword or armed state.
- `draw_topdown_block_rgb()`: modes 3 (free-roam, simplified to
  topdown for now) and 4 (bird's-eye) render a real "extruded block" -
  the existing flat top-face blit, plus a real, art-derived shaded
  wall strip along the bottom of the sprite's own actual opaque
  silhouette (bbox-crop + edge-color-averaging, same real technique
  `bv_render_3d.c`'s own `compute_bbox_and_edge_color()` already
  proved correct). No separate voxel-asset generation needed - reuses
  each entity's own already-loaded `sprite.csv` texture directly.
- Real scope note, matching the direct instruction: modes 1/2 (true
  first/third-person perspective) and mode 3's own real free-roam
  camera movement stay deferred - this pass renders 3 and 4
  identically, both as the topdown/bird's-eye case.
- Live-verified: cursword's sword (real bottom padding) shows a clear
  wall strip in mode 4, reverts cleanly to flat in mode 1; book-stack
  (a completely different entity, zero extra wiring) picked up the
  same switch automatically, proving the shared-file architecture.
- **Known rough edge, honestly flagged, not blocking**: full-bleed
  sprites (art filling the whole 64x64 canvas, e.g. book-stack) have
  almost no room below their own opaque bounds for the wall strip -
  visually negligible there, clearly visible for sprites with real
  padding like cursword's sword. Real follow-up (shrink+reposition the
  top face to guarantee wall room for every sprite) is future polish.

**UPDATE (2026-08-30, same day) - real camera STATE added (pan + tilt),
plus a real rewrite from "shading cue" to real textured extrusion.**
Direct follow-up questions/corrections, in order: "do u understand how
it looks depends on the camera?" -> "both" [tilt changes the block's
own look, AND pan moves the whole desktop] -> "i didn't see any
evidence of extrusion yet, like in piececraft; is that known/
intention?" -> answered honestly (the first pass above was a flat
shading-strip cue, not real extrusion) -> "hopefully we do the
extrusion soon, cause thats the real kpi... to know we have made the
bulk progress."

- New `#.desktop/desktop_camera_state.txt` (`cam_pan_x`/`cam_pan_y`/
  `cam_tilt` 0-100) - real camera PARAMETERS, not just a mode
  selector. Cursword's own w/a/s/d (pan) and r/t (tilt) keys, reusing
  board-viewer's own real camera_control.c key convention verbatim,
  write it while armed in modes 3/4.
- Pan is a real, desktop-wide DISPLAY-position offset - every entity's
  own true `win_x/win_y` (drag/arrow-nudge/click-to-place/saved
  position) stays completely untouched; only the actual X11 window
  position gets `+cam_pan_x/+cam_pan_y` while in 3D mode, snapping
  back the instant camera_mode leaves 3/4.
- **Real bug found and fixed**: an entity nobody is directly
  interacting with never re-checks anything (this file's own
  `if (!need_redraw) continue`) - cursword's own pan/tilt/mode writes
  were silently invisible on every OTHER idle entity. Fixed with a
  real cheap-marker file (`desktop_camera_changed.txt`), same
  convention as this file's own existing `theme_changed_dirty()`.
  Live-verified: book-stack moved automatically from a cursword-only
  pan, zero direct interaction with book-stack itself.
- **`draw_topdown_block_rgb()` rewritten** - real, textured, tilt-
  driven extrusion, not a flat shading rectangle: a TOP face that
  visibly foreshortens (compresses vertically, real per-pixel
  resampling of the sprite's own texture) as tilt increases, revealing
  a FRONT/WALL face below it built from the sprite's own real bottom-
  edge texture row (stretched, progressively darkened with depth) -
  not a flat averaged color. At tilt=0 this is visually identical to
  the plain flat sprite; live-verified across the full 0/60/100 tilt
  range showing a clear, continuous, texture-driven progression
  (crossguard genuinely compresses, a real gray-metal wall
  matching the blade's own color grows taller).
- Real, honest scope note (unchanged): still not a full per-pixel
  raymarch like `bv_render_3d.c`'s own board-scale renderer (no true
  camera-relative perspective projection, no occlusion) - a real,
  texture-driven two-face extrusion for a single object already
  viewed from above, genuinely reactive to camera tilt/pan now.

**Still deferred, unstarted**: real ZOOM (dynamically resizing an
entity's own window - a bigger, separate change touching this file's
own shape-mask/grid/pixmap math throughout, deliberately not rushed
alongside pan+tilt), true first/third-person perspective rendering
(modes 1/2 - a real raymarch/perspective pass), mode 3's own real
free-roam camera movement (currently identical to mode 4), and the
full-bleed-sprite wall-room polish noted above.

## REAL, PHYMOJI RAYMARCH + Z-LEVELS UPDATE (2026-08-30/31)

Two more real, substantial passes since the extrusion update above -
in order:

**1. Real per-voxel phymoji raymarch** - direct correction ("thats
not true. phymoji does it with chicken emoji. u need to dig deeper"):
the earlier "texture-driven extrusion" was superseded by a genuine
per-voxel raymarch, reusing this house's own ALREADY-BUILT phymoji
system (`bv_render_3d.c`'s own `load_phymoji_asset()`/
`test_phymoji_hit()`, generated by real voxel CSVs at
`pieces/registry/phymoji_assets/<id>/voxels.csv`). Built a real,
shared, on-demand generator (`&.widgits/_shared-lib/ops/
sprite_phymoji_gen.c`) that generates a real voxel asset from an
entity's own actual `sprite.csv` (not a re-rasterized emoji glyph -
a real, confirmed bug: the raw Noto glyph for a given codepoint can
look completely different from an entity's own custom art). Wired
into `tp_desktop_window_rgb.c` as automatic, on-demand generation -
ANY entity's first launch now generates its own real voxel asset with
zero manual steps, direct instruction ("all new entities will use it
as well"). Real front/top yaw toggle added via a new `hq_ui.pdl` key
(`emoji_sprite_view`), real 1:1-proportional box scaling (not a
forced unit cube), real camera-controls consolidation
(`cursword_handle_camera_key()`, one real function/dispatch site
instead of three scattered branches), and a real fix for a spurious
self-disarm bug (X11 fires a real FocusOut with `mode==NotifyGrab`
the instant `XGrabKeyboard` establishes a grab - not a genuine focus
loss, needed filtering on `NotifyNormal`).

**2. Real z-levels** - direct instruction ("do we have z layers yet?
... using c & v the xelector/cursword moves up and down z levels but
the rest of the entities should remain on their own z level unless
some event is otherwise moving them"). A real per-entity z
(`desktop_pos.txt`'s own new optional `z=N` line) plus a real shared
`#.desktop/desktop_active_z.txt` - any entity whose own z doesn't
match the shared active z gets a genuine `XUnmapWindow` (real
visibility filter, not just a render skip), in BOTH 2D and 3D camera
mode (direct instruction: "it affects 2d also"), entirely within the
existing one-window-per-entity architecture - no shared 3D scene
needed (direct instruction: "i hope we dont have to switch to shared
scene just yet"). Cursword's own new `c`/`v` keys (closing the last
real gap in board-viewer's own `w/a/s/d/q/e/r/t/c/v` key set, `q/e`
yaw also added this same pass) move CURSWORD'S OWN z and write the
shared active-z file - cursword is the real xelector/selector role,
not a separate camera-only parameter. `tp_place_desktop.c` (the real
palette/picker placement flow) now stamps a freshly-placed entity
with the current real active z instead of always 0.

**Real, explicit scope note, direct instruction acknowledged, NOT
built this pass**: a Minecraft-style movable "highlighted/empty voxel
frame" 3D placement-preview cursor, for placing new entities directly
at a specific (x,y,z) in 3D mode. Real, wanted future work - a
separate, sizable UI feature on its own (2D placement already inherits
the current active z, per above; this is specifically about a live,
movable 3D preview before confirming placement).

## 13. Real plan: reserve keys 1-4 for a future "one map" perspective
mode; 5-8 = today's per-entity ("non map") 3D controls (2026-08-31)

**Status: PLAN, documented before continuing the code change** (direct
instruction, mid-edit: "document first"). Direct instruction, full
quote: "ok, i have decided we are going to move the current camera
controls to 5,6,7,8; so we can do this for 'non map 3d' and use
1,2,3,4, for if we ever do 'one map' perspective style 3d. we will get
rid of all entities, aad place them according to ray marching
perspective, much like a transparent version of piececraft. do u get
it? that will be our final trick" - followed by, mid-edit: "and we
will beable to do the same with camera with piececraft for 5,6,7, 8
get it?"

**The real distinction being drawn** - two genuinely different future
3D modes, not one:

- **"non map" 3D (today's real system, keys 5-8 going forward)**: what
  already exists per §2 above - each entity keeps its OWN real X11
  window, its OWN per-voxel phymoji raymarch, positioned independently
  on the desktop grid. No shared scene, no shared camera transform
  across entities - this is the real, deliberate choice already made
  in §2 ("i hope we dont have to switch to shared scene just yet").
- **"one map" perspective 3D (future, reserved keys 1-4, "the final
  trick")**: a genuinely different, NOT YET BUILT mode where every
  desktop entity is real-time raymarched together into ONE shared
  perspective scene from a single real camera - "get rid of all
  entities, add them according to ray marching perspective, much like
  a transparent version of piececraft." This is real Option B (the
  shared-compositor architecture explicitly declined for now back in
  §2/§12) - deliberately deferred again here, reserved as the meaning
  of keys 1-4 once it's actually built, not started this pass.

**The real, mechanical change this pass (in progress)**: cursword's
own `cursword_handle_camera_key()` in `tp_desktop_window_rgb.c` -
literal key binding moves from `XK_1..XK_4` to `XK_5..XK_8`. The
internal `g_camera_mode` VALUES stay exactly 1-4 (same meaning: 1
first-person, 2 third-person, 3 free-roam, 4 bird's-eye, same
`desktop_camera_mode.txt` contents, same `==3`/`==4` 3D-mode gates
everywhere else in the file) - only the physical keys that reach that
switch move, freeing the literal `1`/`2`/`3`/`4` keypresses for the
future one-map mode above. Real remaining work for this pass, not yet
done as of this section being written:
- Update the cursword debug key-log display (currently labels
  `XK_1..XK_4` as `"1"`/`"2"`/`"3"`/`"4"`) to label the new `XK_5..
  XK_8` keys correctly.
- Live-verify the 5-8 remap end to end (armed cursword, press 5-8,
  confirm `desktop_camera_mode.txt` + history still behave exactly as
  the old 1-4 keys did).
- **Same real remap on piececraft/board-viewer's own side** (direct
  instruction: "we will beable to do the same with camera with
  piececraft for 5,6,7,8 get it?") - `&.widgits/board-viewer/ops/
  bv_menu_input.c`'s own real `1`-`4` `camera_mode`/`is_pov_key`
  handling (§1's cited precedent, ~line 703) needs the identical
  key-binding move to `5`-`8`, for the identical reason: reserve
  piececraft's own literal `1`-`4` keys for the same future "one map"
  perspective mode, kept consistent across both real systems since
  they already share this one camera-mode convention by design (§1).
  NOT started yet.

Not touched by this plan: `9` (`key_possess`) and `x`/`z`/`c`/`v`
(z-level, hero vs camera) in board-viewer's own scheme - only the
`1`-`4` camera_mode digit keys are being reserved/moved.

## 14. §13 SUPERSEDED (2026-08-31) - one-map abandoned, camera
    simplified to a single `0` toggle; new TODO added

§13's whole premise (reserve 1-4 for a future "one map" mode) is dead
- see `^.ONE-MAP-ATTEMPT-ABANDONED.md` for the real, confirmed
compositor limitation that killed it (no app-side fix found: this
compositor honors `ShapeBounding` for click-routing only, never for
visual painting, on a continuously-reshaped override-redirect window).
The piececraft/board-viewer 5-8 remap in §13's own "not started yet"
list is also moot - don't do it.

Direct instruction, same close-out: "i just wanna use 0 to change
between 2d and 3d desk entity mode since theres only 1 camera mode for
desk." Real, live result: `cursword_handle_camera_key()` in
`tp_desktop_window_rgb.c` now binds a single `0` key, toggling
`g_camera_mode` between `1` (2D) and `4` (3D) and zeroing
cam_pan/tilt/yaw on every toggle - keys `1`-`8` are fully unbound
again. Also fixed in the same pass, live-verified: cursword now snaps
back to its real pinned home on a taskbar re-click (previously only
raised it), and a real "red shadow of the 2D shape" bug for every
non-cursword entity in 3D mode (stale `ShapeBounding` mask frozen to
the flat 2D sprite, now rebuilt each 3D frame from what's actually
drawn). Full technical detail and the real root-cause writeup for all
of this lives in `tp_desktop_window_rgb.c`'s own comments at the real
call sites, not duplicated here.

### Real TODO, added here for later (2026-08-31), not started

Direct instruction: "later i want to give all entities (but cursword)
'cut/copy/paste' options. lets add that to our dev dox todo lit for
soon." Real, scoped ask:
- Real cut/copy/paste actions on desktop entities (tiles/pals/etc) -
  cursword explicitly EXCLUDED (it's the desktop's own singleton
  controller, not a regular placeable entity - same real reasoning
  §12/§9 already established for why it's exempt from close-sweeps
  and z-level filtering).
- NOT scoped yet: where these actions surface (right-click context
  menu is the obvious existing precedent - see `tp_desktop_window_rgb.c`'s
  own real per-package `meta.pdl` METHOD-row context menu, already
  built and in use), what "paste" actually does for a live entity
  (spawn a real new placed copy via the same `livedesk_place_pal()`/
  copy-template path cursword's own spawn flow already uses?), and
  whether cut visually differs from a plain close+remember-for-paste.
  Real design pass needed before code - not started.

**Real answers, direct instruction 2026-08-31** ("just leave it there
till pasted; yes multiple pastes is ok. leave it single for now"):
- The clipboard (one shared `#.desktop/desktop_clipboard.txt`, holding
  the copied/cut entity's own template path) does NOT clear on paste -
  it persists until explicitly overwritten by the next real Copy/Cut,
  so pasting the same clipboard entry multiple times in a row is real,
  intended behavior, not a bug to guard against.
- v1 is single-entity select/cut/copy/paste only - one entity at a
  time, real and final for this pass (not a placeholder for
  multi-select, see the real future item just below).

**Real future item, added here, NOT this pass** ("later we will add a
'rectangle select tool for mult[i] draw/cut/copy/paste delete'"): a
real click-drag rectangle selection tool on the desktop background,
selecting every entity whose position falls inside it, so Cut/Copy/
Paste/Delete (and possibly a real multi-entity "draw"/stamp action)
can all operate on the whole selected group at once instead of one
entity at a time. Explicitly deferred - v1 above ships single-entity
first; this needs its own real design pass (how selection is drawn/
stored, what "paste" does for N entities at once - same relative
offsets from the original click point, most likely) before any code.
