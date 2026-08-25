# Piececraft-Wraith Architecture & Standing Conventions

Consolidated 2026-07-06, replacing all prior files in this directory
(2fix.txt, ARCHITECTURE-RGB-RENDERING.md, emoji-entity-feature-report.md,
FINAL-HANDOFF-jul2.md/.txt, handoff-summary-jul2.txt,
HANDOFF-WRAITH-STATE-MYSTERY.md, j5-prompt.txt, j5-summary&handoff.txt,
jul2-work-summary.txt, py3d-inspo.md, SOLUTION-FOUND-jul2.txt,
wraith-man-fix-j2.md, WRAITH-TERMINAL-NO-MANAGER-GAP.md,
wraith-test-checklist.txt, wra-mana-checklist.txt, x0.2do.txt,
x0-piececrafts-claude.txt, x0-piececrafts-prompt.txt). Those files
accumulated across ~5 days of sessions (2026-06-28 through 2026-07-05)
as bug investigations, handoffs, and planning docs; a lot of their
content was superseded by later fixes in the same set of files. This
doc keeps only what's still true and useful to a future agent picking
up this codebase cold. A zip backup of the originals exists (made
before deletion) in case any narrative/historical detail is ever needed.

Session-specific window-geom/settings-hub content that was physically
in this directory (`j5-prompt.txt`, `j5-summary&handoff.txt`) has been
moved topically into `x0.short-term-vision/WRAITH-ARCHITECTURE-AND-ROADMAP.md`
and `x0.short-term-vision/settings-hub-window-geom-design-j5.md`, since
that's a Wraith-desktop-shell feature, not a piececraft-wraith one —
it just happened to be dumped in this folder chronologically.

---

## 1. Manager / Ops Architecture Pattern (standing convention, all Wraith-hosted projects)

This is the single most important standing rule in this directory's
history — it was gotten wrong twice (see "History" below) before
converging on this shape. Apply it to any new Wraith-hosted project.

**The pattern:**
- **Manager (`manager/<project>_manager.c`) = one-time init only.** Its
  job is: `resolve_paths()`, `log_debug()`, and
  `trigger_initial_render()` — fork/exec/waitpid the project's own
  `ops/+x/wraith_project_input.+x` once so the window isn't blank
  before the first keypress, then exit/idle. **No persistent polling
  thread, no per-keypress state mutation in the manager.**
- **`ops/+x/wraith_project_input.+x` = the synchronous hot path.**
  wraith-alpha's raw-key handler invokes this directly (fork/exec/waitpid,
  never `system()`) for every keypress, *before* `trigger_render()`.
  This is where all per-frame state mutation belongs.
