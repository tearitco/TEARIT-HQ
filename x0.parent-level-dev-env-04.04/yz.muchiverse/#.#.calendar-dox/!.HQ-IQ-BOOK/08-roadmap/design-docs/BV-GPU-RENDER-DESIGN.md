# bv_gpu_render — the GPU raymarch path (Path A)

**Written 2026-09-09**, direct instruction: *"lets design, document and
do a, now."* Path A from
`BOARD-VIEWER-3D-PERF-CEILING.md` — move the 3D raymarch off the 8 CPU
cores onto the AMD Raven iGPU, keeping the exact voxel model and DDA.

Status: **v1 + v2 LANDED 2026-09-09.** v1 = drop-in one-shot GPU
renderer (proved EGL-headless + the GLSL DDA port + camera parity, but
~0.25s/frame is all per-process driver init - a wash vs CPU). v2 =
`bv_render_3d --daemon`, a resident EGL context: **~40 ms/frame
internal, ~17 fps live end-to-end during a held arrow** (vs ~4 fps CPU
coalesced) - `bv_dispatch` bumps `.gpu_render_req` instead of exec'ing
a renderer, so the diamond loop no longer stalls on the raymarch.
**v3** (next) drives it toward 100+ fps: cache the per-frame chunk/
asset parse (only re-read on a change marker) + async PBO readback +
skip the 2.9 MB row-flip. v3 also adds phymoji voxel detail (SSBO) and
the shared-memory framebuffer handoff.

---

## 1. Why this works here

Probed 2026-09-09 on this box:

- `eglGetPlatformDisplayEXT(EGL_PLATFORM_SURFACELESS_MESA)` +
  `eglInitialize` → **succeeds** (Mesa 1.5, no window, no DRM-node
  perms). Verified with a 20-line C probe.
- Headers: `/usr/include/EGL/egl.h`, `/usr/include/GLES3/gl3.h`.
  Link: `-lEGL -lGLESv2`.
- GL: OpenGL 4.6 / **GLES 3.0+** on AMD Raven (Vega iGPU).

GLES 3.0 + a fragment-shader raymarch to an off-screen FBO is the
simplest portable path: no compute shader, no GBM, no X. `glReadPixels`
the FBO into the same `rgb_frame_3d_overlay.raw` the CPU renderer
writes → **zero consumer changes** for v1.

---

## 2. v1 — `bv_gpu_render.+x`, a drop-in for `bv_render_3d.+x`

Same contract: run once, read `pieces/system/bv_state.txt` +
`arrow_config.txt` + `#.desktop/pchq_board_view.txt`, write
`pieces/display/rgb_frame_3d_overlay.raw` (+ `.receipt.txt` with
`overlay_w`/`overlay_h`), exit 0. On **any** GL/EGL failure: print to
stderr and exit non-zero — `bv_dispatch` then falls back to
`bv_render_3d.+x` (CPU). Nothing breaks if the GPU path is unavailable.

### 2.1 CPU side (C)

Reuses `bv_render_3d.c`'s scene model. For v1 the needed loaders are
**copied verbatim** into `bv_gpu_render.c` with a pointer back (same
precedent as the per-project `chtpm_rgb_render.c` forks). v2 factors
them into `bv_scene.{h,c}` shared by both renderers — flagged so the
two camera models can't silently drift:

- `load_voxel_chunk()` → `board3d[lvl][row][col]`, `board_w`,
  `board_h`, `z_count`
- `load_terrain_legend()` → glyph→{r,g,b,asset_hex,name}
- `build_camera()` → eye / forward / right / up / focal (all 4
  camera modes, unchanged)
- `bv_state.txt` reads: `selector_x/y`, `current_z`, `camera_mode`,
  `cam_yaw`, `cam_pitch`, `cam_pan_*`, `render_mode`, `focused_project_root`
- `arrow_config.txt`: `fov_deg`, `lighting_enabled`, `tp_*`, `fp_*`
- `load_celestial_body()` (sun/moon), `load_xelector()`
- `#.desktop/pchq_board_view.txt` → frame `W×H` (same clamp:
  160..1280 × 120..960)
- `.bv_render_lod` (adaptive resolution) → still honoured: render the
  FBO at `W/step × H/step`, let the blit upscale, OR just always
  render full res on GPU (it's cheap enough that v1 can ignore LOD —
  decide from the measured number)

### 2.2 GL resources (built once per run in v1, once per lifetime in v2)

