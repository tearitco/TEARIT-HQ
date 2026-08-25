# CHTMGL Wraith Assumptions

Reference:
- `projects/chtmgl-alpha/layouts/index.chtmgl`

Assumptions being tested:
- CHTMGL informs Wraith's rich UI primitive vocabulary: `panel`, `button`, `checkbox`, `slider`, `menu`, media/image placeholders, and canvas-like preview surfaces.
- Wraith should keep runtime payload naming as `${game_map}` for now, even when the source reference says `canvas`.
- map/canvas controls can live in side panels, headerbars, menus, footers, or overlays if scene records declare `target_surface=game_map`.
- normal Wraith window chrome remains separate from project-local controls.
- first media implementation should be image-first:
  - semantic `img/image` object
  - GL textured render
  - ASCII tag/state mirror
  - basic coarse ASCII image preview
- audio can follow the same project-owned pattern:
  - semantic `AUDIO:` tag in body output
  - Wraith scene controls emit `PROJECT_ACTION:AUDIO_*`
  - project-local op owns playback and PID state
  - ASCII remains the audit surface even when GL is richer
- working reference for ASCII projection:
  - `x0.parent-level-dev-env-02.01/#.x0.ref/#.img2term.c`

Current limits:
- static scene records no longer have to stay hand-authored for image proof cases:
  - `ops/src/wraith_project_input.c` now regenerates the image probe session files
- no CHTMGL parser exists yet.
- RGB presenter has first-pass widget shapes, not full CHTMGL style/layout fidelity.
- image proof now includes a true Wraith `img` object plus shared ASCII preview rows.
- audio proof is `mpg123`-backed and semantic-first; it is not yet a native Wraith waveform/timeline widget.

Next correction target:
- extend the image-first generator into a real project-owned parser/manager that reads `layouts/chtmgl-wraith.chtpm` or copied CHTMGL markup and emits `session/scene.objects.pdl`.
- keep video aligned with the same rule:
  - GL gets the richer surface
  - ASCII still gets explicit media tags/state and later frame/color back-projection
