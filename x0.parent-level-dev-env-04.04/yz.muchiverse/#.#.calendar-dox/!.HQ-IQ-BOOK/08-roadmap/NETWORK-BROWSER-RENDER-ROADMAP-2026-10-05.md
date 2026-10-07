# NETWORK-BROWSER RENDERING ROADMAP — 2026-10-05

Goal: render real web pages in the same khtpm window, with house nav
inputs, **no new deps**, and honestly — not via a browser-engine
embed. Reference: `2026-10-05-HANDROLLED-BROWSER-INVENTORY.md`.
Definition of done (from FRONTEND-PLAN §2): a human can read a real
content site without leaving the window; Chrome-pixel parity is not
the bar.

## Done

- [x] Fetch pipeline (`curl` + state files, request file, go/back/forward/reload url) — `network_browser_manager.c`
- [x] Tolerant DOM parse (`nb_dom.c`), CSS cascade (`nb_css.c`) — partial on hard pages
- [x] QuickJS worker runs page scripts; mutated DOM rows flow to page state (NB-JS-ENGINE-WORKER-PLAN rungs 1–5 partial)
- [x] Manager → `page.state.txt` → static xhtpm → khtpm renderer; incremental reparse preserves cli_io focus
- [x] Sidebar/navbar/bookmark/history/console rows fixed: explicit colors after black-on-black incident (`f09f4e63f`)
- [x] `install-deps.sh` now requires libav/ALSA; `nb_video_play` builds (`e75e6ff60`), local-file seekable bug fixed (`d1bc745ce`)
- [x] Video plays in the same window: YouTube watch → VT → `yt_resolve` → V3 op → `surface.raw` canvas row; `video.state` = `playing`
- [x] Worker LOAD wall-clock capped at 20s — js-heavy pages no longer hang at `Status: loading` (`3a1a59e55`)
- [x] House nav: relay file drives keys, address bar type/committed, tabs, back/forward/reload

## Done — Milestone 1: block flow (2026-10-05..07, all verified live)

- [x] One TEXT row per paragraph block (was fixed 88-col pre-wrap); renderer wraps at pane width (`bb99e4c92`)
- [x] Headings close as TITLE rows → page-title class (`1dec8c331`)
- [x] Wiki chrome junk filtered on both paths (extractor + worker-row merge) (`8ae47db9d`)
- [x] Manager TITLE survives worker merge (`3a366c527`)
- [x] Proof: Blockly loads ready, article skeleton renders as title rows

## Done — Milestone 2: rows to tiles (2026-10-07)

- [x] Fragment-only links → text/drop, consecutive dup links collapse (`c0262262a`)
- [x] Navigable inline links split into clickable rows; junk-href still folds (`ee25146f2`)
- [x] Skin-asset furniture images dropped both paths (`4fbd9a95f`)
- [x] Projection caps lifted (rows 128→2048, vars to 4096) so full articles render (`fd45a098f`, `20de0b6fc`)
- [x] Worker IMG rows route through MEDIA→sprite; static fallback when worker imageless; bad resume reverted (`9ca69d596`)
- [x] Fixtures: mini-article (fold/split/heading), media-grid (6-image tiling) — harness PASS
- [x] Harness v2: multi-fixture loop, normalized goldens (machine/run-order stable) (`663400000`)
- [x] Grid proven: 3x2 sprite-flow grid renders at wide window; narrow panes degrade to 1 column by design (no renderer change needed)
- [x] Click safety: malformed LINK URLs never navigate; nav-jump on a content link lands correctly (`9665531b0`)
- [x] Nested-tag link text spaced (`0f61a9c49`)

## Todo — Milestone 2 remaining

- [x] ~~sprite-flow grid inside the panel path~~ — proven working (delegates to layout_scroll_region); narrow-pane stacking is 1-col degradation by design
- [ ] True inline-clickable spans (segment row kind + renderer inline flow; needs xhtpm contract change)
- [ ] `file://` worker image resolution (worker emits zero IMG rows on file pages; static fallback covers it, but JS-driven pages depend on the worker path)
- [ ] Column width as computed layout state (CSS cap is a stand-in)

## Todo — Milestone 1: block flow (readable article) — SUPERSEDED by Done above

- [x] ~~Manager publishes per-row y-offsets/w/h/class~~ — not needed: renderer wraps rows at pane width
- [x] ~~xhtpm uses them~~ — static repeat + scroll region already scroll; nav numbers visible rows
- [x] ~~Proof: Wikipedia readable~~ — done (Blockly)

## Todo — Milestone 2: inline flow — SUPERSEDED by Done/remaining above

- [x] ~~Word-wrap spans~~ — renderer-side wrap suffices for now; true spans need segment rows (listed above)
- [ ] Inline media tiles (sprite-flow grid) — open (panel-path branch missing)

## Todo — Milestone 3: JS ↔ box tree reflow

- [ ] Manager owns the box tree; worker mutations (`textContent`, appendChild) reflow the subtree, not just rewrite a text row
- [ ] `getBoundingClientRect` returns real numbers, `el.style` writes reflow

## Todo — Milestone 4: nav behaviors

- [ ] Back/Forward/Bookmark/History act on real page box rather than state-file swapping
- [ ] Focus/hover semantics on links + buttons from box-tree hit-testing

## Todo — Later

- [ ] `@media` evaluation, cascade layers, media-query-driven layouts
- [ ] Flex row for toolbars; grid only after flex
- [ ] Wikipedia homepage → usable
- [ ] YouTube page → playable via V4 extraction only (already the special case); no plans to reverse-engineer Polymer UI

## Explicitly shelved / trashed

- `_trash/gtk-embed` — WebKitGTK paradigm, parked, not wired
- CEF OSR — full Chromium, rejected (see `2026-10-04-BROWSER-REAL-ENGINE-RESEARCH.md`)
- `network_browser_render.c` — old hand-rolled renderer, retired

## Non-goals, stated once

- Chrome-pixel-parity for complex SPAs as a launch bar (incremental SPA
  support as we go and see fit — QuickJS bridge, box reflow, canvas/WebGL
  surfaces each land when they land; no page class is refused up front)
- New system dependencies for the surface-level browser

## Commitment (2026-10-05)

We commit to doing this work. The definition of done is a reasonable,
non hobby/toy browser shipped from our own stack — no embedded engine,
no hidden upgrades to a "real browser". We will do it step by step and
will develop the supporting tooling as we go (test harness, debuggable
state files, per-layer fixtures, build scripts that prove builds),
keeping the codebase modular rather than shipping a monolith.

## Expanded scope (2026-10-05, second directive)

Maximal ambition mode. Updated away from the previous "Gmail/GMaps out"
stance: we will do everything we can to render the full class of modern
web pages, including Google-class JS apps (Gmail, GMaps, Photos-grade
pages). When such a site misbehaves, we treat it as a bug in our stack to
fix, not a product category to explain. Maintenance lag against upstream
sites is a maintenance cost, filed later — 80/20 rule; the 20% is for
later.

- Canvas 2D + WebGL surfaces feed the same `surface.raw` model the
  house video already uses, exposed through the QuickJS worker bridge.
- The renderer stays khtpm-generic; new page shapes become state-file
  rows / projector output, never per-app C.
- Mount topics (DOM box tree, JS reflow, CSP, web apps) stay in the
  milestone list where they are.
