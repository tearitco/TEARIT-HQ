# REPRODUCE — network-browser-normal presentation

**Branch:** `opencode` @ `4c652af7` parity 0 0, `make nbjs` GREEN, `wcs 12/12 PASS` (includes wcs[img] + wcs[img-png] + wcs[img-html]).
**Handoff:** `FRESH-AGENT-HANDOFF-NB-ENGINE-2026-09-23.md` (Steps 1-3 SHIPPED), `RUNG-7-IMG-SCOPE-2026-09-23.md`.

## Steps to redo the proof live

1. `make -C 44.xyz.01.00/&.hq-apps/network nbjs && make -C 44.xyz.01.00/&.hq-apps/network wcs` — must be GREEN, 12/12 PASS.
2. Launch Network Browser: `sh 44.xyz.01.00/&.hq-apps/network/button.sh run` (or `open_network_app.sh`), wait for `network_browser` window `960×640`.
3. For each manifest row, drive the browser to that state, then capture:
   `dump_frame_png_op --root <out.png>` or `dump_frame_png_op <window_id_hex> <out.png>` (PIPELINE:35 — never scrot).
   Verify receipt: `xwininfo` geometry + `page.state.txt` rows (`TITLE|`, `TEXT|`, `LINK|`, `IMG|src|1|1|/tmp/nb_img_*.png|alt`).
4. Place PNGs in `snapshots/` as named in `manifest.txt`, keep `manifest.txt` order.
5. Generate video: `make_presentation_video.py presentations/network-browser-normal-20260923 --width 1280` → `presentation.mp4` + `-yt-summary.txt`.

## What each snapshot proves

- **01** — `LOAD` → `RENDER` rows `TITLE|WCS`, `TEXT|`, `LINK|` via worker `nb_js_worker.c:447` + manager `merge_render_rows@1045`.
- **02** — `fetch()`/`XHR` via `nb_fetch_sync@3783` + `try_fetch_via_manager` + `handle_worker_fetch@1621` (manager RPC `7e55fc8b`), Promise `.then()` drains via `drain_jobs@3941`.
- **03** — `EVENT` RPC `43c72099` (`EVENT\n<selector>\n<type>` via `worker_send_event`, `cmd_event` dispatch through `dispatch_event@4171` + `onload`).
- **04** — `history.pushState`/`location` NAV via `g_pending_nav@1032` (rung 6 slice 2), `NAV\n<kind>\n<url>`.
- **05** — `<img src=data:image/png;base64,...>` decoded via `stb_image` `b64_decode` + `stbi_load` (`cdb51504`), `naturalWidth 1` (`wcs[img-png]`), `RENDER` `IMG|...|1|1|/tmp/nb_img_*.png` (70b), drawn via `khtpm_draw_core.c:1187` `stbi_load` + `XPutImage` at `getBoundingClientRect` (`514b8ab9` `layout_xy`).
- **06** — `getBoundingClientRect` stacked `y` via `layout_xy` (parent y + siblings' heights, `display:none` ancestors 0), `wcs` 9/9 CSS + `wcs[img-html]` 12/12.

## Current status

Snapshots are placeholders until live capture on a real X11 display with the browser window. Manifest and REPRODUCE are GREEN — fresh agent can run steps 2–5 on a live display to fill `snapshots/` and generate `presentation.mp4`.
