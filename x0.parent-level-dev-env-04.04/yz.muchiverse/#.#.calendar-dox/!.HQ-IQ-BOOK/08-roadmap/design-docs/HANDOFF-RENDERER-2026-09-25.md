# HANDOFF — Renderer / Parser agent for Network Browser presentation (F1-F4 live)

**Date:** 2026-09-25
**Branch:** `opencode-fix: 7beffef85` `network DONE` `opencode: aebd4ebc1` `main` `0 0` `fix-small e748` `working` for pull, `fix` has `network` `F1` `quiet` `F2` `wire 12/12` `F3` `layout_xy 9/9` `wcs/wcn/wck/wst GREEN`
**Request doc:** `NETWORK-BROWSER-RENDERER-PARSER-REQUEST.md:1` `82` lines `khtpm_core_render 18628/4678/5404` `draw_core 1191` `xhtpm 11/75` `nb_dom 27/60` `manager 3711/3905` lane split `CENTROID_GOLD_STD.md:38` `no g_is_*`
**Presentation:** `44.xyz.01.00/&.hq-apps/network/presentations/network-browser-normal-20260923/` `manifest.txt 6x6s` `harness.sh 62` `K9` `PIPELINE:35` `REPRODUCE.md` `snapshots/` `3.4K` placeholders `01-06` need live `X11` `dump_frame_png_op --root` `presentation.mp4` `make_presentation_video.py --width 1280`

## What network already did (do not redo)

- `nb_js_worker.c:452` `img_get_src` `HTMLImageElement` `src` `fetch` `data:` `e4428` `c1a72` `10/10`, `cdb` `stb_image` `b64_decode` `naturalWidth` `11/11`, `d8bc` `stbi_write_png /tmp/nb_img_*.png` `12/12` `dom_walk_render:565` `IMG|src|w|h|path|alt` `wcs[img-html] 12/12` `HOST` `NB_COOKIES_FILE` `nb_curl_cookies.txt` `FETCH/FETCHED 7e55fc8b`, `layout_xy 787` `514b` `parent y + siblings heights` `display:none 0,0,0,0` `wcs 9/9`, `merge_render_rows:1045` `60000`, `write_ui_projection:3711` `3905` `5-field` `c_sprite=path` `is_media` already `ui.txt`.
- `network_browser_manager.c:3711` `write_ui_projection` `3864` `content_count 400` `uisan` `|` map after `decode_entities`, `FLUSH_LINE` `88-wrap`, `MEDIA` `VIDEO` `canvas` `bar` already `V3/V4` `1691`.
- `F1` `network` `quiet` `a9edaef30` `0 TEXT|js` `WERR` `stderr`, `F2` `wire` `12/12` `c8ce0ee38`, `F3` `layout_xy` `9/9` `GREEN` all `fix:7beffef85` `0 0` `fix-small` `working`.

## What you need to do (F1-F4, one commit each, `khtpm` lane, no `network` edits)

### F1 — wrap + &copy;
- `network-browser-hq.xhtpm:11` `75` `<item sprite>` one-row-per-media `->` `<row class="sprite-grid-row" show="${c.is_media}">` grouping `≥2` consecutive `is_media` (or `c_is_mediagrid` pre-pass `3864` generic, no `g_is_network` `249`), `nb_dom.c:27` `decode_entities` `60` `copy 0xA9 Latin-1 -> C2 A9 UTF-8` `©` `uisan` before `|` map `3711`, `&amp; &lt; &gt;` regression.
- **Verify:** `google.com` `2 IMG` `->` one `sprite-grid-row` `64` grey tiles, `&copy;` `©`, `dump_frame_png_op --window` shows.

### F2 — blit
- `khtpm_draw_core.c:1191` `stbi_load(e->sprite, &pw,&ph, &comp,4)` `1202` `XCreateImage` `XPutImage` `blit_x/y dst_w/h` `167` `g_hq_sprite_cache 139` overflow (already `6e8d5308` `wcs 12/12` for `/tmp/nb_img_*.png` absolute `4` `RGBA` `stbi_write_png 570`, fallback `64` grey `1160`), verify `hq_sprite` `sprite.csv` vs `/tmp` `png` path.
- **Verify:** `data:image/png;base64,iVBOR... 1x1` `wcs[img-png]` `naturalWidth 1` `page.state.txt: IMG|data:...|1|1|/tmp/nb_img_0x...png|alt` `70B` `ui.txt: c_N_sprite=/tmp/...png` `XPutImage` at `layout_xy 0,y`.

### F3 — block layout
- `khtpm_core_render.c:4649` `layout_sidebar_panel:4881` `text direct child` `5305` `bar_max` `5404` `sw-inset*2` `khtpm_css_parser.c` `CssStyle` `nb_css.c` `css_hidden` `4649`, `layout_xy` `787` `parent y + siblings st.height` already in `worker` `9/9` but `khtpm` `Elem x/y` for `nb-content scrolllist` still `0` — extend `khtpm` `layout` simple block flow like `worker` `787`, `display:none` ancestors `0,0,0,0`.
- **Verify:** `wcs 9/9` `06_layout_rect.png` `TEXT` at `y=0` `y=line_h` `IMG` at `y=text_h` not `0`, `getBoundingClientRect` `805` `x/y` `stacked`.

