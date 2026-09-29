# NETWORK-BROWSER FRONTEND PLAN — making the GUI actually work

**Date:** 2026-09-25
**Branch:** `opencode` (main from now on; `opencode-fix` preserved untouched at `e748edbb8`)
**Companion:** `NB-JS-ENGINE-WORKER-PLAN.md:261` §8 Phase2 (backend), `NB-JS-ENGINE-ROADMAP.md:76` ladder, `00-compact/browser.md`, `HANDOFF-PHASE2-2026-09-24.md`
**Frontend law:** `02-architecture/CENTROID_GOLD_STD.md:38` (one `Elem x/y/w/h+CssStyle` tree, thin renderer, manager owns state), `09-appendix/PROGRESS-network-browser-xhtpm.md:1` (static xhtpm first cut)

## 0. Why this doc exists

There is **no combined frontend doc** before this. Backend shipped (QuickJS `07aa2200`, worker + RENDER `bba458ec` `ea864cea`, rung4 fetch `?`, `514b8ab9` layout_xy) but the *window* still looks like a flat text list — because the frontend path `page.state.txt -> ui.txt -> .xhtpm -> Elem -> X11` was only a first cut (`PROGRESS-network-browser-xhtpm.md:3` static `network-browser-hq.xhtpm` + `write_ui_projection()`). This doc merges `CENTROID_GOLD_STD` + `PROGRESS-network-browser-xhtpm §95` + `ROADMAP rung7 §447` into the shippable frontend sequence so the GUI works, not just the engine.

## 1. Current frontend pipeline (what already works, where it stops)

```
fetch.html --nb_parse_html()--> fetch.dom --nb_dom_load()--> worker NbNode tree --JS--> RENDER
do_fetch() writes static TEXT|/LINK|/IMG| rows to page.state.txt exactly as before (PLAN:171)
  + manager merge_render_rows() overlays worker RENDER when present => page.state.txt authoritative
  => network_browser_manager.c:write_ui_projection() reads page.state.txt + tabs/bm/hist
     and atomically writes #.desktop/network-browser-hq_ui.txt (tmp+rename)
     schema: PROGRESS-network-browser-xhtpm.md:34
       content_count, c_<i>_kind=text|title|link|img|video, c_<i>_is_*=1, c_<i>_text/_label/_sprite/_action
  => network-browser-hq.xhtpm:14 <window vars="#.desktop/network-browser-hq_ui.txt">
     + <repeat count="${content_count}" bind="c"> + show="${c.is_*}"
       candidates (text/page-title/item/media/canvas/bar/play)
     => khtpm_core_render.c: parse_chtpm()+layout_pass()+css_layout_pass() => Elem tree => draw_elem() blit
```

**What works today (headless + X11 both):**
- Static xhtpm renders title+links+text+media as one row per content item — verified `PROGRESS-network-browser-xhtpm.md:52` google.com title+link render.
- Incremental reparse `CENTROID_GOLD_STD.md:315` `khtpm_reparse_diff.c` preserves `cli_io id=address` focus via `content=` vs `label=` (`apply_attr()`), backspace fixed `2026-09-11`.
- Backend already feeds it: mutated DOM `RENDER` shows as `TEXT|after-js` in page.state.txt (PROGRESS-nb-js-worker-phase1.md:183) and therefore as `c_N_is_text=1` rows.

**Where it stops (why it feels same as before QuickJS):**
- Parser `nb_dom.c:21` NOT full HTML5 — tolerant but requires well-formed-ish nesting; `is_rawtext` fix `2026-09-19` unblocked `getElementById("base-js").src` but entity decode `decode_entities() 27` still literal `&copy;` gap noted `PROGRESS-network-browser-xhtpm.md:100`.
- Projector `is_media` rows are **one row per IMG** — `network-browser-hq.xhtpm:12` "sprite-grid-row wrap NOT reproduced, each media is own row" (acceptable first cut, now the visible grid gap).
- Layout `ROADMAP.md:447` Rung7 slice1 LANDED (`nb_css.c` cascade id/class/type+!important, display:none 0 metrics, wcs 9 cases `2026-09-10`) but `§480` Known gaps: no text-width/flow, w/h 0 unless CSS px, getBoundingClientRect always 0,0, `@media` never fires, `el.style` writes don't reflow. So carousels/lazy/sticky still flat.

