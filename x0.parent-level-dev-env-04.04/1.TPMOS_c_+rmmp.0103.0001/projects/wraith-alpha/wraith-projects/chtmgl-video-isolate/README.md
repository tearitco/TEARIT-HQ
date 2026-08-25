# CHTMGL Video Isolate

Isolated Wraith project for video-tag experiments.

Purpose:
- keep video decode/playback complexity out of `chtmgl-wraith`
- prove `<video>` semantics, runtime state, poster generation, and GL preview in a narrower lane
- preserve a simpler media baseline in `chtmgl-wraith`

Current scope:
- `video` tag semantic contract
- `ffprobe` metadata capture
- `ffmpeg` poster extraction into `session/poster.png`
- playback state rows in ASCII
- in-Wraith frame-swap transport controlled by project-local ops
- shared current-frame preview between ASCII and GL

Runtime pieces:
- `ops/src/wraith_project_input.c`
- `ops/src/wraith_video_player.c`
- sample asset:
  - `x0.parent-level-dev-env-02.01/#.media-library/sample-10s-vp9.mp4`

Audit rule:
- Wraith GL can show the poster image and richer controls
- ASCII must still show the explicit `VIDEO:` tag, transport state, dimensions, duration, and a coarse poster preview
