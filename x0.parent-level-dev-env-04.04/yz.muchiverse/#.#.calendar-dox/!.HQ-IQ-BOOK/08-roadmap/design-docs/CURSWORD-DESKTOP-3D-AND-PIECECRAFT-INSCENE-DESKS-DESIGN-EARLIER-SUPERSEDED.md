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

1. **Left-click already means "drag" for every desktop entity today.
   RESOLVED (2026-08-29, direct answer): movement-threshold click
   detection.** `ButtonPress` records the start position; if
   `ButtonRelease` lands within a small pixel radius/short time of that,
   treat it as a real click (arm halo); otherwise it's the existing drag.
   Cursword keeps BOTH behaviors - drag-to-move still works exactly as
   before, click-to-arm is a real, new, disambiguated second gesture, not
   a replacement (`tp_desktop_window_rgb.c` ~line 3312 is where this real
   threshold check needs to land).
2. **Scope of the 2D/3D switch. RESOLVED (2026-08-29, direct answer):
   desktop-wide single mode.** One shared 2D/3D flag, living in the same
   real shared camera-state file §3b item 3 already established -every
   entity switches together, one consistent camera concept, not a
   per-entity toggle.
3. **Where does the "desktop-wide 3D scene" actually render? RESOLVED
   (2026-08-29, direct answer)**: option (b) below - no new compositor
   process. "each entity will change how its rendered, will render as 3d
   top down display." Each entity's own existing window
   (`tp_desktop_window_rgb.c`, one process per entity, unchanged
   architecture) independently re-renders itself in 3D mode, reading a
   real SHARED camera-state file (angle/zoom/z-level - likely under
   `#.desktop/`, exact path TBD) that every entity window polls, the same
   real "poll a shared state file, no cmd-bus" convention board-viewer's
   own `bv_state.txt` already proves live. No new compositor-style
   process owning a single combined raymarch pass across all entities at
   once (the alternative (a) considered in the first draft of this doc -
   explicitly NOT chosen).

   **Direct, real follow-on implication, same instruction**: the fixed
   80px `GRID_CELL_PX` 2D grid-snap every entity uses today
   (`tp_desktop_window_rgb.c`'s own `WIN_PX`/grid-snap-on-drag-release
   logic, this session's own earlier fix to that exact constant) is a
   REAL 2D-only constraint and does NOT carry over to 3D mode - "we wont
   use the '80px' default 2d style tile constraints any more when camera
   is zoomed in or out obviously." In 3D top-down mode, each entity's own
   on-screen size/position becomes a real function of the shared
   camera's zoom/angle against that entity's real desktop-grid position
   (grid_x/grid_y, or a real world x/y/z once z-levels apply), computed
   fresh per frame - not the fixed 80px (or 160px for footprint>1)
   window size 2D mode always snaps to. Real, concrete consequence for
   implementation: an entity's own X11 window itself likely needs to be
   real-time RESIZABLE/repositionable (`XResizeWindow`/`XMoveWindow`)
   as the shared camera zooms, not just its drawn CONTENT changing
   inside a fixed-size window - still not decided, but the direction is
   clear enough to flag now rather than leave implicit.
4. **Does the halo replace cursword's own emoji sprite, or render as an
   overlay/ring around it? RESOLVED (2026-08-29, direct answer):
   overlay/ring around the sprite.** Matches the "amber tint window"/
   "bright-yellow armed title" precedents already in this house - the
   halo is a visible ARMED-STATE INDICATOR, never a replacement for
   cursword's own normal appearance.
5. **Where does the key-recording session's own real state live?
   RESOLVED (2026-08-29, direct answer): inside cursword's own pal
   directory**, NOT under the shared `#.desktop/` area (a real,
   deliberate departure from this session's own `rmmv_armed.txt`
   precedent, since this state is specific to one entity - cursword -
   not a shared/global armed flag). Still needs to be visible/pollable
   by whatever reads the shared camera-state file (§3b item 3) for the
   actual 2D/3D+zoom+z-level values themselves, which stay separate from
   this per-entity armed/recording flag.
6. **Real key list for POV/camera - AUDITED (2026-08-30), no real
   collision found; recommendation, not yet confirmed by the user.**
   Checked `tp_desktop_window_rgb.c`'s real, existing key handling
   (every entity window's own event loop) directly: its only digit/
   arrow bindings (`0`-`9` jump-to-nav-index, `Up`/`Down` focus-row
   navigation, ~line 3060-3120) are gated entirely behind `popup_win`
   being non-null - i.e. only live while a real right-click context
   menu is already open. A new cursword-armed key-capture mode, gated
   behind its OWN distinct armed-state check (see §3b item 5), would
   never run concurrently with that existing block, so there's no real
   key-CODE collision to resolve at the code level. The taskbar strip's
   own digit dispatch (`1`-`15` header cells) lives in a fully separate
   X11 window/process with independent real keyboard focus - clicking
   cursword transfers real focus to cursword's own window, so no
   cross-window collision either. **Recommendation**: reuse board-
   viewer's exact real key convention verbatim for consistency across
   the house (`1`-`4` camera_mode, `x`/`z` z-level, `c`/`v` camera
   z-level, arrows for movement/pan) - not required by any real
   technical constraint, purely a "one convention, not two" consistency
   choice, still needs a direct yes/no from the user before treating it
   as final.

## 4. Piece 4 - Minecraft-style HUD (2026-08-29, direct request, recorded for later)

A real, additional planned piece of this same long-term feature set,
noted for later rather than designed in depth yet: a Minecraft-style HUD
(health + inventory display, bottom of screen) that can be opened once
the 3D desktop mode above exists. Not designed - no real file/mechanism
research done yet for this piece specifically. Real, obvious open
questions once this gets picked up: does health/inventory apply per
possessed/armed entity (cursword? whichever pal is "active"?) or is it a
real, desktop-wide HUD; does it reuse any existing piececraft-xyz HUD
concept (its own in-game "Hero HP" line, confirmed real and live -
`PIECECRAFT-LOCAL-VERIFY-2026-08-29.md` §3's own screenshot shows
"Hero HP: 20") or is desktop inventory/health a genuinely new, separate
concept with no piececraft equivalent to port. Depends on the 2D/3D
desktop work above landing first.

## 5. Suggested next step

Open question #3 (the largest real architectural fork) is RESOLVED - see
§3b item 3. Open questions #1/#2/#4/#5 are also now RESOLVED (§3b) - only
#6 (real camera/POV key list, needs a real key-binding audit) and piece 4
(the HUD, not designed yet) remain genuinely open. Piece 1 (piececraft's
own setup-screen replacement) is comparatively self-contained and could
start independently of pieces 2/3/4, since it doesn't depend on the
desktop-3D architecture question at all.
