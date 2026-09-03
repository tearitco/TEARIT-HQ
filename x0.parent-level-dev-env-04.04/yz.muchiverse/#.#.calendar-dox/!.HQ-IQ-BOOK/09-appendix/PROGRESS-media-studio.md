# PROGRESS — media-studio khtpm toys

**Branch:** `chtpm-var-substitution`  
**Spec:** `08-roadmap/design-docs/MEDIA-STUDIO-XHTPM-PORT.md`  
**Next agent:** start at **§9** of that file (gold-std reading list + remaining A–E).

## 2026-09-03 — kickoff

- Fresh spec: one **Media Canvas** toy (2D+3D), then DAW, then video.
  TTS ignored. Network apps wait. No layout-updates. Paths in pdl.
- Skeleton: `@.apps/media-canvas/` (`toy.pdl`, `button.sh`, xhtpm,
  `state/ui.txt`, `paths.pdl`, `keybinds.pdl`, stub `action.sh`).
- Glut originals under `103.media-studio/` untouched.
- Not launched/verified in this commit. Next: `sh @.apps/media-canvas/button.sh run`
  then HQ → toys should list Media Canvas.

## 2026-09-03 — DAW + video starters

`@.apps/media-daw/` and `@.apps/media-vid/` with `toy.pdl` (toys cell
will pick them up on next open). Layouts from HOW2: DAW file/transport/
channel/arrangement/piano/mixer; video file/transport/preview/inspector/
V1-V2-A1-A2. Stub `action.sh` only flips ui.txt. No glut.