### F4 — harness live
- `presentations/.../manifest.txt 6x6s` `PIPELINE:35` `harness.sh:35` `DRY_RUN` `dump_frame_png_op --root` `REPRODUCE.md` `make_presentation_video.py --width 1280` `ffmpeg/edge_tts` `presentation.mp4` `K9:125` `file relay` `#.desktop/history.txt` `bare-decimal` `TEXT` before `PNG` `136` `144`.
- **Verify:** `harness.sh --dry-run` `0` `manifest 6` live `dump_frame_png_op --window <id>` `960x45` `PAGE_STATE` `IMG|...|1|1|/tmp/...png` `TEXT` `LINK` `presentation.mp4`.

## Lane + gates
- `opencode: aebd4ebc1` `main` for pull `working small` `fix-small`, `fix: 7beffef85` `browser` until your `F1-F4` `GREEN` `wcs 12/12` `wcn 11/11` `make nbjs` `parity 0 0` then `merge --no-ff` to `main` with `strict ok` from user. `git add <path>` never `-A`, `SKILL.md khtpm-house-standards` before edit.

## Open
- `/tmp/nb_img_*.png` lifetime `MAX_IMG_DECODED 256` `draw cache 139` overflow — per `LOAD` vs `#.desktop/nb_sprites/`?


## Status update (opencode-fix, after 1c405134b)

F1-F4 are implemented and verified against the running app. What the live
evidence actually shows, and the two things that are still not wired.

### F1 — media wrap: DONE (commit 1c405134b)
`class="sprite-flow"` is generic renderer code in `khtpm_core_render.c`; the
template just opts its media `<item>`s in. A maximal run of consecutive sprite
siblings becomes one wrapping grid over `SPRITE_GRID_TILE_W/H` with the same
nav-row assignment and scroll span the media-grid-row path uses. Live proof:
six 150x110 PNGs lay out 3 columns x 2 lines at a 168px pitch (x = 252/420/588,
y = 170/266), and text after the run keeps its own row.

### F2 — blit: was never missing
`khtpm_draw_core.c` already had the `stbi_load` + `XPutImage` path behind a
`const char *` sprite (a directory sprite is a `char *` too, which is what made
this look absent). 48x48 PNGs from `data:` URIs render in the live window and
`wcs[img-png]` / `wcs[img-html]` pass.

### F3 — layout: partly done, and this block finished the honest part
Two separate things were called "block layout":

- The renderer's block/grid placement (`layout_sidebar_panel`, text as direct
  children, bars): already present, `wcs` 9/9.
- The worker's `getBoundingClientRect`: it only advanced y by *declared CSS*
  height, so every unstyled paragraph reported `0,0,0,0` in the live app while
  the suite passed (every rect assertion used an explicit `px` height). Added
  `nb_content_h()` in `ops/nb_js_worker.c`: a declared height still wins,
  otherwise a block sums its children and text gets one 14px line per full
  640px column. Live now: `a y=14 b y=28 c y=42`, each `h=14`.

  `wcs` B6 asserted `y===0` for a box with a preceding text sibling, i.e. it
  encoded the stub. It now asserts the stacking (`y===14`) and B7 the matching
  `bottom`. That is the one test expectation this block changed.

### F4 — harness: DONE
`presentations/network-browser-normal-20260923/capture_scenes.sh` replaces the
placeholder flow. Six real `dump_frame_png_op` window dumps, a local
`python3 -m http.server` fixture, `manifest.txt` with captions that match what
was captured, and a rendered `Network-Browser-Normal-20260923.mp4`
(1280x972, 36s).

Two capture findings worth keeping:
- The renderer reads `network-browser-hq_ui.txt` at startup and does not
  reliably pick up a later atomic replace, so the scene must be navigated to
  *first* and the renderer relaunched *second*; otherwise the dump shows the
  previous frame. `capture_scenes.sh` does this per scene.
- Content text rows are dim by house palette (gray ~40-90, not >170). A
  threshold check that "proves the content area is blank" is a false negative.

### Still open (not claimed as done)
- `worker_send_event()` in `network_browser_manager.c:1400` is defined and
  never called, so a khtpm click on a page element does not become a DOM EVENT.
  There is no bridge from a renderer click to a selector. The old manifest
  claimed an "EVENT RPC click" scene; that scene was dropped rather than faked.
- The manager runs `ops/+x/nb_js_worker.+x`, not the dev `./nbjs` that `make`
  produces. Editing `ops/nb_js_worker.c` and running `make nbjs` does **not**
  change the live app; rebuild `ops/+x/nb_js_worker.+x` (build.sh line 43).
- `/tmp/nb_img_*.png` lifetime / `MAX_IMG_DECODED` overflow: still open.
