# media-img-hq — 2D image editor (house-spec)

Split from the short-lived `media-img3d-hq` hybrid. Source:
`103.media-studio/103.img-editor/` (`HOW2_IMAGE.md`).

3D lives in `@.apps/media-3d-hq/`. Different keymap, different
framebuffer, no shared camera.

## Works now
Loads **cursword 2D** at start (`media/cursword.sprite.csv`, house
`r,g,b,a` 64²; png via ffmpeg as fallback). Layers, tools, gutter,
STROKE. `LOAD:<path>` for another png/csv. No renderer C.

## Still vs HOW2
Canvas drag-paint, PNG drop, ffmpeg export, undo.
