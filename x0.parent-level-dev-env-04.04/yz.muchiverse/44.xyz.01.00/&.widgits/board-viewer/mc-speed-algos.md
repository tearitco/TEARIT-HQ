# MC-SPEED-ALGOS — why the raymarcher was slow, and the real fix

Written 2026-08-03, direct user report while testing piececraft-xyz's
newly-built true multi-layer voxel rendering: "everything looks really
great but... the render is super slow compared to civ / mutaclysm. why
is this happening. minecraft is able to render raymarching much faster
and more than this? even mutaclysm can render more than this, were not
rendering that much so what is it?"

This doc records the real investigation, the real numbers, and the real
fix - so the next person who touches `bv_render_3d.c` (or any future
raymarch-based renderer in this house) doesn't have to re-derive any of
this from scratch.

---

## 1. Why real Minecraft doesn't have this problem at all

Direct answer to "how does minecraft handle this" - it's not a faster
raymarcher. **Minecraft doesn't raymarch per-pixel on the CPU at all.**

- It builds a real **triangle mesh** per chunk section (16x16x16 blocks),
  generating actual polygon geometry only for block *faces that touch
  air* - two solid blocks sharing a face never generate geometry for
  that shared face at all ("face culling"). The interior of a solid
  mountain produces zero triangles.
- That mesh gets handed to the **GPU**, which rasterizes it in hardware -
  a fundamentally different, massively-parallel-by-design pipeline, not
  a single CPU thread testing ray/box intersections one pixel at a time.
- The mesh is **cached** and only rebuilt when the chunk's own block data
  actually changes (a real place/break event) - not every single frame.

Mutaclysm's own raymarcher (this file's real ancestor) is fast for a much
simpler reason: its own board is small and flat (a single 2D heightfield,
not a real multi-layer voxel volume) - a tiny search space, not a
cleverer algorithm.

`bv_render_3d.c` is architecturally closer to a **software raymarcher**
(CPU-side, per-pixel, re-invoked as a brand-new short-lived process
roughly every 30ms per this house's own "no resident process, every op
is a short-lived file-reading/file-writing binary" convention - see
PIECECRAFT_XYZ_DESIGN.md's own compression/decompression section for
that house-wide rule stated directly). A full GPU-mesh-cache rewrite
would mean a real architectural change (a persistent process holding
mesh buffers, real OpenGL/Vulkan calls) - out of scope for this pass,
flagged as real future work if this house's "no resident process"
convention is ever revisited for this specific widget.

---

## 2. What was ACTUALLY making this instance slow (measured, not guessed)

Real numbers, `bv_render_3d.+x` timed directly (`/usr/bin/time`), same
camera/first-person view, before ANY fix in this doc:

| Host | Time/frame | Notes |
|---|---|---|
| civ-txt (untouched, single flat board.txt, always existed) | ~0.8s | Baseline - present BEFORE piececraft-xyz's own real multi-layer voxel work started, not something this session's own changes caused |
| piececraft-xyz (real 32-layer chunk, naive 3-axis per-voxel DDA) | ~1.3s | ~0.5s more than civ-txt's own baseline |

**`strace -c` on the civ-txt render showed total syscall time of ~0.026
seconds** - I/O (file reads, texture CSV parsing) is NOT the bottleneck.
The ~0.8s baseline (present even for a TINY flat single-layer board) is
almost entirely pure CPU floating-point cost: `FRAME_W * FRAME_H` =
640*480 = 307,200 pixels, each doing multiple real ray/AABB slab tests
(legend cube, up to several entities, coarse board bbox, then the actual
per-cell DDA) - all real division-heavy math, single-threaded.

piececraft-xyz's own extra ~0.5s came from the naive 3-axis (col, row,
lvl) DDA added when true multi-layer voxels were built: instead of the
old 2-axis (col, row) walk (at most ~32 steps for a 16x16 board), a ray
crossing a tall, mostly-uniform column (open sky above the surface,
solid stone below it) now stepped through the Y axis one real unit-cube
test at a time - up to `board_w + board_h + z_count + 32` steps
per-pixel worst case, most of them wasted walking through guaranteed-
uniform air or stone that a smarter algorithm never needed to touch
individually.