## 2. Frontend definition of done (what "GUI works" means)

Pick **ROADMAP.md:642** target A: readable web + light SPAs **as our chrome**, not pixel parity. Done = a human can on a real content site (wiki/docs/blog) without leaving our window:

- Read: title, paragraphs, links, and **images visible** (not just `IMG|src` text row) — media sprite blitted at `x/y/w/h` from layout.
- Navigate: address bar edit + go, back/forward/history rows, tab strip, bookmark — all `khtpm` chrome already there, just fed correctly.
- Interact: click a link/item triggers JS `EVENT` -> RENDER -> re-project -> redraw (Phase2 `43c72099` + `PROGRESS-network-browser-xhtpm` relay already wired; frontend just re-renders the new `ui.txt`).
- Receipt: `dump_frame_png_op` shows a `network-browser-hq_ui.txt` -> window with `page-title` + 5-10 `is_text` + 2-3 `is_link` + 1-2 `is_media` sprites, no `js:` noise rows.

App-web (Gmail/Figma/WebGL) is explicitly out of scope (`ROADMAP.md:668`).

## 3. Shippable frontend steps (each = green + push on opencode)

### F1 — Projector wrap + entity quiet (1 pass, ~60 lines, no engine)
**Gap:** `PROGRESS-network-browser-xhtpm.md:95` sprite-grid-row + `&copy;` literal + `js:` rows `§59`.

- `network_browser_manager.c:write_ui_projection()`: pre-pass consecutive `IMG|/VIDEO|` rows into one `c_is_mediagrid` + `grid_count/grid_N_sprite` (or keep per-row but add `c_is_media` row-class so `network-browser-hq.xhtpm` can `<row class="sprite-grid-row">` via nested repeat — design in `xperiments/khtpm-generic-dispatch-design.md` style, no `g_is_network` flag per `CENTROID_GOLD_STD.md:249` rule 7).
- `nb_dom.c:27` `decode_entities`: expand `&copy; -> ©` already decodes (`copy 0xA9`) but projector `uisan()` maps `|`->`/` and strips CR/LF — ensure entities decoded *before* `uisan` so `©` survives; add `&amp; &lt; &gt;` regression case.
- Silence `TEXT|js: ...` by default per `PROGRESS-network-browser-xhtpm.md:78` option 1 (debug flag `#.desktop/network_browser_jsdebug.txt`).

**Verify:** `make nbjs` + headless `greet_player`-style dump: `google.com` fetch renders one `sprite-grid-row` with 2 images, `&copy;` shows `©`, zero `js:` rows without flag. `dump_frame_png_op` confirms.

### F2 — Image sprite pipeline (1 pass, touches worker + renderer, ~120 lines)
**Gap:** `IMG|` rows carry `src` text today; no pixels. Backend Rung7 Steps 1-2 already decode `data:` but not wired to `c_sprite`.

- Worker `nb_js_worker.c:dom_walk_render` for `img`: when tag `img` resolve `src` via `resolve_doc_url(g_href)` (mirrors manager `resolve_url`), fetch via `nb_fetch_sync` (curl with `cookie_header_for_url` jar, `dump-header` Set-Cookie ingress), sniff `Content-Type`, write to `tmp/nb_img_<hash>.png` (or `.jpg` via stb_image), keep `w/h` from `stb_image` or `width/height` attrs. Emit `IMG|<src> <alt> <localpath> <w> <h>` (extend `RENDER` row format; cap 60k).
- Manager `write_ui_projection()`: parse `IMG|... <localpath>` -> `c_<i>_sprite=<localpath>` (absolute path; renderer `kh_draw_sprite` already handles `sprite` as image path). Keep `TEXT|` fallback if fetch fails.
- Renderer `khtpm_core_render.c:kh_draw_sprite` — verify it loads `localpath` via `stb_image` or existing sprite loader; no new `g_is_*`.