| resource | contents |
|---|---|
| EGL surfaceless display + GLES3 context, no surface | — |
| FBO + `GL_RGBA8` colour renderbuffer, `W×H` | render target |
| `voxel_grid` — `GL_R8UI` 3D texture, `(board_w, board_h, z_count)` | `texelFetch(grid, ivec3(col,row,lvl), 0).r` = the glyph byte |
| `legend_lut` — `GL_RGBA8` 2D texture `256×1` | byte → `.rgb` = colour/255, `.a` = 1.0 solid / 0.0 air. Built CPU-side from the legend; air glyphs (`_`, space, and any glyph not in the legend) → `.a = 0`. |
| fullscreen-triangle VAO (3 verts, no VBO needed — `gl_VertexID`) | — |

Entities/hero/trees/xelector/sun/moon for v1: passed as **uniform
arrays** of AABBs + flat colour (`vec3 box_min[80]`, `vec3 box_max[80]`,
`vec3 box_col[80]`, `int box_n`). Coarse boxes only — **no phymoji
voxel detail in v1** (trees render as green boxes). v3 uploads the
phymoji column data as an SSBO and ports `test_phymoji_hit`.

### 2.3 Fragment shader (GLSL ES 3.00)

```
in  vec2 v_ndc;                 // -1..1 from the fullscreen triangle
out vec4 frag;

uniform vec3  u_eye, u_fwd, u_right, u_up;
uniform float u_focal;
uniform vec2  u_res;
uniform ivec3 u_grid_dim;       // board_w, board_h, z_count
uniform highp usampler3D u_grid;
uniform sampler2D u_legend;     // 256x1
uniform float u_light;          // ground light level 0..1
uniform vec3  u_sky;
uniform int   u_box_n;
uniform vec3  u_box_min[80], u_box_max[80], u_box_col[80];

// ray for this pixel
vec2 px  = gl_FragCoord.xy;
float a  = (px.x - u_res.x*0.5) / u_focal;
float b  = (u_res.y*0.5 - px.y) / u_focal;
vec3 rd  = normalize(u_fwd + a*u_right + b*u_up);
vec3 ro  = u_eye;

// 1) nearest box hit (entities / xelector / sun / moon) - slab test
// 2) board-bbox slab test; if miss & no box hit -> sky
// 3) Amanatides-Woo 3D DDA over (col,row,lvl), <= board_w+board_h+z_count steps:
//      g = texelFetch(u_grid, ivec3(col,row,lvl), 0).r;
//      leg = texelFetch(u_legend, ivec2(int(g),0), 0);
//      if (leg.a > 0.5) { hit; colour = leg.rgb; face from last-stepped axis; break; }
// 4) shade: if !top-face colour *= 0.75;  colour *= u_light;  (box hits skip *u_light like the CPU sun)
// 5) frag = vec4(colour, 1.0);   sky -> frag = vec4(u_sky, 1.0);
```

The DDA is a near-mechanical port of `bv_render_3d.c`'s
`ray_aabb_hit_3d` + the `(col,row)` walk, done as a full 3-axis walk
(GPU doesn't need the `col_solid` column-merge optimisation — 65 texel
fetches per ray is free).

### 2.4 Handoff

`glReadPixels(0,0,W,H, GL_RGBA, GL_UNSIGNED_BYTE, buf)` →
`write_file_atomic("pieces/display/rgb_frame_3d_overlay.raw", buf, W*H*4)`
→ `write_overlay_receipt(...)`. GLES reads bottom-left origin; the CPU
renderer's buffer is top-left → **flip rows on readback** (or render
with `b` sign flipped — match whichever gives pixel parity with
`bv_render_3d`).

### 2.5 Wiring into `bv_dispatch`

`arrow_config.txt`: new `use_gpu_render=1` (default 0 until v1 is
proven). In `bv_dispatch`'s 3D branch:

```
if (use_gpu_render && exists("ops/+x/bv_gpu_render.+x")) {
    if (run_op("ops/+x/bv_gpu_render.+x") != 0)   // non-zero = GL init failed
        run_op("ops/+x/bv_render_3d.+x");         // CPU fallback
} else {
    run_op("ops/+x/bv_render_3d.+x");
}
```

Everything downstream (`.raw` + receipt + `frame_changed` marker +
projector `canvas_raw` + khtpm blit) is unchanged.

### 2.6 Build

`scripts/build.sh`: `gcc $CFLAGS -o ops/+x/bv_gpu_render.+x
ops/bv_gpu_render.c -lEGL -lGLESv2 -lm`. If the link fails on a box
without EGL, the script warns and continues — the CPU renderer stays
the default.

