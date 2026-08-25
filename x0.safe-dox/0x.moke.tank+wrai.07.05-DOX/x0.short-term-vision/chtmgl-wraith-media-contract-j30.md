# CHTMGL Wraith Media Contract J30

Date: 2026-06-30
Status: Working contract with first active proof now landed.

==================================================
1. PURPOSE
==================================================

This doc narrows the next `chtmgl-wraith` push:

- image
- audio
- video

The goal is not to clone the old CHTML/CHTMGL prototype exactly.

The goal is to:

1. reuse the proven ideas from the older prototype
2. express them through current Wraith project/session/receipt seams
3. keep ASCII auditability intact while GL grows richer

==================================================
2. REFERENCE PRIOR ART
==================================================

Reference-only older prototype:

- `x0.parent-level-dev-env-02.01/#.CHTMGL.E23=cordsclean]💯️`

Concrete older files worth studying:

Image:
- `chtml/test_image2.chtml`
- `chtml/master-demo]c3.txt`
- `3.view_v0.05]textiles.c`
- `stb_image.h`
- `x0.parent-level-dev-env-02.01/#.x0.ref/#.img2term.c`

Audio:
- `5.audio_v0.01.c`
- `test_audio.chtml`

Video:
- `6.video_controller_v0.0.c`
- `test_video.chtml.txt`
- `task/0.mp4.playerffmpg]PURE!.c`

Important judgment:

- image support in the older lane looks closest to reusable
- audio/video support exists, but is more prototype-shaped and less ready to copy directly
- `#.img2term.c` should be treated as the working reference for first ASCII image projection, even before color-rich terminal output is added

So the right move is copy-mod ideas, not direct transplant.

==================================================
3. FIRST WRAITH SCOPE
==================================================

All first-pass media work should land in:

- `projects/wraith-alpha/wraith-projects/chtmgl-wraith`

Video isolate proof companion:

- `projects/wraith-alpha/wraith-projects/chtmgl-video-isolate`

Short-term tag set:

1. `<img ... />` / `<image ... />`
2. `<audio ... />`
3. `<video ... />`

Short-term objective:

- parse them
- represent them semantically
- render them meaningfully in GL
- expose readable/stateful mirror truth in ASCII

Current proof status:

- image: proven
- audio: proven
- video: proven through project-owned frame swap and marker-triggered rerender

==================================================
4. MINIMAL TAG CONTRACT
==================================================

## Image

Required first-pass fields:

- `src`
- `x`
- `y`
- `width`
- `height`
- optional `id`
- optional `alt`

GL first-pass behavior:

- load image asset
- upload texture
- draw it in the declared rect
- fall back to placeholder if load fails

ASCII first-pass behavior:

- render at least a readable tag/state row, for example:
  - `IMG:id=hero src=assets/hero.png state=loaded 320x180`
  - `IMG:id=hero src=assets/hero.png state=missing`

ASCII image proof behavior:

- in addition to the tag/state row, the first Wraith image pass should aim to emit a basic coarse image projection in ASCII
- working reference:
  - `x0.parent-level-dev-env-02.01/#.x0.ref/#.img2term.c`
- first acceptable form:
  - monochrome / brightness-mapped preview
  - low-resolution cell approximation
- later extension:
  - ANSI/palette color projection using the same decoded RGB source

## Audio

Required first-pass fields:

- `src`
- optional `id`
- optional `autoplay`
- optional `loop`
- optional `controls`

GL/runtime first-pass behavior:

- treat audio primarily as playback state, not a visual rectangle
- allow project state to express:
  - loaded
  - playing
  - paused
  - stopped
  - ended
  - missing/error

ASCII first-pass behavior:

- render readable state, for example:
  - `AUDIO:id=bgm src=audio/theme.mp3 state=playing loop=1`

## Video

Required first-pass fields:

- `src`
- `x`
- `y`
- `width`
- `height`
- optional `id`
- optional `autoplay`
- optional `loop`
- optional `controls`
- optional `poster`

GL first-pass behavior:

- decode frames
- upload/update texture
- render current frame in rect
- preserve playback state
- fall back to placeholder/poster if frame decode is unavailable

ASCII first-pass behavior:

- render readable tag/state row, for example:
  - `VIDEO:id=intro src=video/intro.mp4 state=playing frame=128`
  - `VIDEO:id=intro src=video/intro.mp4 state=paused poster=assets/poster.png`

==================================================
5. ASCII RULE NOW
==================================================

At minimum, media must show up in ASCII as a tag/state object.

That is the required first pass.

Examples:

- `IMG:...`
- `AUDIO:...`
- `VIDEO:...`

This keeps:

- auditability
- agent visibility
- no-screenshot debugging
- stable semantic truth

even before richer ASCII projection exists.

==================================================
6. ASCII RULE LATER
==================================================

ASCII should not be locked forever to token-only media.

Future architecture should assume:

- images will already be decoded into RGB data for GL upload
- video frames will already be decoded into RGB data for GL updates
- basic ASCII image projection can use the same decode path immediately

So the same decoded RGB truth can later be reprojected back into the ASCII/CHTPM renderer as:

- ANSI / palette-mapped color blocks
- ASCII brightness ramps
- coarse pixel-cell previews
- frame-stream approximations for video

That means:

- media token rows are the minimum truth
- RGB-to-ASCII media projection is the later richer audit surface
- both should still originate from shared decoded/media state rather than ad hoc duplicate paths

==================================================
6.5 PERFORMANCE NOTE
==================================================

Current webcam/video slowness is most likely dominated by file churn:

- ffmpeg writes image frames to disk
- project ops reread those files
- Wraith then reprojects them into ASCII and/or GL scene output

That is acceptable for proof-of-contract work, but it is not the long-term fast path.

Preferred future acceleration order:

1. keep the current file-backed seam as the debug-safe fallback
2. add decoded-frame caching inside the project-local runtime
3. move hot frame transport to shared memory or a ring-buffer seam
4. let both GL upload and ASCII projection consume the same in-memory decoded frame
5. keep marker/receipt files for control truth and audit, not as the primary pixel transport

In other words:

- files remain the sovereignty/audit seam
- shared memory becomes the performance seam

That is likely the right shape later for:

- webcam
- screen record preview
- richer video playback
- future screen-share / AR / live-canvas projects

==================================================
7. ARCHITECTURE RULE
==================================================

Do not make media support a GL-only black box.

Preferred shape:

1. media tag parsed
2. semantic media object created
3. decoded/runtime state recorded
4. GL consumes decoded state for rich rendering
5. ASCII consumes semantic state now
6. ASCII may later consume reduced RGB projection too

Additional proven rule from the active lane:

7. dynamic media surfaces that advance over time must publish a redraw seam
8. in current Wraith that seam is project-owned marker output to `session/fs_watch.marker`

This is the cleanest way to avoid divergence.

==================================================
8. IMPLEMENTATION ORDER
==================================================

Recommended order:

1. image
   - simplest proof
   - strongest older reference
   - has both GL and ASCII reference material already:
     - current `gltpm_parser.c` / `gl_desktop.c` image path
     - `#.img2term.c` for text projection
2. audio
   - semantic/state-heavy, less visual
3. video
   - most expensive and dependency-heavy

==================================================
9. WHAT TO REUSE VS WHAT TO REWRITE
==================================================

Reuse candidates:

- image decoding/loading patterns from the old prototype
- media tag vocabulary ideas
- fallback placeholder behavior

Rewrite/adapt candidates:

- session/state ownership
- receipt/object emission
- Wraith project op integration
- ASCII mirror generation
- any raw prototype controller assumptions

==================================================
10. DELIVERABLE
==================================================

The next real `chtmgl-wraith` milestone should prove:

1. `img/image` tag in GL plus ASCII tag/state
2. `audio` tag state surface in ASCII and runtime control path
3. `video` tag rectangle in GL plus ASCII tag/state
4. explicit note of what is semantic truth vs what is only rendered decoration
