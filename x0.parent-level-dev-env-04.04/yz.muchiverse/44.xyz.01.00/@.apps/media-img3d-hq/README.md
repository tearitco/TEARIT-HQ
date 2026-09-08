# media-img3d-hq — SUPERSEDED 2026-09-08

The combined 2D+3D toy was a first pass. **2D and 3D are separate
apps** (different HOW2s, keymaps, and canvases):

- `@.apps/media-img-hq/` — image editor
- `@.apps/media-3d-hq/` — 3D viewport

`toy.pdl` is removed so HQ toys does not list this hybrid. Source
glut dirs still point at the split toys via `CONVERTED.txt`.
