# "One map" perspective 3D — design (2026-08-31)

**Status: DESIGN ONLY, nothing built.** This is the deferred "final trick"
from [[CURSWORD-DESKTOP-3D-AND-PIECECRAFT-INSCENE-DESKS-DESIGN.md]] §13 —
written up on its own per direct instruction ("lets write a design doc for
'one-map' first"), before any real code, since it's a genuine architecture
change, not a follow-on tweak.

## 0. Direct quotes this design is built from

- "ok, i have decided we are going to move the current camera controls to
  5,6,7,8; so we can do this for 'non map 3d' and use 1,2,3,4, for if we
  ever do 'one map' perspective style 3d. we will get rid of all entities,
  aad place them according to ray marching perspective, much like a
  transparent version of piececraft. do u get it? that will be our final
  trick"
- "and we will beable to do the same with camera with piececraft for
  5,6,7,8 get it?" / "the camera keys stay the same, they will work in
  'piecemode' as well" — confirms one-map and piecemode share ONE control
  scheme, not two.
- "ok, and is '0' still what goes back and forth from 2d to 3d?" /
  "we will need 0 when we do perspective 3d mode etc like piececraft" —
  `0` is the reserved future on/off toggle INTO one-map mode itself (see
  §4).

## 1. The real distinction (already drawn in §13, restated here as the
anchor for everything below)

Today's desktop 3D (keys 5-8, [[CURSWORD-DESKTOP-3D-AND-PIECECRAFT-INSCENE-DESKS-DESIGN.md]]
§2/§12) is **"non map"**: every entity is its own real X11 window, its own
process, its own independent phymoji raymarch, positioned by ordinary
window-manager coordinates. No shared camera, no shared scene. Real,
deliberate, already shipped.

