# pc-hq 3D Move overlay + z-layer range matrices — design (2026-09-29)

Written per direct instruction after the desk's tile-picker Move
range-finder landed (writer/renderer split, `tp_gen_range_matrix.+x` +
`tp_arm_placer_rmmv.c`, see that op pair's own header comments) and the
pc-hq parity audit that followed it (`PCHQ-ENTITY-MENU-AND-TASKBAR-
DESIGN.md` context + this session's own research). Task #1 of that
audit — "does Move show in pc-hq" — the answer is **no**: pc-hq's real
"PLACE" verb (`pc_menu_input.c`'s `CTX_PLACE` branch) is an explicit
stub (`"Place: pick a block palette (todo)"`), and there is no overlay
window at all in the 3D path. This doc is the plan for that overlay,
scoped exactly to the two things the owner asked for:

1. Reuse the desk's existing flat-matrix format/functions as-is when
   pc-hq is in **2D view mode**.
2. Generate **per-Z-height matrix files ("z layer pages")** when in
   **3D view mode**, so the 3D renderer can look up the correct shape
   slice for the entity's current Z.

Status: **plan only, not started** — explicitly held off ("yes hold
off on those, we should probably do 4 first") until #4 (BOOK:PAGE
label fix, done, `847fd8009`) and #2 (context-menu Events/Inventory/
Dir parity, done, `3c44fab3e`) landed. Both are done as of this doc.

## 0. What already exists, and why it's directly reusable

- `&.widgits/tile-picker/ops/tp_gen_range_matrix.c` — the writer. Reads
  `desk_grid.pdl`'s `move_view_range` (or `TP_VIEW_RANGE` env
  override), computes a Manhattan-distance diamond, writes a plain
  `#`/`.` grid to a file. **Zero X11, zero rendering — pure text-in/
  text-out.** This is exactly the same shape pc-hq's 2D mode needs; no
  fork required for the 2D case (see §1).
- `&.widgits/tile-picker/ops/tp_arm_placer_rmmv.c` — the renderer (X11,
  2D desktop-window based). Loads `TP_RANGE_MATRIX`, does
  `range_matrix_allows(origin_c, origin_r, r, c)` lookups for both
  validity (`ov_cell_valid`) and the drawn wireframe (`ov_draw_pane`).
  This renderer is **desktop-window-specific** (raw Xlib windows on the
  X11 desktop) and cannot be reused as-is for pc-hq's 3D view — pc-hq's
  3D scene is a single blitted `<canvas>` inside a khtpm-rendered
  window (`bv_render_3d.c` writing `rgb_frame_3d_overlay.raw`), not a
  grid of real X11 windows. This is the same "different POV, shared
  data format, not shared renderer binary" split the owner named
  directly ("they should share much of the same code when possible.
  they are different pov's of the same file and functions").
- `PIECECRAFT_XYZ_DESIGN.md` (`MAPS-TILES-ZLEVELS-CONSOLIDATED-SPEC.md`
  §3) already establishes **per-Z-level flat grid files** as the
  house's real, chosen storage shape for chunked voxel terrain — the
  z-layer-pages idea in this doc is not a new pattern, it's the same
  one applied to a range matrix instead of terrain.

## 1. 2D mode: reuse the desk format and functions directly

pc-hq's 2D view (`PCHQ-2D-TILE-VIEW.md`) is, per that doc, a flat grid
render — closer in shape to the desk's own tile grid than the 3D
raymarch path is. The concrete plan:

- `tp_gen_range_matrix.+x` runs UNCHANGED — it already takes
  `<desktop_root> <out_file>` and has no desk-specific assumptions in
  its math (Manhattan-distance diamond from an origin cell, radius from
  a `.pdl` key or env override). Point it at pc-hq's own project root
  and a pc-hq-owned output path (e.g.
  `$ROOT/pieces/display/move_range_matrix.txt`) and it produces the
  identical `#`/`.` grid format.
- A new, small launcher (per house rule: shell = pure launcher, no
  logic) — `pc_move_entity.sh`, mirroring `move_entity_on_desk.sh`'s
  own three lines (generate, `rm -f` the stale file first, export the
  path) — calls the writer before pc-hq's 2D renderer reads it.
- The 2D renderer's own cell-validity check (wherever `PCHQ-2D-TILE-
  VIEW.md`'s grid-draw loop lives) gets the same `range_matrix_allows()`
  lookup **ported, not shared-linked** — per the house's own "no
  header+link sharing" rule (`no-header-link-split` memory: inline for
  1 consumer, text-include for 2+ pure consumers, op+fork+IPC for
  stateful 2+). Since this is a second, independent C binary (not a
  second call site inside `tp_arm_placer_rmmv.c` itself), a **text-
  include** of `range_matrix_allows()`/`load_range_matrix()` as a
  small shared `.c` (e.g. `_shared-lib/range_matrix.c`) is the correct
  shape — pure functions, no shared runtime state, 2 consumers (desk +
  pc-hq 2D) as of this doc, a 3rd (pc-hq 3D, §2) coming next.

No new file format, no new radius/shape logic — this is a straight
"same writer, same grid, different reader" port.

## 2. 3D mode: z-layer range-matrix pages

3D mode cannot use one flat grid the way 2D can, because "diamond
around the entity" is really a **3D shape** (a diamond in X/Y that
should plausibly also fall off in Z, or at minimum needs to be
evaluated per-Z-slice so the raymarch renderer can draw the right
wireframe cells at the entity's current height without re-deriving
range math itself).

**Design: the writer emits one file per relevant Z-offset, not one
combined 3D grid.**

- New writer, `tp_gen_range_matrix_z.c` (or `tp_gen_range_matrix.c`
  gains a `--z-layers` mode — leaning toward a **separate op**, since
  the 2D writer must stay a trivial, single-file, zero-argument-beyond-
  root-and-out tool per its own existing contract, and z-layer mode
  needs an extra axis of parameters; a new op with a shared internal
  helper keeps `tp_gen_range_matrix.c` untouched for the desk/2D
  consumers already depending on its exact CLI shape).
- Output shape: `<out_dir>/z<offset>.txt` for `offset` in
  `-radius..+radius` (or a separately configured vertical range,
  `move_view_z_range` in `desk_grid.pdl`/pc-hq's own `pchq.pdl`,
  defaulting to 0 — i.e. same-Z-only — until a real vertical-move
  entity exists to motivate more). Each file is the exact same `#`/`.`
  format as the 2D case, just computed against
  `sqrt(dc²+dr²+dz²) <= radius` (a spherical/octahedral falloff) or
  kept as pure-Manhattan-in-XY-at-every-Z (flat "columns" of the same
  diamond at every reachable Z) — **this is a real open question for
  the owner, not decided here** (see §4).
- `z0.txt` (the entity's own Z) is the one that must always exist and
  matches the 2D grid exactly when radius/shape config is shared — this
  keeps 2D/3D visually consistent for the same move, which the owner's
  "different POVs of the same file and functions" framing implies is
  the goal, not an incidental nicety.
- The 3D renderer (`bv_render_3d.c`) loads only the page(s) needed for
  the Z-range currently visible/relevant (the entity's own Z ± however
  many layers the overlay is allowed to show at once — likely just the
  entity's own Z for a first cut, matching "starts right on entity
  space" precedent from the desk fix), looks up
  `range_matrix_allows()` per voxel face exactly like the 2D/desk path,
  and draws a wireframe overlay on the affected voxels (reusing
  whatever cell-highlight drawing primitive comes out of the raycast
  design, §3 of `PCHQ-3D-RAYCAST-AND-VOXEL-HIGHLIGHT-DESIGN.md` — the
  Move overlay is really "the highlight-box mechanism, called once per
  in-range cell instead of once for the clicked cell").

## 3. What this does NOT decide yet (flagged, not silently assumed)

- **Whether Z-falloff is spherical or "same shape at every Z within a
  vertical range."** Needs a one-line owner decision before
  `tp_gen_range_matrix_z.c` is written; the file-per-Z-offset shape
  works identically either way, so this doesn't block scaffolding the
  op, only its distance-formula.
- **How many Z-layers the 3D overlay actually renders at once.** Likely
  1 (current Z only) for a first cut; multi-layer rendering (seeing
  cells above/below through transparency) is real future scope, not
  needed for parity with the desk's Move today (the desk is inherently
  single-Z).
- **PLACE's stub status.** This doc only covers Move's overlay parity;
  `CTX_PLACE`'s "pick a block palette" stub is a separate, already-
  flagged pc-hq gap, untouched here.

## 4. Build order (once picked up)

1. Text-include `range_matrix_allows()`/`load_range_matrix()` out of
   `tp_arm_placer_rmmv.c` into a shared `_shared-lib/range_matrix.c`,
   with the desk renderer as the first (only, so far) consumer —
   verifies the extraction doesn't regress the already-shipped desk
   Move before anything pc-hq-specific touches it.
2. Wire pc-hq's 2D mode to `tp_gen_range_matrix.+x` output + the new
   shared `range_matrix_allows()` (§1) — this alone gets 2D-mode parity
   with zero 3D/raycast work.
3. Get the owner's answer on the Z-falloff-formula open question (§3),
   then write `tp_gen_range_matrix_z.c`.
4. Wire `bv_render_3d.c`'s overlay draw once the raycast/highlight
   primitive from the sibling design doc exists (real dependency — the
   3D Move overlay reuses that primitive rather than inventing its own
   voxel-outline drawing code a second time).
