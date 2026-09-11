# Network browser: cli-io editing restore + in-page images + video reuse — DESIGN

Status: **design, pending build** (2026-09-10). Scope of this doc:
(a) restore one-shot cli-io editing in the address bar (typing /
Backspace regression), (b) route `<img>` through the house's existing
sprite/canvas image abilities, (c) reuse the wraith-alpha video player
for `<video>` if feasible, (d) state the real HTML-parser position.

All four were researched live on 2026-09-10 before writing. File/line
anchors are to the live tree at time of writing.

---

## 0. TL;DR

1. **Backspace is NOT gone from the renderer** — it works the moment the
   address `<cli_io>` is *armed*. What regressed is **arm-on-click**: the
   house-wide two-step click rule now makes the address bar require TWO
   clicks (1st = focus, 2nd = arm), and any template/vars reparse disarms
   the field mid-edit. Live probe: 1 click → letters AND BackSpace dead;
   2 clicks → `abc`, 2×BackSpace → `ab`. Fix = cli_io always arms on
   first click + armed field survives reparse (re-arm by target_id).
2. **Images**: the house already renders images two ways — generic
   `sprite=` thumbnail blits (≤64px tiles; browser already emits them)
   and `<canvas sprite=".raw">` native framebuffers (media-img-hq
   proven). The browser's own `nb_media_to_sprite` (stb_image) already
   decodes PNG/JPEG/GIF/BMP. **One functional bug** blocks page images
   on JS pages: the worker RENDER emits `IMG|<url>` but the sprite pass
   only consumes `MEDIA|` rows → broken tiles.
3. **Video**: wraith-alpha plays via ffmpeg pre-extracted PNG frames +
   a `current_frame.png` file-swap + text control/playback files. It is a
   standalone binary but has a clean subprocess contract — feasible to
   drive from the browser and draw the frame PNG with the existing
   stb_image→canvas path. Caveats: 8fps, 320x240, PNG-copy bottleneck.
