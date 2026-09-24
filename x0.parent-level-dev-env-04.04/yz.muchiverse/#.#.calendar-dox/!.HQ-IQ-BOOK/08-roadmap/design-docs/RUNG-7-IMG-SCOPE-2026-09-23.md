# Rung 7 — <img> support — Scope 2026-09-23

**Branch:** `opencode` @ `8bfe921e` parity 0 0, `make nbjs` GREEN.
**Gate:** `PRESENTATION-VIDEO-PIPELINE.md:115` — network browser is TEXT-only today; image support is next before normal-browser presentation.

## Current state (TEXT-only)

- **DOM:** `nb_dom.c:83` lists `img` as void element, `nb_js_worker.c:505` emits `IMG|<sprite_dir>|<alt>` rows (house sprite.csv, no decode). `nb_dom.h` has no `img` fetch.
- **Wire:** `IMG|<sprite_dir>|<alt>` text rows via `page.state.txt` (`network_browser_manager.c:30`), not binary.
- **Renderer:** `khtpm_draw_core.c` draws sprite tiles, no `stb_image` decode, no `draw-image` op with clip.
- **Decoder:** `stb_image.h` already in house (`44.xyz.01.00/&.hq-apps/js/stb_image.h`, `101.mutaclsym.../ops/lib/stb_image.h`, `stb_image_write.h` in same lib) — per `PIPELINE:115` ready to wire.

## Required per PIPELINE:115 (in order)

1. **Decoder in worker** — vendor `stb_image.h` into `&.hq-apps/network` (copy from `&.hq-apps/js/`), `stbi_load` from `fetch` body bytes.
2. **`img` element + fetch in `nb_dom.c`** — `HTMLImageElement` (src, onload/onerror, complete), fetch `src` via existing `nb_fetch_sync` / `FETCH` RPC `7e55fc8b`, decode to RGBA, store in node.
3. **Binary `IMG` wire frame** — extend `RENDER` rows or add `IMG_BIN|<w>|<h>|<rgba-b64>` (or file path under `#.desktop/nb_images/`), keep text `IMG|...` as fallback for TEXT-only readers.
4. **Draw-image op + clip in renderer** — `khtpm_draw_core.c` / manager `network_browser_manager.c:3456` `IMG`/`VIDEO` run handling: decode `w/h`, `stbi` RGBA → XImage, clip to `getBoundingClientRect` (`514b8ab9` layout), draw.

Video (`ffmpeg` demux/decode) after `<img>` — deferred, not in this scope.

## Scope verdict

**Step 1 SHIPPED e4428e13 — next is Step 2 (stb_image decode).** All 4 steps are new code; no in-tree `stb_image` decode for network browser, no `img` fetch, no binary wire, no draw-image. `TEXT-only` is correct per `PIPELINE:115`.

## Next bounded steps (each GREEN, parity 0 0, one push)

1. **Step 1 — img element + fetch (no decode yet) — SHIPPED e4428e13 + fix c1a72fdd:** `HTMLImageElement` src accessor (`nb_img_src_get/set` + `img_get_src` map, `window.Image` alias), fetch via `nb_fetch_sync` (data:/http(s) via manager RPC), fires `load`/`error` via `dispatch_event`; `wcs[img]` 10/10 PASS (`c1a72fdd` fix) — `make nbjs` GREEN.
2. **Step 2 — stb_image decode + binary wire — SHIPPED cdb51504:** vendor `stb_image.h` (`../js/stb_image.h`), `b64_decode` + `stbi_load_from_memory` for `data:image/png;base64` in `nb_img_src_set`, store `g_img_decoded`, `naturalWidth`/`complete` — `wcs[img-png]` 1x1 PASS.
3. **Step 3 — renderer draw + clip — wire SHIPPED d8bc3378 (draw still open):** `khtpm_draw_core.c` `draw-image` with `getBoundingClientRect` layout, clip, `make nbjs` + `dump_frame_png_op` proof — worker wire now `IMG|<src>|<w>|<h>|<path>|<alt>` with file, manager parses and sets `sprite=path`.

## House law

- Verify `stb_image.h` already in house; do not vendor duplicate if `&.hq-apps/network` can include from `&.hq-apps/js/`.
- One commit per step, `make nbjs` GREEN, `wcs` GREEN, parity 0 0, non-force push.
- Presentation parked until `<img>` lands — see `FRESH-AGENT-HANDOFF` Future Todo.

## Timeline / KPIs (for go-ahead)

- **Timeline:** Step 1 `≈1 week` (dom + fetch), Step 2 `≈1 week` (decode + wire), Step 3 `≈1 week` (renderer) — **3 weeks total** to `<img>` GREEN, based on `rungs 3-6` velocity (7 commits in 19 days).
- **KPIs:** `make nbjs` GREEN, `wcs` 9/9, new `worker_img_test` 3/3 PASS (file:// png, http png, onload fires), `dump_frame_png_op` shows `IMG` rendered at `getBoundingClientRect` position (not 0,0), `page.state.txt` `IMG|...` rows carry `w/h`.