- Registration: `project.pdl`'s `META | manager | ...` line is the real
  auto-launch trigger; the layout's `<module>` tag documents the same
  thing (correctness/future-proofing) but isn't itself parsed by Wraith
  for this purpose. `<module>` tags must always contain a concrete path
  — never a `${variable}` substitution (the manager that would set that
  variable hasn't run yet when the layout is first parsed — a deadlock).
- Compile convention: `gcc -std=gnu11 -Wall -O2 manager/<project>_manager.c
  -o manager/+x/<project>_manager.+x`.

**What NOT to do (both were real, reproduced bugs):**
- Never put per-keypress state mutation in the manager's own async
  poll/background thread — it races wraith-alpha's synchronous
  `trigger_render()` and produces a structural, always-reproducible
  one-keypress-behind input lag. (This was the "production ready"
  pattern in the earliest handoffs; it was wrong and was replaced.)
- Never write session/project state non-atomically — a real bug was
  reproduced where `read_project_map_control()` read a truncated file
  mid-write. Always temp-file + rename.
- Not every ops-only project (one with no manager at all) is
  automatically broken — audit actual input-latency behavior before
  assuming a manager needs adding. A manager's only job may be to exist
  so the `project.pdl`/`<module>` convention is satisfied; some
  legitimately need nothing more.

**History (context only, don't resurrect these):** the pattern converged
in this order: (1) manager owns a background thread that polls
`history.txt` and writes `session/wraith_body.txt` directly → worked,
declared "production ready" → (2) found to cause the one-frame lag
above → (3) corrected to manager=init-only +
`ops/wraith_project_input.c`=hot-path, confirmed via two independent
investigations same evening. Also settled during this history: the
real file Wraith reads for a hosted project's window body content is
`session/wraith_body.txt` (plain text, no `${var}` substitution) — this
file-naming fact stayed true across all three iterations above.

---

## 2. GL / ASCII Rendering & Nav Mechanism Facts

- **Real clickable nav in Wraith-hosted GL views comes from
  `scene.objects.pdl` `OBJECT tag=control ... nav=N action=KEY:N` lines
  — not `<button>` tags in a `.chtpm` layout.** Button tags are dead
  code for GL rendering of hosted-project content; only the
  `objects.pdl` control rows are real.
- Nav index collision rule: indices 1–4 are reserved for window chrome;
  index 5 is shared between a project's own `Control_Map` and the first
  nested-launcher entry unless `extra_project_slots` accounts for the
  extra controls.
- **Nav-index/`launcher_start` math has drifted across sessions before**
  — one session found "exactly two" hardcoded sites, a later session
  re-grepped and found three. Treat any single document's site count as
  stale; re-grep fresh every time before touching this arithmetic.
- `wraith_rgb_daemon.c`'s `tile_zmap` object role only renders terrain
  plus (if present) one `selected=X,Y` highlighted tile — it early-`return`s
  and never renders generic label/HUD text. Any HUD/coordinate text
  needed in a GL tile view must be a separate `tag=text` object
  positioned outside the `tile_zmap` object's bounds, not baked into its
  label.
- `wraith_rgb_daemon.c` may run as a persistent background process not
  restarted per-window — after recompiling it, a full Wraith restart may
  be needed, not just closing/reopening the affected project's window.
- ASCII mode and GL mode are two almost-entirely-separate rendering
  pipelines (`current_frame.txt` via the parser's `render_element()`,
  built by pure sequential `strcat()` with no coordinate concept at all,
  vs. `current_frame.objects.pdl` via `wraith-alpha_manager.c`'s
  `write_semantic_projection_files()`, explicit `x=/y=/w=/h=/nav=/action=`
  per element). **Any rendering or click-handling change must be
  verified against both pipelines separately** — this has caused
  multiple real, separate bugs across sessions where a fix landed on
  only one side.
- **Fork-duplication fact:** `wraith_parser_alpha.c` (~2700 lines) is a
  separate reimplementation of the classic `pieces/chtpm/plugins/chtpm_parser.c`
  parser, not a thin wrapper around it — the two files independently
  duplicate most of the same parsing/rendering/state logic. As of
  2026-07-06 (see `2fix-july6.txt` in the parent dev-env directory, bug
  4), **`chtpm_parser.c` — not `wraith_parser_alpha.c` — was confirmed
  to be the actual live parser process for wraith-alpha's sessions**,
  contrary to what the name suggests. Any fix to cli_io/state-loading
  behavior needs to be checked against (and usually ported to) both
  files to keep them in sync, but verify which one is *actually running*
  before assuming a code change takes effect.

---

## 3. RGB / Voxel Rendering Architecture (piececraft-wraith's 3D mode)

**Locked decision: no OpenGL, ever.** The target is a direct RGB/framebuffer
signal (`current_frame.rgba32` — raw headerless RGBA32, `WIDTH×HEIGHT×4`
bytes, written at up to 60fps; deliberately binary for size/parse-cost
reasons, with `current_frame.png` serving human-inspectable needs
separately). OpenCL is allowed later, but only as pure compute — it must
never touch the display path.

**Current final rendering model** (converged after a chronological
pivot — see History below): **ray-march the solid tile/voxel grid**
(near-to-far DDA walk, first hit wins — structurally immune to
painter's-order/Z-fight bugs, since the world is a small regular
axis-aligned grid, a Minecraft-shaped problem). Rasterize+depth-test
only the small number of overlay entities (xelector/pet) and the
translucent-tile pass, sharing one scoped depth buffer with the ray
marcher.

**Ground-plane / tile conventions:**
- `wy=0` is the walkable ground-plane datum. **Walkable tiles render as
  one shared wireframe grid sized to the whole map, drawn once** — they
  are NOT individually extruded boxes. Only non-walkable (solid) tiles
  get real extruded boxes, and they always extrude **up** from `wy=0`.
  Entities (xelector/pet) also extrude up from the ground plane.
  - **Correction, supersedes an earlier design:** a dedicated
    `extrude_dir=up|down` per-tile field was added at one point, then
    removed entirely (no longer in `TileMeta` or any `.tile.txt`) once
    the shared-wireframe-floor approach replaced per-tile floor
    extrusion. If you see `extrude_dir` referenced anywhere, it's stale.
- Tile shape/color/height come from `assets/tiles/registry.txt` +
  `assets/tiles/<id>.tile.txt` — data-driven, not hardcoded per-glyph
  switch statements ("everything is a cube unless specified," including
  the fallback path). Watch out: `#`-prefixed lines are usually
  comments *except* `#=wall` (real registry data) — a real bug was once
  caused by this ambiguity.
- Voxel tiles can declare `voxel_source=<csv>` for a real per-pixel
  shape (same CSV convention used by emoji-studio — see `# resolution=N`
  / `# scale=1.0` / `# transform=x,y,z` header + row-major `r,g,b,a`
  pixel rows, alpha=occupancy/RGB=material), falling back to a flat
  colored box when absent.
- `draw_box()` draws all 6 faces with true per-face visibility (via
  `unrotate_by_yaw()`) — an earlier version only drew 2 of 6 faces,
  causing a "paper-thin pillars" look; this is fixed.

**Camera:**
- `camera_mode` 1/2/3 presets are still fixed constant tuples — they do
  **not** yet follow the xelector's live position (no true first/third-person
  yet). Free-cam (WASD pan) is its own independent mode. This remains an
  open TODO.
- Mouse-orbit exists (gated to map-control/interact mode): drag →
  `cam_yaw`/`cam_pitch`, forwarded through the same `KEY_PRESSED`-style
  history path as `MOUSE_DRAG: dx dy`.
- `assets/camera_default.txt` (`key=value`) + `reset_project_view_from_default()`
  (called from `launch_window_instance()`, so it fires on every window
  open, not just once per session) resets only the listed camera-related
  keys, leaving `xel_*`/`display_mode`/etc. untouched — a generic,
  data-driven reset mechanism any project can opt into.
- A render-clip-vs-render-beyond-viewport toggle exists at
  `pieces/config/wraith_debug.conf`'s `render_beyond_viewport` key.

**Known deferred gaps:** Z-level visual continuity between levels is
untuned; camera-follow (above) not done.

**History (context only):** the renderer went through rasterization →
Z-buffer → near-plane clipping → the ray-marching pivot described above,
in that order, across ~3 days. The pivot to ray marching for the solid
grid specifically (keeping rasterization only for overlays/translucency)
is the reason several earlier "how to fix Z-fighting" investigations in
the old files are no longer relevant.

---

## 4. Terminal Project — Documented Stub, Not a Bug

The `terminal` Wraith project has only `project.pdl` + layout +
`README` — no manager, no `ops/`, no `session/`. Its window and two nav
buttons work today (backed by generic `DESKTOP_ACTION`/`href` desktop
chrome, not project-specific code), but **there is no real shell/command
execution** — nothing consumes `history.txt` to actually run a command.
This is a confirmed, intentional stub, not a hidden bug (owner has said
to leave it alone — see the manager-migration status note below).
Finishing it would need: `manager/terminal_manager.c` (fork/exec a real
shell), `ops/` input-forwarding, standard `session/` files, and a
`META | manager | ...` line — none of that exists yet.

Note: the toolbar "Terminal" and the launcher-row "WRAITH TERMINAL" are
confirmed (via call-path tracing) to be the exact same project/window,
not two separate implementations — don't waste time looking for a
second one.

---

## 5. Manager-Migration Status Snapshot (as of 2026-07-03 — reverify, don't trust blindly)

At last check, 9 of ~14 Wraith-hosted projects were compliant with the
manager/ops pattern in section 1. Notes that may still apply:
- `blank-project` was deleted (it was scaffolding only).
- `probe-project` and `terminal` — owner said low-priority/ignore.
- `emoji-studio-wraith` still needed its manager added (mechanical, low
  priority) as of this snapshot.
- `wraith-3d-cube`'s own docs were found to be **wrong** at the time —
  it's supposed to migrate from a single-cube probe to a tile-based map
  like piececraft-wraith; unclear if this has since happened.
- `web-cam` is a special case using POSIX shared memory
  (`tpmos_share_kvp_runtime.c` — see the short-term-vision doc's Media
  section) — audited and confirmed safe as its own pattern, not a gap.

This is a point-in-time snapshot from one audit pass — re-grep the
actual project directories rather than trusting these numbers if
precision matters.

---

## 6. Emoji-as-Voxel-Entity Feature (design only, not yet implemented)

Investigated but not built: using emoji as 2D/3D entities via voxel-CSV
extrusion generated from a glyph atlas (same CSV convention as section 3's
`voxel_source=`).

**State of prior art surveyed:**
- A standalone GLUT/FreeType proof-of-concept (atlas→CSV pipeline) is
  real and working, outside TPMOS.
- `projects/emoji-studio` is an abandoned/incomplete port — read-only
  reference only, not TPMOS-wired.
- `emoji-studio-wraith` is the real intended Wraith project — has a good
  spec (its own `ASSUMPTIONS.md`), a real sample CSV, and a
  `rgba_extrusion` object contract, but **no manager and no
  `ops/wraith_project_input.c`**, so `EMOJI_RES:`/`INTERACT` currently
  no-op. `wraith_rgb_daemon.c` already has a stub renderer
  (`draw_rgba_extrusion_preview()`) that self-documents as a fake
  "flat_extrusion_preview," not real camera projection.

**Recommended build order, if resumed:** decide live-generation
(needs the codebase's first-ever external lib dependency, FreeType) vs.
a pre-baked fixed asset set → give `emoji-studio-wraith` its manager
(section 1's template) → write its `ops/wraith_project_input.c` →
extract a shared `draw_voxel_grid_extrusion()` helper reusing
`draw_box()`/`project_world_point()`/existing camera parsing from
piececraft-wraith → only then wire an optional `voxel_source=` into
tile files generally.

**Not yet decided — ask before guessing:** live FreeType generation vs.
fixed pre-baked set; uniform bar-chart extrusion vs. real relief;
whether `emoji-studio-wraith` becomes a shared asset generator/library
for other projects.
