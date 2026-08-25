# CHTMGL Wraith

Internal Wraith validation project for markup-driven widgets.

Reference source:
- `projects/chtmgl-alpha`

This is not named `chtmgl-alpha` because it is a Wraith validation project, not the original project identity.

Initial scope:
- header/panel/button/checkbox/slider/menu placeholders
- image/media placeholders
- canvas-like preview represented using the current `${game_map}` naming standard
- receipt-backed ASCII/GL auditability

Current media direction:
- first real media work starts with `img/image`
- GL-side image handling should reuse the active `1.TPMOS` GLTPM image path where practical
- ASCII-side image handling should show:
  - a readable media tag/state row
  - a basic coarse image projection
- working ASCII projection reference:
  - `x0.parent-level-dev-env-02.01/#.x0.ref/#.img2term.c`
- current implemented phase:
  - shared media asset: `x0.parent-level-dev-env-02.01/#.media-library/missingno.png`
  - project-owned generator op:
    - `ops/src/wraith_project_input.c`
  - generated outputs:
    - `session/state.txt`
    - `session/wraith_body.txt`
    - `session/scene.objects.pdl`
  - current visual truth:
    - shared image decode drives a coarse 24x12 ASCII preview
    - same preview rows are emitted into scene text objects so GL and ASCII both see the same sampled result
  - current audio truth:
    - project-local audio op:
      - `ops/src/wraith_audio.c`
    - runtime control path:
      - scene controls emit `PROJECT_ACTION:AUDIO_*`
      - `ops/src/wraith_project_input.c` reads `session/history.txt`
      - local `mpg123` playback is driven from the project root and keeps PID state in `session/audio.pid`
      - default audio source is `x0.parent-level-dev-env-02.01/#.media-library/gowon.mp3`
    - ASCII-side audio handling shows:
      - semantic `AUDIO:` tag
      - current state mirror: `playing`, `paused`, `stopped`, `missing`
  - not done yet:
    - ANSI/colorized ASCII projection
    - video tags

Audit rule:
- media features should degrade into readable ASCII state instead of becoming GL-only truth
- current proof keeps image and audio state project-owned so future image/video back-projection can reuse the same session outputs

See `ASSUMPTIONS.md` for the current design bets that need user correction.