---

## 3. Real fix #1 - empty-space skipping (the CPU-raymarch analog of face culling)

Direct precedent for this technique: real voxel raymarchers (and
conceptually Minecraft's own face-culling) never test empty/uniform
regions one unit at a time - they skip past them in one jump. Applied
here:

- **Once per frame** (not per pixel - cheap, `board_w * board_h *
  z_count` worst case, ~8,192 operations for a 16x16x32 chunk), compute
  for every column `(col, row)`:
  - `col_top` / `col_bottom` - the real topmost/bottommost solid voxel Z.
  - `col_solid` - a real, VERIFIED boolean: is every voxel between
    `col_bottom` and `col_top` actually solid, with no gap?
- The per-pixel DDA goes back to walking only 2 axes (`col`, `row`) -
  the same cost class as the ORIGINAL single-slice version.
- For a column with `col_solid == 1` (true for every column in today's
  world-gen - `pc_generate_chunk.c` only ever produces one contiguous
  solid run per column: stone → dirt → grass, air above), the ENTIRE
  column tests as **one merged box** - O(1), not O(z_count).
- For a column with `col_solid == 0` (impossible today, real once
  Phase 2 mining can carve actual holes), the code falls back to a true
  per-voxel walk - but ONLY across that one column's own real solid
  range, never the whole grid. **This is a real, checked fallback, not
  an assumption** - direct user concern addressed explicitly: "is it
  gonna make things look weird and see thru sometimes? i dont like
  that." It can't, because `col_solid` is computed by actually scanning
  for gaps, not just trusting there aren't any.
- The exact entered Z on a hit (needed for correct grass-on-top vs.
  dirt/stone-lower-down texturing on a side/cutaway view) is computed
  from the ray's own real hit point, not assumed from `col_top` alone -
  so a side view through a merged column still shows the real per-layer
  material at whatever height the ray actually crossed.

**This alone did not fully explain the gap to real usable speed** - see
part 2 below.

---

## 4. Real fix #2 - OpenMP, not OpenCL (only one of them actually works here)

Direct question worth answering explicitly: **"wait cant we use open-cl
instead?"** - investigated, and rejected for a concrete, checked reason:

```
$ clinfo
Number of platforms   0
```

The OpenCL loader and headers (`libOpenCL.so`, `/usr/include/CL/`) ARE
installed in this environment, but **zero actual platforms/devices are
registered** - no GPU driver, no CPU-based OpenCL runtime (e.g. pocl).
An OpenCL kernel would compile cleanly and then fail at runtime with
"no platform found." Writing one here would be real wasted work, not a
real option, in THIS specific environment.

**OpenMP, by contrast, is real and available**: `libgomp.so.1` is
installed, `gcc -fopenmp` compiles and links successfully, and this
machine has **8 real CPU cores** (`nproc` = 8). Each pixel's own ray is
genuinely independent - reads only shared, already-fully-loaded,
read-only state (camera, voxel grid, per-column intervals, terrain
legend, entities, xelector), writes only to its own disjoint
`fb[sy][sx]` framebuffer cell. This is the exact same "many independent
per-pixel computations" property real GPUs exploit for rasterization -
just applied to the CPU cores actually present here instead of a GPU
that isn't.

**One real thread-safety hazard found and fixed before parallelizing**:
`get_voxel8_cached()` mutates a shared global texture cache
(`g_voxel8_cache[]`/`g_voxel8_cache_count`) on a cache MISS (real
`fopen`+parse+insert). Safe from one thread; a genuine data race if two
worker threads both miss the SAME uncached texture path at once. Real
fix: a single-threaded **warm-up pass** right before the parallel
region, populating the cache for every distinct terrain-legend and
entity texture path this frame could possibly need. By the time the
parallel loop runs, every real `get_voxel8_cached()` call is a pure
cache HIT (read-only, genuinely safe to share across threads with zero
locking).

Implementation: `#pragma omp parallel for schedule(dynamic, 4)` on the
outer `for (int sy = 0; sy < FRAME_H; sy++)` loop, `-fopenmp` added to
both compile and link flags in `scripts/build.sh`.

---

## 5. Real measured results (same camera, same seed's terrain)

| Host | Before (naive 3-axis DDA, single-threaded) | After (merged-column + OpenMP) |
|---|---|---|
| civ-txt | ~0.8s | ~0.02-0.03s |
| piececraft-xyz (real 32-layer chunk) | ~1.3s | ~0.03-0.04s |

**Pixel output verified byte-identical before/after** (same exact color
histogram, same pixel counts) - this is a pure performance fix, zero
visual change, confirmed by direct comparison, not assumed.

Roughly a **35-40x speedup** for piececraft-xyz's own real multi-layer
voxel rendering, and both hosts landed in the same ~0.02-0.04s range -
the empty-space-skip fix alone made piececraft-xyz's own cost
comparable to civ-txt's; OpenMP then cut BOTH down further together.

---

## 6. When this breaks / real future work

- **`col_solid` becomes false somewhere once Phase 2 (mining/placing)
  can carve real holes.** The code already handles this correctly (a
  real per-column fallback, not a crash or a visual bug) - but a world
  with MANY mined-out columns would see MORE columns take the slower
  per-voxel fallback path, and average frame cost would rise somewhat.
  If that ever becomes a real problem, the right next step is a
  **multi-interval list per column** (not a full revert to per-voxel
  walking) - the same empty-space-skipping principle, generalized to
  handle more than one contiguous solid run per column (e.g. a real
  cave: solid, gap, solid again).
- **True GPU rendering** (a real triangle mesh + hardware rasterizer,
  matching what Minecraft actually does) would be the "real" long-term
  answer, but requires a genuine architectural change (a persistent
  process holding mesh state, real OpenGL/Vulkan integration) that
  conflicts with this house's own "every op is short-lived, no resident
  process" convention as currently practiced. Flagged as real future
  work if that convention is ever revisited for this specific widget,
  not attempted this session.
- **OpenCL remains unavailable** in this specific environment
  (`clinfo` = 0 platforms) - if this code ever runs somewhere with a
  real GPU/OpenCL runtime present, OpenCL (or a real GPU compute path)
  would likely outperform OpenMP's CPU-only parallelism further, but
  there's nothing to gain from writing that code against hardware that
  doesn't exist here.

---

## 7. 2026-09-09 — second pass + the "is it file I/O?" question

Re-profiled `bv_render_3d` at the pc-hq TV canvas (1108x656, up from
the old fixed 640x480 after the "open the view" change). Reported as
"slow again with key input".

### What it is NOT

- **NOT file I/O.** `strace -c` on one frame: 0.023s total in
  syscalls (3 writes, 166 reads, 88 opens). `bv_dispatch` idle tick =
  0.00s (coalesce works - the raymarch only runs when an input burst
  settles). `bv_compose_frame` = 0.00s. `bv_render_2d` = 0.03s. The
  wraith-alpha "in-memory DB that fakes file read/write, flushes to
  disk once a minute" shim would remove **nothing measurable** here -
  there is no I/O bottleneck in this pipeline. Do not reach for it for
  board-viewer.
- **NOT thread count.** `nproc` = 8; libgomp default already uses all
  8. `OMP_NUM_THREADS=8` vs unset: identical (~0.10s).
- **NOT a new algorithmic bug from the bigger canvas.** Cost scales
  cleanly linear with pixel count (~2.3 CPU-us/pixel).

### What it IS

- The **raymarch itself**, plus **machine contention**. On an idle box
  (loadavg ~5) the frame is **~0.10s wall** on 8 cores. The 0.25-0.47s
  numbers that triggered the report were measured while Chrome held
  ~74% CPU across cores (loadavg ~18) - the OpenMP region was
  core-starved. ~0.10s = ~10 fps during *sustained* 3D camera motion;
  coalescing means a fast key burst renders once, not per key.

### Fixes landed this pass (all byte-identical output, verified)

| commit | change | effect |
|---|---|---|
| `33e5c144` | A&W volume clamp: capture the ray's *exit* t from the board bbox, stop the DDA there (kills the ~90-step empty tail every grazing/sky ray walked). `bv_render_3d` also gets `-O3 -march=native -funroll-loops` (was `-O2`). | 1.65 -> 1.36s user @ default cam; bigger win panned-back |
| `d94c04da` | gate the per-pixel foreground tests (xelector/hero/tree/entity) on `board_bbox_hit` - sky rays skip ~10 AABB tests + tree voxel marches | no change at top-down cam; win panned-back |
| `dde14725` | **`test_phymoji_hit` was ~30% of the frame** - it slab-tested every (lx,ly) column of every tree/hero per covered pixel (~425 double AABB for a TILE_N=32 tree). Replaced with a local 2D column-grid DDA (`PHYMOJI_GRID_DIM=32`, built once per template), front-to-back, stop at first hit. Same A&W as the board. | 1.41 -> 1.19s user |

Cumulative: ~1.65 -> ~1.19s user / ~0.16 -> ~0.10s wall on an idle
8-core box, **output byte-identical throughout**, no resolution cap,
no letterbox, no FOV change.

### Left on the table (not done - diminishing returns at ~0.10s)

- `-ffast-math` measured ~10% more. Skipped: the loop uses `1e17`/
  `1e18` as *finite* "no hit" sentinels; `-ffinite-math-only` around
  them plus `-fassociative-math` on the `t_max += t_delta`
  accumulation is a footgun for 10%.
- Float conversion of the per-pixel path: **tried, measured, does not
  help** - a float-internal `ray_aabb_hit_3d` built the same frame
  byte-identical at the same speed. The FP slab math is not the
  bottleneck; memory-bound array access + the (now-fixed) phymoji
  march were.
- Adaptive resolution - **DONE** (commit after this doc). `bv_dispatch`
  writes `pieces/display/.bv_render_lod` = 1 for a mid-burst coalesced
  refresh (camera still moving), 0 for the settled / host-driven frame.
  A "moving" frame raymarches a `1/motion_lod_step` grid (host
  `arrow_config.txt`, default 2) and block-fills; the settled frame is
  full res. Measured: full 0.18s -> motion 0.06s (~3x) at 1108x656,
  ~9% of pixels differ (silhouette blocks only), full-res path
  byte-identical to before. Sustained 3D camera motion ~16 fps, snaps
  crisp the instant you stop.

---

## 8. The ceiling (2026-09-09) - see the dedicated doc

The incremental fixes in this file are all CPU-side and have a hard
floor: a per-pixel CPU raymarch is ~O(pixels) ≈ **80-100 ms** at the
pc-hq canvas even fully tuned. Reaching "Minecraft-class" (60+ fps,
more voxels, more logic) needs an **architectural** change - GPU
rendering in a persistent process - because MC rasterises a pre-built
chunk mesh on the GPU (O(visible quads), ~1-3 ms CPU/frame) while this
raymarches 727 k rays on 8 CPU cores. The AMD Raven iGPU + EGL/GLX
stack is present and unused. Full analysis, the four paths (GPU
raymarch / CPU mesh+raster / persistent CPU / make-2D-default), and the
recommendation are in
`#.#.calendar-dox/!.HQ-IQ-BOOK/08-roadmap/design-docs/BOARD-VIEWER-3D-PERF-CEILING.md`.

**2026-09-09: Path A v1+v2 landed.** `bv_render_3d --daemon` is a
resident EGL/GLES3 process that raymarches the voxel grid in a
fragment shader (`ops/bv_gpu_raymarch.c`, GLSL DDA ported from this
file). arrow_config.txt `use_gpu_render=1`; `bv_dispatch` bumps
`.gpu_render_req` instead of exec'ing a renderer. ~40 ms/frame
internal (~17 fps live during a held arrow, vs ~4 fps CPU),
CPU-fallback on any GL failure. v3 = cache the per-frame chunk parse +
PBO readback -> 100+ fps. See `BV-GPU-RENDER-DESIGN.md`.
