# NETWORK BROWSER — RENDERER / PARSER REQUEST (what the network cell needs from you)

**Date:** 2026-09-25
**Branch:** `opencode` (main) `c8ce0ee38` `0 0` `opencode-fix: e748edbb8` safe `5e9fbade9` fix-small
**Requesting cell:** `44.xyz.01.00/&.hq-apps/network/` (network browser) — `opencode` agent stays off parser/renderer per your call, needs your lane
**Your lane:** `*.monads/*.livedesk-taskbar/ops/khtpm_core_render.c` `&.widgits/_shared-lib/khtpm_draw_core.c` `&.hq-apps/network/nb_dom.c` `nb_css.c` `network-browser-hq.xhtpm` `khtpm_css_parser.c` `CENTROID_GOLD_STD.md:38`
**Our lane (done, do not touch):** `ops/nb_js_worker.c` `network_browser_manager.c` `js/quickjs/*` `tests/worker_*` `presentations/network-browser-normal-20260923/`

## 0. Lane split (why this doc exists)

`opencode` owns `network` per `CENTROID_GOLD_STD.md:38` one `Elem x/y/w/h+CssStyle` tree `khtpm_hq_manager` shape. We shipped `network` wire end-to-end (QuickJS `07aa2200`, `RER` `bba458ec`, `RENDER` `60000` `merge_render_rows:1045`, `EVENT 43c72099`, `FETCH 7e55fc8b`, `Rung7` `e4428` `cdb` `d8bc` `514b` `wcs 12/12` `wcn 11/11` `wck 3/3` `wst 2/2`) but the GUI still flat because the frontend path `page.state.txt -> ui.txt -> .xhtpm -> Elem -> X11` was first-cut `PROGRESS-network-browser-xhtpm.md:1`. You own `Elem` layout + `draw` + `HTML parse`. This doc is the exact, line-numbered handoff so you can land `F1-F4` without touching `network`.

**House law:** `CENTROID_GOLD_STD.md:249` no `g_is_network` branch in shared renderer — use generic dispatch `g_khtpm_modes[]` or existing `reusable_slot`/`elem_inject_loop`. `SKILL.md khtpm-house-standards` before edit. `make nbjs` `wcs 12/12` `parity 0 0` before push, `git add <path>` never `-A`.

## 1. What we already wire (so you know what to consume)

### 1.1 page.state.txt rows (manager authoritative)
`network_browser_manager.c:3711` `write_ui_projection()` `3864` reads `#.desktop/network_browser_page.state.txt`:
```
URL|https://example.com
TITLE|Example
TEXT|visible paragraph (88-wrap, document order)
LINK|https://example.com/more|More
IMG|https://example.com/a.png|1|1|/tmp/nb_img_0xABC.png|alt text   (5-field, d8bc3378)
MEDIA|I|...  MEDIA|V|...
VIDEO|... (V3 canvas surface.raw)
```
`nb_js_worker.c:519` `dom_walk_render()` `543` `img_get_src()` `565` `stbi_write_png(imgpath, dw,dh, rgba)` `571` `IMG|src|w|h|path|alt` `60000` cap `sb_put` `| -> space` `MER` `worker -> manager: RENDER\n<rows>` `manager: merge_render_rows:1045` overlay.

### 1.2 ui.txt projection (what your template sees)
`write_ui_projection:3728` `UI_PUT` `tmp+rename` `g_ui_output_path = #.desktop/network-browser-hq_ui.txt` `3893` `content_count` `3864` `NB_UI_ROWS_MAX 400`:
```
act_back='.../nb_write_back.sh' 'back'
addr_label=URL: https://example.com   (uisan |->/ CR/LF strip)
status=Status: ready
n_bm, bm_0_label, bm_0_action, n_hist, h_0_label/action/del_action, n_tabs, t_0_label/action
content_count=6, c_0_kind=title c_0_is_title=1 c_0_text=Example, c_1_is_text=1 c_1_text=..., c_2_is_link=1 c_2_text/link/action, c_3_is_media=1 c_3_sprite=/tmp/nb_img_*.png c_3_label=alt
```
`IMG` parse `3905` `q1..q5` `5` `|` `src|w|h|path|alt` `s1=path` `lab_s=alt` `3928` `c_N_is_media=1 c_N_sprite=path c_N_label=alt` `3934` `LINK` tail `action`. Current `xhtpm` `75` `<item sprite="${c.sprite}" show="${c.is_media}">` one row per media (no grid).