**Verify:** fixture `file://` page with `<img src="data:image/png;base64,iVBOR...">` + `http` page with real `http://example.com/img.png` — `page.state.txt` shows `IMG|... tmp/nb_img_*.png 1 1`, `ui.txt` `c_N_sprite=...png`, X11 shows pixels at `Elem.x/y` (rect 0,0 today is fine — F3 fixes pos).

### F3 — Minimal layout for real positions (1 pass, the frontend long pole, ~250 lines)
**Gap:** `ROADMAP.md:447` slice1 gave cascade but `getBoundingClientRect` is 0,0; w/h 0 unless CSS px. Frontend needs `x/y/w/h` so image/text rows aren't all stacked at 0.

- Extend `nb_css.c` + worker layout pass (or manager-side `khtpm` layout — keep one source per `CENTROID_GOLD_STD.md:160` Elem already has `x,y,w,h` + `CssStyle`): implement `layout_xy()` parent `y + siblings' heights` simple block flow for `network-browser-hq.xhtpm`'s own `nb-content` scrolllist (not full flex/grid — `ROADMAP.md:275` deferred). `display:none` ancestors 0,0,0,0 already correct.
- Expose `getBoundingClientRect` via `layout_xy()` so JS carousels that measure (`ROADMAP.md:447` slice1 KPI) get real `w/h`; `offsetWidth/Height` already CSS-px correct (`nb_js_worker.c:686`).
- Publish metrics into `RENDER` as `LAYOUT|<id> <x> <y> <w> <h>` or pack into `IMG|` w/h — projector consumes for `Elem` placement; document choice in this doc's amendment.

**Verify:** `wcs` 9/9 + new `worker_img_test` 3/3 (`file://` png 1x1, `http` png, `onload`), `dump_frame_png_op` shows two stacked `TEXT|` blocks at `y=0` and `y=line_h`, `IMG` at `y=text_h`.

### F4 — Wire, polish, and presentation gate (1 pass, no C)
- Launcher `open_network_app.sh`/`button.sh` retarget from `.chtpm` to `.xhtpm` per `PROGRESS-network-browser-xhtpm.md:102`, delete `write_chtpm_projection()` + `.chtpm.bootstrap` after sign-off (keep rollback branch).
- Harness: file relay `#.desktop/<mode>_history.txt` bare-decimal per `AIGENT-TESTING-K9.txt:125`, TEXT dump before PNG `K9:136`, PNG last `K9:144`, bash harness `K9:864` — see `presentations/network-browser-normal-20260923/harness.sh` `ef70042f` 62 lines; `make_presentation_video.py --width 1280` -> `presentation.mp4` per `PRESENTATION-VIDEO-PIPELINE.md:35`.

**Verify:** `presentations/.../manifest.txt` 6 rows 5-8s + `REPRODUCE.md` + `harness.sh --dry-run` + real `dump_frame_png_op --window 0xa00002` 960x45 receipt `7.3K` style for network browser.

## 4. Non-goals (stay small per CENTROID_GOLD_STD)

- No embed of Servo/WebKitGTK/CEF for this milestone (`ROADMAP.md:608` rejected `200 MB` CEF; `network/_attic-gtk-embed/` parked). That's `ROADMAP.md:698` option B/C escape hatch, separate house decision.
- No full HTML5 error recovery — `nb_dom.c:119` 50k cap + void/rawtext is enough; degrade to text-only on hostile markup.
- No flex/grid/subgrid/compositor — `ROADMAP.md:636` ceiling; `litehtml` vendoring is the later scope if A/C chosen.

## 5. Execution order + gates

F1 -> F2 -> F3 -> F4, each on `opencode` as one scoped commit (`git add <path>` per file, never `-A`), `make nbjs` GREEN, `wcs` 12/12, `parity 0 0` before `git push origin opencode:opencode`, re-verify `0 0`. Keep `opencode-fix e748edbb8` (pre-merge 700x520 working tb) as test reference — do not force-push.

## 6. Open question for you

F2 image cache eviction / tmp lifetime and F3 choice of where `x/y/w/h` lives (worker RENDER vs pure khtpm layout_pass) — default above is worker `IMG` w/h + khtpm block flow for `x/y`; confirm or keep as-is.