---

## 3. v1 acceptance

1. `bv_gpu_render.+x` run standalone in a live session produces
   `rgb_frame_3d_overlay.raw` whose terrain / sky / xelector visually
   match `bv_render_3d`'s output for the same `bv_state.txt` (side-by-
   side PNG). Trees/hero as flat boxes is expected.
2. Wall time **< 30 ms** for a full-res frame (vs ~100–250 ms CPU).
3. `use_gpu_render=0` or a missing/failing binary → identical
   behaviour to today (CPU).

## 4. v2 — resident process (the real Path A)

`bv_gpu_render` becomes a daemon: EGL context + GL resources created
**once** at session boot, then `loop { wait on .gpu_render_req marker;
re-read bv_state; re-upload only changed voxel columns; render;
glReadPixels; write .raw; append frame_changed }`. `bv_dispatch` stops
`run_op`-ing a renderer for `render_mode==1` and instead bumps
`.gpu_render_req` (append-only) with the frame's LOD, non-blocking.
Expected steady-state: **< 10 ms/frame → 100+ fps**, context-creation
cost paid once.

Lifecycle: started by `button.sh` / the session orchestrator next to
`pchq_board_projector`; `SIGTERM` on session teardown; a stale-pid
guard like the projector's. This is the **one documented persistent
process** the perf-ceiling doc calls for.

## 4b. v3 measured (2026-09-09) - the readback-sync wall

v3 landed: shader renders top-down (no 2.9 MB row-flip memcpy), cached
uniform locations, `glTexSubImage2D` for the legend, and `.bv_render_lod`
plumbed into the GPU path (renders a `w/step x h/step` sub-rect of a
full-size FBO + nearest-upscale, so LOD never re-allocates the FBO).

Daemon now ~30-33 ms/frame (~31 fps internal, ~17 fps live). But the
per-frame breakdown is stuck:

| bucket | ms | what |
|---|---|---|
| load | ~5-8 | file reads (chunk/legend/entities/camera) |
| gpu  | **~15-17** | scene upload + draw + **`glReadPixels` (synchronous)** |
| write | ~9-13 | `write_file_atomic` 2.9 MB |

The `gpu` bucket does **not** shrink with LOD (step-2 = same ~15 ms).
On this AMD Mesa iGPU `glReadPixels` from a renderbuffer forces a full
pipeline flush + a blocking GPU->CPU copy, and that sync has a ~15 ms
fixed cost here regardless of pixel count. Every frame is fully
serial: draw -> sync-read -> CPU write -> next draw.

## 4c. v4 - the real 60+ fps push (not started)

1. **PBO + fence async readback.** `glReadPixels` into a `GL_PIXEL_PACK_BUFFER`,
   read frame N-1's PBO (with an `glFenceSync`) while frame N renders.
   Genuine pipelining - removes the ~15 ms serial stall. This is the
   single biggest remaining lever.
2. **Shared-memory framebuffer.** `shm_open` a `w*h*4` segment; the
   khtpm canvas element `mmap`s it instead of reading `.raw`. Removes
   the ~10 ms `write_file_atomic` per frame.
3. **Cache the scene load** behind a `bv_screen_changed` marker - only
   re-read the chunk/legend/entities when the host changes them
   (camera stays per-frame). ~5 ms.
4. Then LOD actually pays off (the raymarch is real work again once the
   sync stall is gone), and a bigger chunk / more entities have
   headroom.

## 5. v3

- Phymoji voxel detail: upload `PhymojiColumn` data as an SSBO, port
  `test_phymoji_hit` (the 2D column DDA) into the shader. Trees/hero/
  chicken regain their shaped silhouettes.
- Shared-memory handoff: `shm_open` a `W×H×4` segment, the khtpm
  canvas element `mmap`s it instead of reading `.raw` — removes the
  2.9 MB write+read per frame.
- Real sky gradient + sun disc + soft shadow rays (cheap on GPU).
- Frustum-cull the DDA start (skip rays whose bbox entry is behind the
  camera) — minor on GPU, free to add.

## 6. Related

- `BOARD-VIEWER-3D-PERF-CEILING.md` — the analysis this implements.
- `&.widgits/board-viewer/mc-speed-algos.md` — CPU-side history; §8.
- `&.widgits/board-viewer/ops/bv_render_3d.c` — the reference
  scene model + DDA being ported.
- `[[board-viewer-3d-perf-ceiling]]` (memory).
