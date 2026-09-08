# media-img3d-hq — combined 2D image + 3D viewport (house-spec)

**Owner's call:** one toy, 2D and 3D views, shared camera. Source glut
apps stay under `103.media-studio/` until this reaches full HOW2 parity.

## Layout (skeleton 2)

Sidebar: File tabs (New/Demo/Export), 2D|3D mode, tool strip, camera
row, layers/outliner. Panel: status + `<canvas sprite="${canvas_raw}">`.

## What works now

- Manager publishes `_ui.txt` and writes `state/canvas.raw` + receipt
  (`overlay_w/h=320x240`).
- 2D: checker composite, 6 layers, vis toggle (backspace on row),
  tools B/E/G/R/I/H, brush +/-, fg/bg swap, **STROKE** applies the
  active tool at canvas center (file-backed stand-in for drag-paint).
- 3D: software wireframe cube/sphere/ground, orbit/pan/zoom via CAM:*,
  select in outliner, Grab/Rot/Scl nudge with camera keys while that
  tool is active, +cube/+sphere.
- Actions only through `ops/media_img3d_hq_action.sh`. **Zero**
  `khtpm_core_render.c` changes.

## Still open vs HOW2

- Real canvas pointer paint / MMB orbit (needs a **generic** canvas
  click/drag attribute — do not add `g_is_media`).
- PNG/JPG drop import, ffmpeg export PNG, Assimp `.obj` import.
- Undo stack, eyedrop from true canvas coords.

## Test

```
bash 44.xyz.01.00/@.apps/media-img3d-hq/button.sh run
# read state/media_img3d_hq_ui.txt ; inject via entity_menu_history/<pid>.txt
```
