# Board-viewer 3D — the performance ceiling, and how to get to Minecraft-class

**Written 2026-09-09**, direct instruction: *"how do we achieve
minecraft levels of performance? mc is in java and shows much more
moving voxels & logic on screen: this should be faster, not slower. we
need to research and document this now."*

Companion to `&.widgits/board-viewer/mc-speed-algos.md` (the
incremental CPU-side fixes so far). This doc is the honest architectural
answer: **the current approach cannot reach 60 fps 3D at the pc-hq
canvas size, no matter how much the raymarch is tuned** — and what it
would actually take.

---

## 1. Measured reality (2026-09-09, this box)

Hardware: AMD Ryzen APU, **8 CPU cores**, **AMD Raven iGPU (Vega),
Mesa 23.2, OpenGL 4.6**, `/dev/dri/renderD128` present, `libEGL` /
`libgbm` / `libGLESv2` installed. GLX + EGL-on-X11 both initialise
(pbuffer + surfaceless configs advertised). `clinfo` = 0 platforms
(no OpenCL) — but **OpenGL compute / shaders are fully available and
currently unused**.

One `bv_render_3d` frame at 1108×656 (the pc-hq "TV" canvas):

| stage | cost | share | notes |
|---|---|---|---|
| fork+exec ×4 ops (`bv_dispatch` → `bv_menu_input` / `bv_render_3d` / `bv_compose_frame`) | **< 5 ms** | ~2% | measured `4 × /bin/true` = 0.00s — **process spawn is NOT the problem** |
| re-read + parse 32 chunk z-layer text files + legend + entities + hero + phymoji CSVs | ~20–40 ms | ~15% | 132 KB re-parsed **every frame**; pure waste (static data) |
| **CPU per-pixel raymarch** (727 k rays, DDA + AABB slab tests, `double`) | **~150–200 ms** | **~80%** | `PROBE_RAYONLY` (skip everything after ray setup) = 0.05s; so raymarch ≈ 0.20s of a 0.25s frame |
| `calloc` 2.9 MB framebuffer + `write_file_atomic` 2.9 MB | ~5 ms | ~2% | |
| `bv_compose_frame` (text/marker) | ~0 ms | — | |

Wall time: **~0.10 s on an idle box, ~0.25–0.45 s under load** (Chrome
etc. starving the OpenMP region). Coalesced, so a key *tap* renders
once; a *held* key streams coarse frames at ~4–10 fps.

`bv_render_2d` (the flat / CJK view) is **~0.03 s** — a non-issue.

### The syscall / IO red herring

`strace -c` one frame = **0.023 s total in syscalls**. `bv_dispatch`
idle tick = 0.00s. There is no file-IO bottleneck; an in-memory-DB
shim (wraith-alpha style) would remove nothing. See
`[[board-viewer-3d-perf-not-io-bound]]`.

---

## 2. Why Minecraft shows more and runs faster

It is not "C vs Java". It is a different **class of algorithm** on
different **hardware**:

| | board-viewer today | Minecraft (and every real voxel engine) |
|---|---|---|
| where pixels are computed | 8 CPU cores | **GPU** (100s–1000s of ALUs) |
| what is computed per frame | **per-pixel raymarch**, O(pixels × steps) ≈ 727 k × ~30 | **rasterise a pre-built mesh**, O(visible triangles) ≈ a few thousand |
| chunk geometry | re-derived from text every frame | **meshed once** on block change → VBO on the GPU, re-drawn for free |
| world state | nothing retained between frames (new process each frame) | resident in RAM for the process lifetime |
| culling | one coarse board-AABB test per ray | frustum + face + (often) occlusion culling before anything is drawn |
| per-frame CPU | ~0.20 s | ~1–3 ms (just camera + visible-chunk list + draw calls) |

Minecraft's *per-frame CPU* budget is roughly **100× smaller** than
board-viewer's, and it hands the actual pixel work to a chip that is
another **~50–100×** faster at it. That product — not the language — is
the gap.

---

## 3. What caps the current design

1. **Per-pixel CPU raymarch.** Even fully tuned (`float`, perfect
   culling, SIMD) the floor is ~O(pixels) ≈ **80–100 ms** at this
   canvas on this CPU. That is already ~6–10 fps *before* any game
   logic, and it grows with the window.
2. **No persistent state.** The process-per-frame model (`loop: exec
   bv_dispatch ; sleep 16667`) means the voxel grid, any mesh, the
   framebuffer, and the OpenMP thread pool are all rebuilt every
   frame. Spawn itself is cheap (measured); the *re-derivation* is the
   ~15% waste, and it structurally forbids "mesh once, draw many".
3. **GPU unused.** The iGPU + EGL/GLX stack is present. Every frame of
   raymarch work it does could be done there ~2 orders of magnitude
   faster.
4. **Fixed to the window size.** `g_fw × g_fh` tracks the canvas, so a
   bigger window is quadratically slower — the opposite of a mesh
   renderer, whose cost barely moves with resolution.

---

## 4. The paths, with honest effort / payoff

### Path A — GPU raymarch in a persistent process  *(recommended target)*

A new long-lived binary, e.g. `bv_gpu_render`, launched once per
session by the pal boot instead of the per-frame `bv_render_3d`:

- EGL context on `EGL_MESA_platform_surfaceless` or `platform_gbm`
  (`/dev/dri/renderD128`; needs the `render` group — or fall back to
  EGL-on-X11 + a pbuffer, which initialises fine here today).