### 1.3 Current window sizing (why fix-small matters)
`khtpm_core_render.c:18628` `if(g_user_resizable){ sw=DisplayWidth; g_win_x=90; g_win_y=WM_MANAGED_DRAG_MIN_Y; g_win_w=kh_default_win_w(); }` `3814` `kh_default_win_w(){ g_default_win_w>0?g_default_win_w:kh_screen_w()*WM_DEFAULT_PCT_W/100 }` `57ab727e` `hq_ui.pdl:360x280` now `pdl-test` ` fix-small e748` `700` `DEFAULT` `4299` `kh_default`. `fix: e748edbb8` `DEFAULT 700x520` small via `ui_scale 0.54` `149` `ps` on `1360x768` is safe.

## 2. What we need from you (F1-F4, each shippable, one commit)

### F1 — sprite-grid-row wrap + &copy; (no network code)
**Gap:** `network-browser-hq.xhtpm:11` wrap `NOT reproduced, each media own row` + `PROGRESS-network-browser-xhtpm.md:100` `&copy;` literal.
- **File:** `network-browser-hq.xhtpm:70` `repeat bind="c"` + `khtpm_core_render.c:4649` `layout_sidebar_panel:4881` + `nb_dom.c:27` `decode_entities()`.
- **Task:** `write_ui_projection` already has `IMG` rows `3905`, `network-browser-hq.xhtpm` add `<row class="sprite-grid-row" show="${c.is_media}">` grouping `≥2` consecutive `is_media` into one row (or `c_is_mediagrid` `grid_count` pre-pass `3864` — your call, keep generic, no `g_is_network` `249`). `nb_dom.c:60` `copy 0xA9` single-byte `->` UTF-8 `C2 A9` `©` (currently `0xA9` `Latin-1` shows tofu), ensure `uisan()` `3711` does `|` map *after* `decode_entities` so `©` survives, add `&amp; &lt; &gt; &quot; &apos; &nbsp;` regression.
- **Verify:** `google.com` `2` `IMG` `→` one `sprite-grid-row` `3.4K` placeholder `→` real `64x64` grey tiles, `&copy;` `©` not `&copy;` or `©` mojibake, `dump_frame_png_op --window <id>` shows.

### F2 — image sprite blit (consume our wire, no network fetch)
**Gap:** `page.state.txt` already `IMG|...|/tmp/nb_img_*.png` `d8bc` but `X11` shows text `alt` only.
- **File:** `&.widgits/_shared-lib/khtpm_draw_core.c:1191` `stbi_load(e->sprite, &pw,&ph, &comp,4)` `1202` `XCreateImage` `XPutImage` `blit_x/y dst_w/h` `draw_sprite` `167` `g_hq_sprite_cache` `139` overflow comment + `khtpm_core_render.c:829` `sprite=` path comment.
- **Task:** `khtpm_draw_core.c:1191` already does `if(sprite ends .png) stbi_load+XPutImage at e->x,y clip` `6e8d5308` `wcs[img-html]`. Verify it handles our `/tmp/nb_img_*.png` `dw|dh` `w/h` `path` absolute `4` `RGBA` `stbi_write_png` `570`, fallback `hq_sprite` `sprite.csv` `64` grey tile `1160` when fetch fails, cache evict `139`. No new `g_is_*`.
- **Verify:** `file://` `data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mP8z8DwHwAFBQIAX8jx0gAAAABJRU5ErkJggg==` `1x1` `wcs[img-png]` `wcs[img-html]` `12/12` `page.state.txt: IMG|data:...|1|1|/tmp/nb_img_0x...png|alt` `70B` `ui.txt: c_N_sprite=/tmp/...png` `XPutImage` at `layout_xy` `0, y` visible.