4. **HTML parser**: only two small hand-rolled scanners exist (`nb_dom.c`
   tiny tree + the manager's linear extractor). Verdict: do NOT port an
   external HTML5 parser now — formally specify the supported subset the
   browser actually renders and make media path through it.

---

## 1. (a) Cli-io editing — root cause + fix

### Evidence (live, 2026-09-10)

- Editing + BackSpace lives in `default_cli_io_handle_key()`:
  `*.monads/*.livedesk-taskbar/ops/khtpm_core_render.c` (XK_BackSpace at
  ~6499; text-area variant ~6602/6612; run-action ~6375).
- It is reached ONLY when the address bar is armed:
  `if (g_default_input_elem) { default_cli_io_handle_key(ks, ch); return; }`
  (~7857). Arm happens in `activate_focused()` (~6667) on a focused
  cli_io, seeded from the end of its buffer.
- Click arm route: `popup_handle_click()` + `click_focus_then_activate()`
  (~2159) — under house `click_two_step` the FIRST click only focuses
  (returns 0), the SECOND activates (returns 1 → arm). The 2026-09-10
  dropdown fix removed the only "click = immediate fire" exception, so
  the address bar became two-click-to-type.
- The generic default-mode ButtonPress path (~9108) only re-asserts X
  input focus on click (`XSetInputFocus`) — it does NOT hit-test or arm,
  so no one-click path exists for this window at all.
- Reparse semantics: any template mtime change OR vars-hash change
  re-parses, and reparse clears the arm (`g_default_input_elem = NULL`,
  ~1738-1758, the 2026-08-31 dangling-pointer fix). The browser manager
  re-writes its projection every tick and on every page/console event,
  so editing through a live page update drops the arm.
- Live probes (xdotool window-rel coords, address bar x=200 y=72 w=506):
  click once, type abc → `#.desktop/../cli_io_state.txt` stayed
  `address=` (nothing typed). Click twice, type abc → `address=abc`;
  BackSpace ×2 → `address=ab` (BACKSPACE WORKS once armed).

### Decisions

- D1. **cli_io always arms on first click** in `click_focus_then_activate`:
  a hit element whose tag is `cli_io`/`text_area`/`grid` returns `1`
  (activate → arm) regardless of `click_two_step`. Arming is not an
  action; it only takes keyboard ownership, so it must never be gated by
  the two-step rule. Keep two-step for real action items.
- D2. **Armed field survives reparse**: capture `{target_id/id, buffer,
  cursor}` before the tree rebuild (~1758), re-locate the element after
  `parse_chtpm`, re-arm + restore cursor (mirror the existing
  `text_area_<id>.txt` save/restore pattern). Then live projection
  rewrites (page text, console rows) can no longer kill an edit in
  flight.
- D3. Optional UX: while armed, the address bar draws its own
  `input_buffer` instead of the URL var label, so what you're typing is
  visible before Enter (currently only `cli_io_state.txt` + the ^ cursor
  prove it).

### Verify after build

- A1: one click → type `abc` → state `address=abc`; BackSpace → `ab`.
- A2: type one char, force a console change (`eval:1`) via the request
  file mid-edit, keep typing → arm must persist, buffer intact.
- A3: Enter after edit dispatches `go:<typed>` (manager `nb_write_go.sh`).

---

## 2. (b) In-page images — reuse the existing sprite/canvas path

### What already exists (verified)

- Generic `sprite=` attribute on any item/row: `hq_sprite()` →
  `hq_blit_sprite()` in `&.widgits/_shared-lib/khtpm_draw_core.c`
  (LRU-cached sprite.csv loader, ≤64px, alpha-blended XPutImage). Zero
  per-app renderer code.
- `<canvas sprite="<dir>.raw">`: `kh_draw_canvas()` (khtpm_draw_core.c
  ~533) — full native-size RGBA framebuffer + receipt. Proven by
  `@.apps/media-img-hq` (stb_image or ffmpeg → `state/canvas.raw` +
  `receipt.txt`).
- Browser side: `&.hq-apps/network/ops/nb_media_to_sprite.c` already
  decodes PNG/JPEG/GIF/BMP/TGA/PSD (stb_image v2.30) → 64×64 sprite.csv.
  The manager already sniffs image bytes (`looks_image_bytes` ~1909),
  fetches (`collect_page_media` ~1075), emits `IMG|<sprite_dir>|<alt>`
  / `VIDEO|…` rows, and wraps them in sprite-grid `<item sprite=…>` rows
  in the projection (~2498-2611). Static (no-JS) pages show these tiles
  TODAY.

### The one functional gap (blocking page images on JS pages)

- Worker RENDER path emits `IMG|<raw_src_url>|<alt>` (`nb_js_worker.c`
  ~431-440), then `merge_render_rows` (manager ~965-992) replaces the
  static rows with these — and the sprite pass only consumes `MEDIA|`
  rows, so on any page that runs JS the `<img>` rows carry a raw URL
  into the `sprite=` attribute and render as a broken/blank tile.

### Decisions / build order for images

- D4. Fix the worker IMG-row mismatch: route worker `IMG|<url>` rows
  through the same fetch→sprite pass (treat them as MEDIA), or defer the
  worker-row merge until after `collect_page_media`. Smallest safe change
  first.
- D5. Surface placeholders instead of silent drops: on sprite-fetch
  failure, emit a neutral 1×1 sprite + alt caption (currently the row is
  dropped at ~1117; `media_skip_url` ~1019 also nukes pixel/analytics).
- D6. Phase-2 full-size images: a `<canvas id="media-view"
  sprite="${canvas_raw}">` driven by `LOAD:<url>` → stb_image → raw +
  receipt (media-img-hq recipe). Enables "open image full-size";
  no renderer C changes required.
- D7. Sizing: keep 64px tiles for thumbnails; make SPR_RES an
  attribute-driven value (sprite-grid cells are 168×96) before raising.

### Verify

- B1: a JS page with `<img>` renders a real tile (not blank); alt shows
  on failure.
- B2: `go:https://…/photo.jpg` → direct-image canvas path shows full
  image.

---

## 3. (c) Video — reuse the wraith-alpha player

### How wraith-alpha plays video (researched)

`…/x0.moke-pet-project-04.04/x0.5-liz.fiter4-mew-00.03/projects/wraith-alpha/wraith-projects/chtmgl-video-isolate/`
(plus identical copies in web-cam/ and screen-record/):

- `wraith_video_player.c`: ffmpeg pre-extract `-vf fps=8,scale=320:240`
  → `session/video_frames/frame_%04d.png`, then a forked loop `cp`s each
  PNG to `session/current_frame.png` at 8fps.
- `wraith_project_input.c`: ffprobe metadata, ffmpeg poster.png,
  audio extract (mpg123) — all CLI subprocesses.
- Contract (all files, easy to talk to): `session/video.control`
  ("play"/"pause"/"stop"), `session/video.playback` (`state=`,
  `frame_index=`, `frame_total=`), `session/video.pid`, `current_frame.png`.
- Rendered in wraith via the SAME shared khtpm renderer: `OBJECT
  tag=img source_ref=session/current_frame.png` → stb_image →
  sprite.csv → blit. So the house renderer already knows how to show it.

### Feasibility verdict: YES as a subprocess plug-in

The browser's `<video>` today opens external `ffplay` (`write_chtpm_projection`
~2600-2604). Phase plan:

- V1 (keep, already works): `<video>` rows keep launching `ffplay` for
  real playback (external, audio included).
