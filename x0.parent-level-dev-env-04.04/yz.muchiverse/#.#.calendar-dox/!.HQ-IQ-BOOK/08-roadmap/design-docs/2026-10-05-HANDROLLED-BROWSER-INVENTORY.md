# HAND-ROLLED BROWSER STACK INVENTORY — 2026-10-05

**Why:** user decided to own the full rendering stack (§8 path A) rather
embed a real engine. WebKitGTK tree moved to
`&.hq-apps/network/_trash/gtk-embed/`. This doc is the concrete map of
what exists, what's partial, and what's missing, so the next agent
tackles missing pieces in dependency order instead of rediscovering the
gap live.

## Goal definition (from NETWORK-BROWSER-FRONTEND-PLAN.md §2)

A human can read a real content site (wiki/docs/blog) and light SPAs
without leaving our window. Parity with Chrome is NOT the target — a
*navigable, laid-out* page is.

## Present state of the layers

| Layer | Files / entry point | State | Gap |
|---|---|---|---|
| HTTP fetch + URL resolver | manager `do_fetch`, `resolve_url` | works (curl) | watch-URL JS extraction was unbounded — fixed 2026-10-05 with a 20s LOAD wall cap (`3a1a59e55`) |
| HTML → DOM | `nb_dom.c` | usable on well-formed pages | not a spec HTML5 parser — deep nesting/error recovery on real sites can still produce tar; entity decode gaps |
| CSS cascade | `nb_css.c` | id/class/type + `!important`, `display:none` zeroed metrics | no cascade layers, no `@media`, `el.style` writes don't reflow, no computed values for layout |
| JS host | QuickJS worker (`nb_js_worker.+x`) | runs scripts, RENDER rows merge into `page.state.txt` | bridge exposes text rows only — there is no box/visual tree to mutate |
| Video in page | `nb_video_play.+x` (V3) | works: watch-URL `VIDEO\|` row → V3 → `surface.raw` → canvas row in the CONTENT pane | remote http URLs still need the UA/referer chain verified (googlevideo 403 risk) |
| Rows → visual rows | manager merge_render_rows | one row per content item in `page.state.txt` | that's a *list* — every item gets one line height. No flow, wrap-as-page, or media grid reproducing square |
| Rows → Elem tree | shared `khtpm_core_render.c` | static xhtpm + repeat | blits rows; no CSS layout tree |
| CSS layout engine | — | **MISSING** | this is the big one: turn the DOM+CSS cascade into a positioned box tree |

## The missing core: box layout

What browsers call "layout" is: for every DOM node with a rendered box,
compute `x, y, width, height` from its CSS + its parent's box + siblings'
boxes. Without it all we have is a vertical list; no floats, no inline
flow, no multi-column text, no positioned headers, no media grids.

Minimal viable (first milestone — this is where I would start):

1. **Block box flow**: walk DOM in order; each block child takes the
   parent's width, its own height = content height; margins collapse;
   text wraps inside. This gets a *readable article page*.
2. **Inline flow**: text spans within a block wrap by word; `<a>` spans
   stay inline and are clickable. This is glyph-extent math; we have
   Xft text extents.
3. **Scrollable content pane**: the layout height exceeds the window —
   the scroll region belongs in the content `<scrolllist>`/`scrollregion`
   with a scroll offset driven by PageUp/Down; items nav-number only
   visible rows.
4. **Absolute/fixed**: skip for v1; these are 10% of sites, mostly ads.

Second milestone: **flex row** (a few layout modes already use flex in
khtpm's own layout pass — see `layout_scroll_region`/`layout_fixed_rows`)
for toolbars/cards. Grid last; YouTube-grade CSS grid is not worth
tackling until blocks work.

Third milestone: **JS → box tree reflow**. Currently the worker's
mutation of textContent becomes a row; with a box tree in the manager,
the same mutation path must reflow the subtree. Without this the worker
is decorative.

## What lands in worker vs manager vs renderer

House law: `khtpm_core_render.c` gets no per-app branch. So the box tree
belongs in the **manager** (it's business logic, state-owned), published
as a new row kind in `page.state.txt` — or better, a new projector file
`layout.txt` with per-item `x|y|w|h|class|label` — which the xhtpm
`<repeat>` consumes with absolute positioning. Renderer stays generic.

## Suggested first workable slice (do this next, next session)

1. Manager computes block-flow y-offsets for `page.state.txt` TEXT/LINK
   rows (content width = content pane width from `win_size.txt`).
2. Add a generic absolutely-positioned repeat row in the xhtpm that
   places each content item at its computed offset.
3. Scroll offset moves a region; keep the theme rows pinned.
4. Proof target: `go:` a real Wikipedia article, verify readable columns,
   no overlap at 2 line counts. (This is exactly the §1 layout-note fix
   that generalizing `scroll_row_span` helped with; don't repeat that bug.)

After that slice works, inline flow, media grid (`sprite-flow`), and
flex are the next layers.

## Dead ends (do not resurrect)

- `network_browser_render.c` old hand-rolled renderer — retired, keep
  parked.
- `_trash/gtk-embed` — WebKitGTK, parked as trash. See
  `2026-10-04-BROWSER-REAL-ENGINE-RESEARCH.md` for the record of why.
- Per-app `+x` binaries that bypass the manager+projection contract —
  violates house law.

## References

- `NB-JS-ENGINE-ROADMAP.md` (§8 = the engine policy)
- `NETWORK-BROWSER-FRONTEND-PLAN.md` (§1 pipeline, §2 definition of done)
- `PROGRESS-network-browser-xhtpm.md` (what the frontend already ships)
- `CENTROID_GOLD_STD.md` (house rendering law)
- `khtpm_reparse_diff.c` / incremental reparse (element identity quirk)