**"One map"** is genuinely different: every entity raymarched together
into ONE shared perspective scene from a SINGLE camera — "get rid of all
entities, add them according to ray marching perspective, much like a
transparent version of piececraft." This is real Option B, the
shared-compositor architecture [[CURSWORD-DESKTOP-3D-AND-PIECECRAFT-INSCENE-DESKS-DESIGN.md]]
§2/§12 explicitly declined for the "non map" pass ("i hope we dont have to
switch to shared scene just yet") — that deferral ends here, this is where
it actually gets built.

## 2. Real precedent already in the house (not inventing from scratch)

Board-viewer's own `render_mode==1` path already does almost exactly this,
just scoped to ONE piececraft project instead of the whole desktop:
`&.widgits/board-viewer/ops/bv_render_3d.c` raymarches every real piece on
the board (hero, NPCs, terrain) from ONE shared camera
(`build_raymarch_cam()`-equivalent), testing each ray against
`test_phymoji_hit()` (~line 781) per piece, picking the nearest hit across
all of them — real multi-object raymarching against one camera, already
built, already shipped. **This is the real, closest analog** — "a
transparent version of piececraft" is describing this exact mechanism,
generalized from "pieces on a board" to "entities on the desktop."

The real gap: board-viewer's pieces already live in ONE process with one
shared coordinate space (the project's own board grid). Desktop entities
today are N separate OS processes/windows with independent local
coordinate spaces (each phymoji column set is in that entity's own local
voxel-grid space, built fresh per-process by `build_phymoji_columns()` in
`tp_desktop_window_rgb.c`). One-map has to bridge that gap.

## 3. Real architecture options for the gap

**Option A — new standalone compositor process** (recommended starting
point): one new binary (e.g. `khtpm_one_map_render.c`, same "own real
process" convention as `khtpm_strip_parser.+x`), spawned when one-map mode
turns on, that:
1. Reads every entity's own `desktop_pos.txt` (`x=`/`y=`/`z=`) to get its
   real world position — already exists, zero new state needed.
2. Reads every entity's own `pieces/registry/phymoji_assets/<entity_id>/
   voxels.csv` directly (same file `tp_desktop_window_rgb.c`'s
   `load_entity_phymoji()` already loads per-process) and OFFSETS each
   entity's local voxel coords by its real world position (grid_x *
   GRID_CELL_PX, grid_y * GRID_CELL_PX, z * some-real-z-unit) into one
   shared world voxel space.
3. Builds ONE shared camera (`build_raymarch_cam()`, ported/shared same as
   §12's own porting from `bv_render_3d.c`) and raymarches the WHOLE
   merged scene into one real fullscreen (or large) window.
4. Every individual entity's own per-window rendering is suppressed while
   one-map is active — real, same `XUnmapWindow` mechanism already used
   for z-level filtering ([[CURSWORD-DESKTOP-3D-AND-PIECECRAFT-INSCENE-DESKS-DESIGN.md]]
   §2), just gating on "one-map active" instead of "wrong z."

**Option B — extend `tp_desktop_window_rgb.c` itself**: cursword's own
process (already the desktop's real controller) grows a second render path
that, on entering one-map mode, reads every other entity's voxels the same
way as Option A and draws the merged scene into ITS OWN window
(temporarily resized/repositioned to fill the view). Simpler (no new
binary/build-script entry), but couples an unrelated real responsibility
(desktop-wide compositing) onto cursword's own process, which today is
scoped to "one entity's own window + desktop-wide key input." Option A
keeps that separation cleaner, closer to the real "one process per real
responsibility" house convention this whole codebase already follows
(`khtpm_taskbar_manager_main.c` vs `khtpm_strip_parser.c`, etc.).

**Recommendation: Option A.** Open question for the user before real code
starts (see §7).

## 4. Real key scheme (ties §13's reservation back together)

- `0` — reserved future on/off toggle for one-map mode itself (per direct
  instruction, mirrors board-viewer's own `key_toggle_render_mode='0'`
  exactly — same key, same real meaning, now also on the desktop side).
  Toggling on: suppress all individual entity windows, launch/reveal the
  one-map compositor. Toggling off: tear it down, restore individual
  windows.
- `1`-`4` — once `0` is on, these become the one-map camera's own POV
  switch, same real 4-mode meaning board-viewer already uses (1
  first-person, 2 third-person, 3 free-roam, 4 bird's-eye) — the exact
  numbering "non map" mode used before its own move to 5-8 in §13. This is
  why 1-4 had to be freed up first.
- `5`-`8` keep meaning "non map" mode's own 4 camera modes when one-map is
  OFF (§13, already shipped) — no collision, since 1-4 only mean anything
  once `0` has turned one-map on (same real "gated digit keys" pattern
  board-viewer's `render_mode` gate already uses for its own 1-4).
- `w/a/s/d`/`q/e`/`r/t`/`f` (pan/yaw/tilt/reset) — same real keys, same
  meanings, in BOTH modes ("the camera keys stay the same, they will work
  in 'piecemode' as well") — one shared control layer, not two.

## 5. Real open questions this design doc surfaces (not yet decided)

1. **Transparency** — "a transparent version of piececraft": does this
   mean genuinely translucent/see-through voxels (alpha blending through
   overlapping entities, real depth-sorted compositing, more expensive),
   or "transparent" in the sense of "the same mechanism, made visible/
   legible to you" (i.e. not literal alpha blending, just describing the
   effect informally)? Changes real render-cost and real hit-testing
   design (a literal-transparency raymarch can't early-exit on first hit
   the way `test_phymoji_hit()` does today).
2. **Z-level interaction** — does one-map show every z-level's entities
   at once (a real, full "dollhouse" cross-section view), or does the
   existing active-z filter ([[CURSWORD-DESKTOP-3D-AND-PIECECRAFT-INSCENE-DESKS-DESIGN.md]]
   §2) still apply, showing only the current active layer merged into one
   scene? Both are real, valid designs; they're different features.
3. **Scale/performance** — how many entities realistically need to be in
   one merged scene at once? Determines whether real per-entity AABB
   culling (skip an entity's whole voxel set if the current ray's overall
   direction can't possibly cross its world-space bounding box before
   testing individual columns) is required day one or can wait.
4. **Placement while in one-map mode** — is placing a NEW entity while
   one-map is active in scope for this design, or strictly out of scope
   (place in "non map" mode, see it reflected next time one-map turns
   on)? The Minecraft-style 3D placement-preview cursor
   ([[CURSWORD-DESKTOP-3D-AND-PIECECRAFT-INSCENE-DESKS-DESIGN.md]]'s own
   real deferred scope note) would live here if so.
5. **Option A vs B** (§3) — new standalone compositor process, or grown
   into cursword's own process?

## 6. Explicit non-goals for this doc

Not attempting to spec exact pixel/world-unit conversions, exact file
formats for any new shared state, or exact function signatures yet — this
doc is scoped to settling the real architecture questions in §5 first, per
direct instruction to write the design before any code. A follow-up
revision (or a §-numbered addendum here) turns the accepted answers into a
real implementation plan once decided.