- V2 (p.o.c., borrow wraith): run `wraith_video_player <video>
  <session_dir>` as the browser's child; the page canvas shows
  `current_frame.png` via the existing PNG→canvas path (controller:
  `eval:` / row action writes `video.control`; frame advance observed on
  `video.playback`). Use only for short clips — the PNG-copy/8fps/320x240
  model is the honest ceiling (per original wraith design goals).
- V3 (later, only if needed): rawvideo pipe from ffmpeg
  (`-f rawvideo -pix_fmt rgba pipe:1`) to a shared canvas. Out of scope
  now.

### Verify

- C1: a short local mp4 plays inside the browser canvas at ≥8fps with a
  watching `frame_index` in `video.playback`; controls via
  `video.control`.

---

## 4. (d) HTML parser — real position + the plan

### Position (researched, no external parser anywhere in repo)

- `&.hq-apps/network/nb_dom.c`: a real tiny tree (not an HTML5
  tokenizer). `<head>`/`<script>`/`<style>`/`<title>`/`<noscript>`
  skipped; void tags incl. `img source video track embed` kept as nodes;
  anything else → generic node with id/class + raw attr blob; ~15
  entities + numeric <0x100. Intentionally "NOT a full HTML5 parser"
  (nb_dom.h ~17-22).
- Manager `network_browser_manager.c` `extract_and_publish` (~429-683):
  linear scanner (no regex, no libxml), emits URL/TITLE/TEXT/LINK rows +
  `MEDIA|I` (img) and `MEDIA|V` (video/poster/<source>).
- Worker DOM RENDER (`nb_js_worker.c` ~409-452): title/a/img branches,
  NO video branch. `fetch()`/XHR `blob()`/`arrayBuffer()` are stubs →
  JS can GET text but not binary media.

### Decision

- D8. **Specify, don't port.** Formalize the "browser-renderable
  subset" (a documented contract inside nb_dom.h + a parser-test corpus):
  text flow + headings/lists/paragraph semantics, `a`, `img`
  (`src`/`alt`), `video`+`source`+`poster`, title; script bodies run via
  the worker; collected CSS stays `nb_css`-scoped; NO forms/layout
  engines beyond the current content flow. External tokenizers
  (gumbo/html5ever/libxml) are out — not needed for the house's scope.
- D9. Close the enumerated holes (in order): worker `<video>` RENDER
  branch; worker IMG-row → sprite pass (D4); entity-set expansion;
  `<picture>`/`srcset` selection when a real need appears; incremental
  re-render after async media fetch (needs a 2nd RENDER/merge cycle).

### Verify

- D: parser-corpus tests run green (text/a/img/video rows all emit for
  fixture pages, JS and no-JS).

---

## 5. Build order & evidence protocol

1. **A first** (small, unblocks the console REPL UX): D1+D2 (+D3), then
   A1-A3 probes.
2. **B**: D4 city `eval:`-style repro on a JS page with an `<img>`; then
   D5 placeholders; optional D6 full-size canvas.
3. **C**: V2 p.o.c. using one short mp4 + local fixture page; C1 probe.
4. **D**: subset spec (nb_dom.h header) + corpus tests; D9 holes.
5. Every step: fresh `make` build + live run + receipt/state evidence +
   (for anything visible) a PRESENTATION-VIDEO-PIPELINE recording.
   No step is "done" on compile alone.

## 6. References

- `*.monads/*.livedesk-taskbar/ops/khtpm_core_render.c` — cli_io arm
  (~6667), BackSpace (~6499/6602/6612), reparse disarm (~1738-1758),
  two-step gate (~2159), generic click path (~9108).
- `*.monads/*.livedesk-taskbar/ops/khtpm_draw_core.c` — hq_sprite (~145),
  hq_blit_sprite (~219), kh_draw_canvas (~533).
- `&.hq-apps/network/` — nb_dom.c/h, nb_js_worker.c, network_browser_manager.c,
  ops/nb_media_to_sprite.c, ops/+x/nb_media_to_sprite.+x.
- `&.widgits/_shared-lib/system/chtpm_parser.c` — the CHTPM UI parser
  (NOT HTML; excluded).
- `02-architecture/xperiments/khtpm-generic-dispatch-design.md` — cli_io
  arm-on-Enter/click semantics; reparse disarm rationale.
- `08-roadmap/design-docs/CLI_IO-CURSOR-AND-TEXT_AREA-MULTILINE-EDITING-DESIGN.md`.
- wraith-alpha: `…/x0.moke-pet-project-04.04/x0.5-liz.fiter4-mew-00.03/projects/wraith-alpha/wraith-projects/chtmgl-video-isolate/` (+ web-cam/, screen-record/).
- `10-user-docs/PRESENTATION-VIDEO-PIPELINE.md` — proof-video recorder
  used for evidence on steps A-D.