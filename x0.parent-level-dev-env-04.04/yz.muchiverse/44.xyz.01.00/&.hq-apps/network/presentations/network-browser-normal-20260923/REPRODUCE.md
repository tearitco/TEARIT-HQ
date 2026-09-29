# REPRODUCE — network-browser-normal presentation

**Branch:** `opencode-fix` (renderer/worker work; never merged into `opencode`
without the user's explicit ok).
**Suites:** `wcs wcn wck wst wps wdt wet wft` all PASS.

## Redo the proof live

The capture is scripted, so it is one command:

```sh
# 1. fixture pages over real HTTP
python3 -m http.server 8126 --bind 127.0.0.1 --directory /tmp/nb_site &

# 2. the six real window dumps
sh presentations/network-browser-normal-20260923/capture_scenes.sh <house_root>

# 3. the video
python3 <livedesk>/pals/cursword/presentations/make_presentation_video.py \
  presentations/network-browser-normal-20260923 --width 1280
```

Fixture pages live in `/tmp/nb_site`: `index.html` (text, links, six PNGs),
`page2.html`, `api.json`, `pic0..5.png`, plus `scene2_fetch.html`,
`scene3_image.html`, `scene6_rect.html`.

If you drive the browser by hand instead of with `capture_scenes.sh`:

1. `make -C 44.xyz.01.00/&.hq-apps/network nbjs wcs` must be GREEN.
2. `sh 44.xyz.01.00/&.hq-apps/network/button.sh <house_root>`, wait for the
   `960x640` window (`xwininfo -root -children | grep 960x640`).
3. Navigate **first**, then relaunch the renderer **second**, then capture. The
   renderer reads `network-browser-hq_ui.txt` at startup and does not reliably
   pick up a later atomic replace of that file, so navigating after launch and
   capturing immediately yields the *previous* frame. `capture_scenes.sh` does
   this per scene.
4. Capture with `dump_frame_png_op <window_id_hex> <out.png>` (PIPELINE:35,
   never scrot).

Rebuilding the live worker: the manager runs `ops/+x/nb_js_worker.+x`, **not**
the dev `./nbjs` that `make nbjs` writes. Use `build.sh` (line 43) or the exact
`gcc` line it prints, otherwise an edit to `ops/nb_js_worker.c` will not reach
the running app.

## What each snapshot proves

- **01** — a real page over HTTP: `RENDER` rows (`TITLE|`, `TEXT|`, `LINK|`,
  `IMG|`) land in `network-browser-hq_ui.txt` and are drawn by khtpm.
- **02** — `fetch()` from an inline script through the manager RPC, with the
  response written back into the DOM (`c_0_text=fetch ok: {`).
- **03** — three 48x48 PNGs inlined as `data:image/png;base64`, decoded by
  stb_image and blitted by `khtpm_draw_core`; the rendered tiles are visible
  in the window.
- **04** — a second document in the same session, with the history strip and
  address bar updated by the manager.
- **05** — the F1 grid: six 150x110 PNGs as one `class="sprite-flow"` run,
  3 columns x 2 lines at a 168px pitch (x = 252/420/588, y = 170/266).
- **06** — the F3 fix: `getBoundingClientRect` inside the page reports real
  stacked boxes for *unstyled* paragraphs, `a y=14 b y=28 c y=42`, each
  `h=14`, from `nb_content_h()` in `ops/nb_js_worker.c`.

## Reading these screenshots

Content text rows are dim by house palette (gray ~40-90). A "is the content
area blank?" check with a brightness threshold above ~100 is a false negative;
scan at a low threshold or read `network-browser-hq_ui.txt`.

## Video

`Network-Browser-Normal-20260923.mp4` — 1280x972, 6 frames, 36s. It rendered
silent because `edge_tts`/`pydub` are not importable in this environment; the
captions are burned onto the frames regardless, so the video never overclaims.

## Status

Six real captures are in `snapshots/` and the mp4 exists. Not claimed as done:
the click path (`worker_send_event` is defined and never called, so a khtpm
click on a page element does not reach a DOM EVENT) — the old manifest's EVENT
RPC scene was dropped rather than faked. See the F-section status update in
`HANDOFF-RENDERER-2026-09-25.md`.
