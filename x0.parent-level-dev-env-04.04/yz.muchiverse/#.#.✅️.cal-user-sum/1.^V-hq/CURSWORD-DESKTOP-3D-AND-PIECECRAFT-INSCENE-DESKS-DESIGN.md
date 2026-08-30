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
