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

## Todo — Milestone 1: block flow (readable article)

- [ ] Manager publishes per-row y-offsets/w/h/class so TEXT/LINK rows lay out as a column, not a list
- [ ] xhtpm uses them; content scroll region scrolls; nav indexes only visible rows
- [ ] Proof: `go:` a real Wikipedia article → readable, no overlap at 2 line counts

## Todo — Milestone 2: inline flow

- [ ] Word-wrap text spans inside a block row using Xft extents; links stay inline + clickable
- [ ] Inline media tiles (sprite-flow grid) — interim instead of real block-embedded `<img>`

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

- Chrome-pixel-parity for complex SPAs
- Reimplementing a general media player / JS SPA host
- New system dependencies for the surface-level browser