- Upload the voxel grid **once** as a 3D texture (`R8UI`, 17×16×32
  today, room for 256³) or an SSBO; re-upload only the columns that
  changed (mining/building) — a marker file or a small "dirty rects"
  channel.
- Port the existing Amanatides–Woo DDA + AABB logic **verbatim** into
  a fragment or compute shader (it is ~120 lines of already-correct
  C; GLSL is a near-mechanical translation). Camera + sun + xelector
  come in as uniforms each frame.
- Render to an FBO; hand the RGBA to `khtpm` via **POSIX shared
  memory** (`shm_open` + the khtpm canvas element reads the segment
  instead of a `.raw` path) — or, to touch less code first, keep
  writing `rgb_frame_3d_overlay.raw` and only move to shm later.
- The pal loop shrinks to: write camera/state deltas to a tiny file,
  bump a marker; the GPU process renders on the marker.

**Payoff:** the raymarch stops being the bottleneck entirely.
Expect **hundreds of fps** at the current canvas, resolution-scalable,
with headroom for real shadow rays / more entities / bigger chunks —
i.e. "Minecraft-class" for *this* rendering style.
**Cost:** ~500–1000 lines of EGL + GLSL; one documented persistent
process for this widget; a shm or marker handoff. This is the real
answer and the only one that reaches the stated goal.

### Path B — CPU greedy-mesh + software rasteriser

Greedy-mesh the visible chunk (a flat grass surface + a few trees ≈
hundreds–low-thousands of quads), rasterise with a z-buffer. O(quads),
not O(rays); cost barely scales with window size.

**Payoff:** ~**5–20×** (→ ~20–50 fps while moving). Keeps everything
CPU-only and (if you accept re-meshing per frame) needs no persistent
process — though caching the mesh in a resident process is where most
of the win is.
**Cost:** a greedy mesher + a triangle rasteriser with perspective-
correct interpolation and a depth buffer (~600–900 lines). Still no
60 fps guarantee at large canvases; a stepping stone, not the
destination.

### Path C — persistent CPU raymarch process

Today's raymarch, moved into a long-lived process: world grid +
framebuffer + warm thread pool retained, no per-frame parse.

**Payoff:** ~**1.3–1.5×** (the parse + cold-pool savings only).
**Cost:** the process-lifecycle plumbing (which Path A needs anyway).
Not worth doing alone — do it *as* the vehicle for A or B.

### Path D — stop rendering 3D at this fidelity  *(product, not engineering)*

`bv_render_2d` is already **0.03 s / ~30 fps**. Make 2D the default
(`arrow_config.txt: default_render_mode=0`), treat 3D as an on-demand
"look around" that is allowed to be ~10 fps, and stop paying the cost
on every session.
**Payoff:** the *felt* problem largely goes away for free.
**Cost:** zero code; a decision about what the board window is for.

---

## 5. Recommendation

**Path D now** (make 2D the default — it removes the daily pain for
nothing), **Path A as the real project** (GPU raymarch in a resident
process — the hardware is sitting right there, and it keeps the exact
voxel model + DDA the CPU code already proves correct). Path B only if
a persistent process is ruled out on principle; Path C only folded
into A.

The house's "every op is a short-lived process" rule is what stands
between here and the goal. `mc-speed-algos.md` §6 already flags "true
GPU rendering … requires a persistent process … conflicts with this
house's own convention" as acceptable-if-revisited. This is the
revisit: **one persistent, sandboxed render process for one widget**,
talking to the pal loop through the same file/marker channel
everything else already uses.

---

## 6. Also: reconsider the 2026-09-09 CPU-pass tradeoffs

Two changes from this session's incremental pass can read as
"inaccurate / slower" and should be revisited when Path A lands (or
sooner):

- **`RELAY_STALE_MS = 120`** (stale-key drop, commit `7f2fa6fc`).
  Under load a frame is 250–450 ms, so *every* queued key after the
  first ages past 120 ms and is dropped → the xelector under-moves and
  feels unresponsive. Fix: make the window track real latency —
  `stale_ms = max(150, 2 × last_3d_render_ms)` (the render time is
  already recorded in `.bv_dispatch_3d_ms`).
- **`motion_lod_step = 2`** (adaptive resolution, commit `8dda558f`).
  The block-filled motion frames look blocky — the intended tradeoff,
  but net-negative if 3D isn't the primary view. Consider shipping
  `motion_lod_step = 1` (off) until Path A, or gate it on "3D is the
  default view".

---

## 7. Related

- `&.widgits/board-viewer/mc-speed-algos.md` — the incremental CPU
  fixes (empty-space skip, OpenMP, DDA volume-clamp, phymoji column
  DDA, adaptive resolution) and §7's 2026-09-09 profiling.
- `[[board-viewer-3d-perf-not-io-bound]]` (memory) — the "is it file
  IO?" question, answered: no.
- `[[tpmos-reference-location]]` — the 60 Hz pulse-marker render-chain
  standard the pal loop follows.
- `08-roadmap/design-docs/PCHQ-2D-TILE-VIEW.md` — the 2D view that is
  already fast and could become the default (Path D).
- `CENTROID_GOLD_STD.md` / the "no resident process" convention — the
  rule Path A asks to make one scoped exception to.
