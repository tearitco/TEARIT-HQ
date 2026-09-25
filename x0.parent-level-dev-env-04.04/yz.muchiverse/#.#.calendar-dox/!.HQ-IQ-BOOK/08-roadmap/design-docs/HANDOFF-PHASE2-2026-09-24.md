# HANDOFF — Phase 2: Network Browser Chromium-like Parity (JS + DOM + Fetch + History)

**Date:** 2026-09-24
**Branch:** `opencode` (opencode-fix is pre-merge working tb at fefbbd0a, now merged back via ee6a30fe1)
**Status:** Phase 1 **RETIRED** — see `NB-JS-ENGINE-WORKER-PLAN.md` Status RETIRED 2026-09-23, §8.6 SHIPPED-LOG Rungs 3/4/5 + Rung 7 Steps 1-2.
**Next:** Phase 2 per `NB-JS-ENGINE-WORKER-PLAN.md:261` §8 + `NB-JS-ENGINE-ROADMAP.md` Rung 7. This doc is the hand-off for the next agent.

## Why Phase 1 felt the same as before QuickJS

- **HTML parser still `nb_dom.c`** — `Manager side (nb_dom.h/in manager): parse won't live in JS. Compact C parse + serialize; emit fetch.dom` (`PLAN:98`). No HTML5 error recovery.
- **`page.state.txt` still `do_fetch` static first** — `do_fetch writes static rows exactly as today` (`PLAN:171`), `WORKER RENDER` merged only when `RENDER` exists (`merge_render_rows@1045`). `no-script` pages never spawn worker, `byte-identical`.
- **QuickJS graft DONE 2026-09-18** (`browser.md:10` `07aa2200` `44 PASS`) was **engine-cell translation** Duktape → QuickJS, not browser feature. `DOM rung 2` + `RENDER glue` `rung 5` landed, but `Phase 2` (`rung 3` events, `rung 4` XHR/fetch, `rung 6` BOM) was `hand-off-ready` not done — hence same feel.

## What Phase 1 shipped (for reference, not to redo)

- `rung 2` DOM tree + accessors, `rung 5` RENDER glue (`TITLE` first, 60k), worker plumbing — 6 commits `68df5763`..`4bb45dfa` on `origin/chtpm-js-rungs` (handoff-2026-09-04-slave).
- `514b8ab9` Phase 3 slice 2 `layout_xy` `getBoundingClientRect` `wcs 9/9`
- `e4428e13`/`c1a72fdd`/`cdb51504` `Rung 7 <img> Steps 1-2` `HTMLImageElement` `src` fetch + `stb_image` decode `naturalWidth` `wcs[img]`
- `d8bc3378`/`6e8d5308` `Step 3` wire `IMG|<src>|<w>|<h>|<path>|<alt>` + `khtpm_draw_core.c:1187` `stbi_load` `XPutImage`
- All on `opencode` `600x400` `500` strip `0f78a225`/`052ef9b19` + `board hint` `1b4cc3c5` (lost in grok merge `8984b20c`, restored `c4957c64`/`2e79554a`).

## Phase 2 — what is hand-off-ready (§8) and what to do

**Plan §8.2 + §8.5:** `rung 3` events + loop, `rung 4` XHR/fetch via manager RPC, `rung 6` real BOM (`history` `location` `cookies` `MutationObserver` stub), `rung 5` re-serialize already `DONE` (glue).

**Already SHIPPED in-tree (verify before claiming as next):**

- `rung 3` `43c72099` `EVENT RPC` — `manager -> worker: EVENT\n<selector>\n<type>` via `worker_send_event`, `cmd_event` dispatch through `dispatch_event`/`on-property`, drains microtasks/timers, re-emits `RENDER` — user click in window → scripted el.
- `rung 4` `7e55fc8b` `async FETCH RPC` — `worker -> manager: FETCH\n<id>\n<method>\n<url>` / `manager -> worker: FETCHED\n<id>\n<status>\n<body>` (manager `handle_worker_fetch` in `worker_load@1621`/`worker_eval@1691`, worker `try_fetch_via_manager` + fallback `curl`).
- `Rung 5` `RENDER` glue `ea864cea` `RENDER\n<rows>` `60k` — already authoritative.

**Verify-only (do not re-land):** `history.pushState`/`location` NAV (`g_pending_nav@1032`), `document.cookie` file-backed (`1849`), `MutationObserver` stub (`2776`).

**Next green frontier (pick one, one commit at a time):**

1. **`Rung 6` remainder verify-only** — confirm `history.pushState` address-bar update without fetch, `location.assign` NAV, `cookie` persistence across `LOAD`s (each `LOAD` fresh engine, jar on disk `#.desktop/nb_cookies.txt`).
2. **Phase 3 Rung 7 slice 3** — `khtpm_draw_core.c` `draw-image` at `getBoundingClientRect` `514b8ab9` is `SHIPPED 6e8d5308` wire, but full `flex`/`grid` layout is deferred per `PLAN:275` — only if carousels/lazy need it.
3. **Presentation** `PIPELINE:35` `presentations/network-browser-normal-20260923/` — `manifest.txt` `6` rows + `harness.sh` `K9` bash + `REPRODUCE.md` are `GREEN` (`ef70042f`), snapshots pending live `X11` `dump_frame_png_op --root` on `opencode` `4c652af7` (`wcs 12/12` after `c1b9ae45`).

## House law for next agent

- **Branch:** `opencode` only (never `main`/`claude`/`grok`). `opencode-fix` `e748edbb8` is `pre-merge` `working tb` preserved for testing, now merged back via `ee6a30fe1` — do not force-push `opencode`.
- **K9 drive:** `khtpm` family → file relay `#.desktop/<mode>_history.txt` bare-decimal, `TEXT` dump `page.state.txt` `RENDER` rows before `PNG` (`dump_frame_png_op`), never `scrot`/`xwd` (`K9:505`).
- **Gate:** `make nbjs` `GREEN` (`0`), `wcs` `12/12` (`11/11` + `img` `1x1`), `parity 0 0` before push, one non-force `push`, re-verify `0 0`.
- **Commit:** `git add <path>` per file, never `git add -A` — runtime `*.pdl`/`history.txt` are noise.

## Timeline / KPIs for Phase 2 (for go-ahead)

- **Timeline:** `rung 3` `1w` (events), `rung 4` `1w` (XHR/fetch RPC already `7e55fc8b` — verify-only), `rung 6` `1w` (BOM verify) — `3w` total to `Phase 2` `GREEN`, based on `rungs 3-6` velocity `7` commits/`19d`.
- **KPIs:** `make nbjs` `GREEN`, `wcs` `12/12`, `worker_img_test` `3/3` (`file://` png, `http` png, `onload`), `dump_frame_png_op` shows `IMG` `1x1` at `getBoundingClientRect` `w/h`, `page.state.txt` `IMG|src|1|1|/tmp/nb_img_*.png`.

## Retire for the evening

- `opencode` `052ef9b19` `500×350` `500` strip is after `grok`+`claude` merges, `board hint` restored, `presentation` scaffold `ef70042f` + `harness.sh` `K9` ready.
- `opencode-fix` `e748edbb8` `fefbbd0a` `700×520` `working tb` preserved for testing.
- Next agent resumes from this doc + `FRESH-AGENT-HANDOFF-NB-ENGINE-2026-09-23.md` + `RUNG-7-IMG-SCOPE-2026-09-23.md`.

