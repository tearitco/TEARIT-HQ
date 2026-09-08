# media-img-hq — 2D image editor (house-spec)

Split from the short-lived `media-img3d-hq` hybrid. Source:
`103.media-studio/103.img-editor/` (`HOW2_IMAGE.md`).

3D lives in `@.apps/media-3d-hq/`. Different keymap, different
framebuffer, no shared camera.

## Works now
Layers 1–6, vis, B/E/G/R/I/H, brush +/−, fg/bg, pan/zoom state,
STROKE at canvas center, `canvas.raw` blit. No renderer C.

## Still vs HOW2
Canvas drag-paint, PNG drop, ffmpeg export, undo.
