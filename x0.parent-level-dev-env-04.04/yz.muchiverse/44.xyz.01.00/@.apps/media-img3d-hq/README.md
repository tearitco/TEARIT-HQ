# media-img3d-hq — conversion skeleton (2026-09-08)

**Owner's call:** the image editor and the 3D/Blender-clone become
**ONE app** — a 2D view and a 3D view of the same project, sharing
**piececraft-hq-style camera controls** (mode toggle, drag-look,
scroll-zoom, WASD/arrow pan). They were only separate for build ease.

## Source (out-of-house-spec, unchanged for now)
- `44.xyz.01.00/103.media-studio/103.img-editor/`  — `HOW2_IMAGE.md`
- `44.xyz.01.00/103.media-studio/103.3d=blender-clone/` — `HOW2_BLEND.md`

## Target layout (house spec — build in `media-img3d-hq.xhtpm`)
- **File** menu row (New / Demo / open / save)
- **Mode toggle**: 2D | 3D (one button, camera + tool set swap)
- **Tool strip** (sidebar): 2D = B/E/G/R/I/H ; 3D = Select/Grab/Rotate/Scale
- **Canvas** (`<canvas>`): 2D = paint surface w/ transparency checker ;
  3D = viewport (grid, axes, meshes). Reuse `kh_draw_canvas` + a
  manager-fed framebuffer, same as the piececraft board.
- **Layers / Outliner** (right): 2D layers 1–6 + visibility ; 3D object list
- **Status** bar: tool, brush size / transform, zoom, fps

## Camera
Reuse the Interact-Mode relay + `&.widgits/board-viewer/ops/bv_menu_input.c`
camera dispatch (`ARROW_*=1000..1003`). See
`09-appendix/PLAN-pchq-interact-camera-pov.md`.

## Skeleton status
Compiles, launches, shows in HQ toys, round-trips one action. Real
layout + engine wiring: TODO(grok). Keep every `HOW2_*` feature.
Retire the two `103.media-studio/` source dirs (leave a pointer) once
this reaches parity.