### F3 — minimal block layout for real positions (F2 needs it)
**Gap:** `ROADMAP.md:447` `slice1` cascade `w/h` `0` unless CSS `px`, `getBoundingClientRect` `0,0` `nb_js_worker.c:787` `layout_xy()` already `514b8ab9` `9/9` in worker, but `khtpm` `Elem x/y` for `network-browser-hq.xhtpm` `nb-content` `scrolllist` is still `0`.
- **File:** `khtpm_core_render.c:4649` `layout_sidebar_panel:4881` `for(i=0;i<page->n_children;i++) text direct child` `5305` `bar_max` `5404` `sw-inset*2` + `khtpm_css_parser.c` `CssStyle` `nb_css.c:787` `css_hidden`.
- **Task:** Extend `khtpm` `layout` for `network-browser-hq.xhtpm` `nb-content` `scrolllist` simple block flow: `y = parent y + sum prev siblings st.height` `display:none` ancestors `0,0,0,0` `css_hidden` `4649`, like `worker:layout_xy:787` `parent y + siblings heights`. Keep `RUNG 7 flex/grid` `ROADMAP.md:275` deferred, `litehtml` `30k` later. Expose via existing `Elem x/y/w/h` `CENTROID_GOLD_STD.md:160`.
- **Verify:** `wcs 9/9` `ancestor-hidden` + new `worker_img_test 3/3` `getBoundingClientRect` `x/y` `stacked` `0` `line_h` `text_h`, `dump_frame_png_op` `TEXT` at `y=0` `y=line_h` `IMG` at `y=text_h` not `0`.

### F4 — launcher + presentation gate (no C, just wire)
- **File:** `network-browser-hq.xhtpm:16` `module src=".../network_browser_manager.+x" id="ui"` `button.sh` `open_network_app.sh` `livedesk_launchers.pdl` `PROGRESS-network-browser-xhtpm.md:102` `+.hq-apps/network/presentations/network-browser-normal-20260923/manifest.txt` `6` `5-8s` `PIPELINE:35`.
- **Task:** Retarget `open_network_app.sh`/`button.sh` `default -> .xhtpm` `NB_ROLLBACK=1 -> .chtpm` `bootstrap` heal, keep `write_chtpm_projection()` until sign-off then delete `write_chtpm_projection` `+.bootstrap`. Harness `presentations/.../harness.sh:35` `DRY_RUN` `dump_frame_png_op --root` `manifest.txt` `REPRODUCE.md` `make_presentation_video.py --width 1280` `ffmpeg/edge_tts` `presentation.mp4`.
- **Verify:** `harness.sh --dry-run` `manifest 6` `01_load_example.png |6| TEXT/LINK via RENDER` `05_image_png.png` `06_layout_rect.png`, live `dump_frame_png_op --window` `960x45` `7.3K` `PAGE_STATE` `IMG|...|1|1|/tmp/...png` `TEXT` `LINK`.

## 3. Non-goals (stay small)
- No `Servo`/`WebKitGTK`/`CEF` `200MB` `network/_attic-gtk-embed/` `ROADMAP.md:608` `698` B/C — hand-built `A` readable web only `ROADMAP.md:642` `668` `Gmail/Figma/WebGL` out.
- No full `HTML5` error recovery `nb_dom.c:119` `50000` `void/rawtext` enough, degrade text-only.
- No `flex/grid/subgrid/compositor` `ROADMAP.md:636` `litehtml` later.

## 4. Execution + receipts (for you)
- One `git add <path>` `fix:`/`docs:` per `F1-F4` on your branch (`claude`/`grok`/`hai`), never `git add -A` `AGENTS.md`, `BRANCH-STRATEGY.md` `opencode` is ours `c8ce0ee38`.
- Gate each `F`: `make nbjs` `GREEN` `wcs 12/12` `wcn 11/11` `wck 3/3` `wst 2/2`, `dump_frame_png_op` `PAGE_STATE` `ui.txt` `xhtpm` `Elem` `X11` before `push`.
- Our `F1` `network` `quiet` `DONE` `a9edaef30` `0` `TEXT|js`, `F2` `wire` `12/12` `F3` `layout_xy` `9/9` already `GREEN` on `opencode` for you to consume — no `network` code needed from you.

## 5. Open question for you
- `F2` `/tmp/nb_img_*.png` lifetime/eviction `MAX_IMG_DECODED 256` `g_img_decoded` `139` cache overflow (your `draw` cache `139` `overflows once ... DISTINCT sprite paths`) — keep `/tmp` per page `LOAD` or per house `#.desktop/nb_sprites/` like `MEDIA|I` `1736`?
- `F3` `x/y/w/h` source of truth: `worker` `layout_xy` `787` `RER` `LAYOUT|` vs pure `khtpm` `layout_pass` `Elem` — `CENTROID_GOLD_STD.md:160` one tree, we kept `worker` minimal, you own `khtpm` final.

