# pc-hq 3D raycast + click-voxel highlight — design (2026-09-29)

Written per direct instruction, task #3 of the pc-hq parity audit
("there seems to be a raycast issue where when i click map in pc-hq it
didn't hit? can we show a grid highlight outline of the exact voxel i
clicked?"). Root cause confirmed (not assumed): there is no raycast at
all today, not a broken one. `bv_render_3d.c`'s own header comment on
`write_pick_txt()` (~line 1005) says so directly: **"Keyboard/selector-
driven for now; a real click ray can overwrite the same file later."**
The shared renderer's canvas-click handler
(`khtpm_core_render.c`, ~line 11477, `kh_canvas_hit()` +
`kh_interact_engage_if_needed()`) only ever **arms keyboard-forward
mode** on a canvas click — it explicitly does NOT forward the click's
`x, y` anywhere a ray could be built from them. So a click "not
hitting" is exactly correct current behavior for what's built, not a
bug in existing raycast code.

Status: **plan only, not started** — held off with #1
(`PCHQ-3D-MOVE-OVERLAY-DESIGN.md`) pending this doc, and explicitly
flagged by the owner as a candidate hand-off to Grok once written
("are those easy modification additions, or would u prefer to pass
them to grok" / "maybe hand off to 3").

## 0. What already exists to build on

- `write_pick_txt()` (`bv_render_3d.c` ~line 1019) — MILESTONE D. Every
  frame, publishes `pieces/display/pick.txt` (`sel_x/y/z`, `kind`,
  `id`, `template`, `glyph`) for whatever the **keyboard selector** is
  currently over. `pc_entity_ctx.sh` already reads this file for the
  footer-driven ("open their context from bottom tb") menu path — this
  is the file a real click ray needs to become able to write, per the
  comment's own stated intent, not a new file/format.
- `build_camera()` (`bv_render_3d.c` ~line 1890) — produces a real
  `Camera { Vec3 eye, forward, right, up; }` (`Vec3 = {double x,y,z}`,
  ~line 68) every frame from `camera_mode`/`cam_yaw`/`cam_pitch`. This
  is exactly the basis a screen-space ray needs; no new camera-state
  plumbing required, just reading the same struct the rasterizer
  already builds.
- The xelector marker draw path (~line 1720, "xelector (the board's own
  targeting cursor...) - cyan") already highlights a specific world
  cell distinctly from ordinary terrain — this is the real precedent
  for "outline the exact voxel clicked," just keyed off the keyboard
  selector's position today instead of a screen click's resolved hit.
- `kh_canvas_hit(x, y)` (`khtpm_core_render.c`) already knows the
  canvas's own screen-space bounding box and already receives the raw
  `ev->xbutton.x/y` — the click coordinates exist at the exact call
  site that currently discards them for anything beyond an engage
  toggle.

## 1. The real gap: click coords never leave the shared renderer

`kh_canvas_hit()`'s caller has the pixel `x, y` (window-space) and
knows it hit the canvas, but `khtpm_core_render.c` is the **generic**
renderer (CENTROID_GOLD_STD.md: zero new per-project C, ever) — it
must not gain pc-hq-specific raycast math itself. The correct shape,
matching the manager/renderer split every other khtpm app already
uses:

1. On a canvas click that's already past `kh_canvas_hit()`'s bbox
   check, the generic renderer writes the **canvas-relative** pixel
   coords (not the ray, not a world hit — that's real per-app logic it
   must not own) to a small, generic, already-precedented relay file —
   reusing the **existing per-process relay** (`#.desktop/
   entity_menu_history/<pid>.txt`) with a NEW line shape,
   `CANVAS_CLICK: <button> <canvas_x> <canvas_y> <is_press>`, sibling
   to the existing `MOUSE_EVENT:` line the house-standards skill
   documents. This keeps the generic/per-app boundary exactly where
   `khtpm-house-standards` already draws it — the renderer publishes
   raw input, `bv_render_3d.c` (a real per-app manager, not the shared
   renderer) does the math.
2. `bv_render_3d.c`'s own frame loop (it already polls its own inputs
   each tick — the same loop that reads keyboard-selector movement)
   also polls this new relay line. When one arrives:
   - Convert canvas-relative pixel → normalized device coords using
     the canvas's own known render width/height (already tracked,
     `bv_render_3d.c` owns `g_fbuf`'s dimensions).
   - Build a ray: `origin = camera.eye`, `dir = normalize(forward +
     right*ndc_x*tan(fov/2)*aspect + up*ndc_y*tan(fov/2))` — standard
     pinhole-camera ray, using the SAME `fov`/aspect the rasterizer
     already computes per frame (grep the existing perspective-project
     call for the exact constants already in use — do not
     independently re-derive FOV, reuse the rasterizer's own value so
     the ray and the picture it's clicking on never drift apart).
   - Voxel-march the ray through `board3d[MAX_VOXEL_Z][...]` (the same
     3D grid the rasterizer already walks per-column) using a DDA/
     Amanatides-Woo step — cheap, this house already has a fully
     working GPU raymarch daemon (`board-viewer 3D perf ceiling`
     memory) for the whole-frame case; a single ray for one click is
     negligible against that baseline, no perf-budget concern.
   - First solid voxel hit (or a hero/tree/entity's own AABB, checked
     first since those already have real world positions independent
     of `board3d`) becomes the new `sel_x/sel_y/sel_z` — **overwrite
     the same fields `write_pick_txt()` already publishes**, exactly as
     that function's own comment anticipates, so every existing
     consumer (`pc_entity_ctx.sh`, the future Move overlay) needs zero
     changes to accept a click-driven selection instead of a keyboard-
     driven one.
3. No ray hit (click into open sky/void) → leave the existing
   keyboard-selector position as-is (a no-op), matching "click doesn't
   fight the keyboard cursor" as the least-surprising default.

## 2. Click-voxel highlight outline

Once §1 lands, "highlight the exact voxel clicked" is **not a new
drawing feature** — it is the existing xelector-marker draw code
(~line 1720) generalized to draw at `sel_x/sel_y/sel_z` regardless of
*why* the selector moved there, using a distinct outline color/style
from both the xelector cursor and the hero marker (three real, visually
distinct roles: keyboard cursor, hero, and now "last click hit" — the
existing code already handles two colors distinctly, this is the same
pattern extended to a third). If the click hit an entity (hero/tree/
etc, not a bare voxel), reuse whatever bounding-box outline convention
`pick.txt`'s existing `kind=` consumers already imply (an entity has
its own footprint, not a single voxel cell) — this doc does not invent
new per-kind highlight shapes, it wires the existing one to also fire
on a resolved ray hit.

## 3. What this does NOT decide yet

- **Exact FOV/projection constants to reuse.** Must be read out of the
  live rasterizer code at implementation time, not re-derived from
  scratch here, specifically to avoid the ray and the picture disagreeing
  about what a given screen pixel points at (a click that visually
  looks right but resolves to the wrong voxel is a worse bug than "no
  raycast" — it is not merely a build detail).
- **Whether entity AABB checks happen before or interleaved with the
  voxel DDA march.** Checking entities first is proposed above (§1)
  because entities are typically what a player intends to click on
  and a coarse-first-then-fine check is the standard approach, but this
  is a real implementation call, not fixed by this doc.
- **Multi-select / drag-rectangle selection** (`PIECECRAFT_XYZ_DESIGN.md`
  §3's own "future 7.edit rectangle selector" reference, and
  `write_pick_txt()`'s comment naming it) is explicitly out of scope —
  this doc is single-click, single-voxel only, matching what was asked.

## 4. Suggested hand-off framing (per the owner's own question)

This is a reasonable Grok hand-off candidate specifically because the
hard architectural question — "where does click input cross the
generic/per-app boundary, and what file/line shape does it use" — is
answered by this doc (§1.1's new `CANVAS_CLICK:` relay line, sibling to
the existing `MOUSE_EVENT:` line); what's left is mechanical (DDA
voxel-march math, wiring the existing highlight draw to a new trigger)
and doesn't require further architectural judgment calls beyond the
two open items in §3.
