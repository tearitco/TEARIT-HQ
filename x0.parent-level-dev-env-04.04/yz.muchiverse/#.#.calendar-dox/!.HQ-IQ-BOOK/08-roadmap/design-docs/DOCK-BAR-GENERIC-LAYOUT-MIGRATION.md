Dock Bar Generic Layout Migration
====================================
Reference doc, 2026-09-14. PLAN doc, written before any code change per
direct instruction ("we need to scope a documented plan to do this fix
the right way"). Nothing in this doc is built yet.

## 1. The real problem, stated plainly

Direct live report, same session: the bottom taskbar's `+`/`-` row
pager rendered in the wrong place, wasn't centered in its own reserved
margin, and a second row of entities visually overlapped the first.
All three are real, hand-math bugs I introduced or found while
patching `layout_dock_bar()`/`dock_place_pager()` directly - and all
three are exactly the CLASS of bug a real layout engine eliminates by
construction, not something that needed three separate hand-fixes.

Direct follow-up: "should we really be hardcoding this? doesn't the
tb's use layouts/managers?" and "this was supposed to have been
refactored already and the old hardcoded code cleaned out of the
core." Checked the house's own docs for that prior plan
(`LIVEDESK-UI-SCALE.md`'s "dock strip math is most hardcoded" note is
about DPI `scaled()` coverage, not this; `XHTPM-PARSER-REFERENCE.md`
§9.1 just catalogs `layout_dock_bar()` as one of several real layout
modes, doesn't propose removing it) - no such plan exists yet. This
doc is that plan, written now.

## 2. What's hardcoded today, and why it's the wrong shape

`khtpm_core_render.c`'s dock mode does NOT use the generic
`display:flex`/`<panel>` CSS layout engine (`css_layout_pass`,
`khtpm_render_core.c`) every other real layout mode uses -
`layout_sidebar_panel()` (db-hq-pal, DSR, canvas-craft's own proven
3-column flex-wrap shape) all go through it. The dock bar instead has
its own, separate, hand-written packing algorithm:

- `layout_dock_bar()` - manual column-x advance + row-wrap loop
  (`col_x`, `r`, `max_w`), reimplementing what `flex-wrap: wrap` does
  generically for canvas-craft's own tile grid already.
- `dock_place_pager()` - hand-computed `+`/`-` button positions (today's
  own bug: wrong side, then not centered - both were literal pixel-math
  mistakes in C, the exact class of error a declarative layout can't
  make).
- `dock_item_cw()` - manual per-cell width computation
  (`6 + DOCK_NAV_BADGE_PX [+ DOCK_SPRITE_PX + 4]`), duplicating what
  the generic flex/measure path already does for every other window's
  own `<item>`s.
- `dock_draw_separators()` - manual `|` glyph placement between cells,
  computed from raw `prev->x + prev->w` arithmetic.
- A cluster of hardcoded pixel constants: `DOCK_BAR_H`, `DOCK_SPRITE_PX`,
  `DOCK_CELL_GAP`, `DOCK_NAV_BADGE_PX`, `DOCK_FOCUS_BOX_W`,
  `DOCK_PAGER_W` - none of these are real CSS properties a template
  author can see or change; they're invisible to anyone editing
  `khtpm_strip_header.xhtpm`/`khtpm_strip_bottom.xhtpm`.
- The `+`/`-` row-paging affordance itself (`g_dock_plus_elem`/
  `g_dock_minus_elem`, `PAGEROW:±1`) is a bespoke, one-off mechanism -
  this house already has a real, generic, PROVEN equivalent
  (`generic_sbar_register()`/`SCROLLUP:<i>`/`SCROLLDOWN:<i>`, used by
  the sidebar+panel scroll region, the swatch grid, and more) that a
  flex-wrapped dock row could reuse directly instead of duplicating.

This violates this house's own standing rule (`CENTROID_GOLD_STD.md`,
`khtpm-house-standards` skill): "zero new per-project C in the
renderer, generic tag vocabulary only." The dock bar is the one real
layout mode left that doesn't follow it.

## 3. Real target shape

`khtpm_strip_bottom.xhtpm`'s toolbar row becomes a real flex-wrap
container, same proven shape `canvas-craft.xhtpm`/`canvas-craft.css`
already use live:

```
<row class="dock-toolbar-row">   <!-- CSS: display:flex; flex-wrap:wrap -->
  <repeat count="${n_tabs}" bind="tab">
    <item id="tab${tab.#}" class="dock-cell" label="${tab.label}" .../>
  </repeat>
</row>
```

- Row-wrapping: `flex-wrap: wrap` on the container, real CSS, same
  mechanism the periodic-table/tile-grid work already proved (see
  pitfall #14, `HOUSE_CODE_PITFALLS.md` - "reuse the existing scroll
  path", the exact lesson this migration is applying at the
  ARCHITECTURE level instead of per-bug).
- Overflow beyond the visible row budget: a real `generic_sbar_register()`
  scroll region, not a bespoke `+`/`-` pair - inherits real nav-numbered
  `^`/`v` arrows, real drag-thumb, and (per pc-hq's own working
  reference, already cited this session) right-side placement FOR FREE,
  no hand math.
- Cell width/height: real CSS (`width`, `height`, `padding`), computed
  by the shared measure/layout path every other window already uses -
  `dock_item_cw()` deleted, not reimplemented.
- Separators: either a real CSS `border-left` on each cell (simplest,
  matches how every other bordered-box in this house already does
  dividers - canvas-craft's own `.cc-panel { border: 1px solid ... }`)
  or dropped entirely if the generic flex gutter reads cleanly enough
  without them - decide during implementation, not pre-designed here.

## 4. Staged migration plan

**Phase 1 - convert row-packing to real flex-wrap, keep the pager.**
Change `khtpm_strip_bottom.xhtpm`'s toolbar `<row>` to
`display:flex; flex-wrap:wrap`, remove `layout_dock_bar()`'s manual
column/row-advance loop, let `css_layout_pass` do it. Keep
`dock_place_pager()` as-is for this phase (already just fixed, not
broken) so this phase is scoped to ONE real change at a time,
verified via the standard house testing protocol (relay clicks +
`dump_frame_png_op`, not guessed) before phase 2 starts.

**Phase 2 - replace the `+`/`-` pager with the real generic scrollbar.**
Delete `g_dock_plus_elem`/`g_dock_minus_elem`/`dock_place_pager()`/
`PAGEROW:±1` entirely; wire a `generic_sbar_register()` call for the
dock's own toolbar region instead, matching the sidebar+panel scroll
region's own real call shape. This is the phase that actually removes
the bug class today's live reports came from, not just relocates it.

**Phase 3 - delete the now-dead hardcoded constants/functions.**
`DOCK_SPRITE_PX`, `DOCK_CELL_GAP`, `DOCK_NAV_BADGE_PX`, `DOCK_PAGER_W`,
`dock_item_cw()`, `dock_draw_separators()` (if superseded by CSS
borders) - remove for real, not leave as dead code, matching this
house's own "delete, don't half-remove" convention. `DOCK_BAR_H` and
`DOCK_FOCUS_BOX_W` likely survive as real CSS values on
`khtpm_strip_header.xhtpm`/`khtpm_strip_bottom.xhtpm` instead of C
`#define`s.

## 5. Real risks / must-not-break list

- **Nav numbering stays globally unified** (this house's own standing
  convention, `khtpm-house-standards` skill's own "Rule 7") - the flex
  engine must assign `nav_index` in the same visual left-to-right,
  top-to-bottom order the hand-packer currently does, or every
  digit-jump test/AI-driven click in this house silently desyncs.
- **The dock's own chrome-less, override-redirect, always-on-top
  window semantics** (no title bar, no X/!/_ chrome trio, pinned
  screen edge) are dock-specific behavior living OUTSIDE
  `layout_dock_bar()` itself (window creation flags) - must not be
  touched by this migration, scope is layout only.
- **The focus-box (`DOCK_FOCUS_BOX_W`) and the peer-window split**
  (`g_dock_peer`/`g_dock_peer_win`, this same session's own 5th/6th/7th
  bounty occurrences) are a separate, already-hardened mechanism - this
  migration changes HOW cells lay out inside each window, not the
  two-window architecture itself.
- **Test via the real house protocol before/after each phase**
  (`AIGENT-TESTING-K9.txt`'s relay convention + `dump_frame_png_op`
  frame dumps) - this exact bug class (pager position, centering, row
  overlap) was only ever caught this session by dumping real pixels,
  never by reading the code alone. Same standard applies here, doubly:
  a layout-engine migration is exactly the kind of change that can
  look correct in the diff and still misrender live.

## 6. Status

**Phase 1 DONE and verified live, 2026-09-14** - merged the bottom
bar's three real `<row>`s into one `<row class="toolbar dock-flexrow">`
(`khtpm_strip_bottom.xhtpm`); `.dock-flexrow { display:flex;
flex-direction:row; flex-wrap:wrap; }` added - real gotcha found and
fixed live: this rule had to go in `khtpm_strip_header.css`, NOT a
same-stem `khtpm_strip_bottom.css` sibling, because CSS auto-load is
keyed off `main()`'s own startup `g_chtpm_path` (the header's path)
only - the peer never gets its own separate load, confirmed by reading
the real load site, not guessed (a first attempt at
`khtpm_strip_bottom.css` silently never took effect). `layout_dock_bar()`
now only measures each cell's real content width
(`dock_item_cw()`, kept for that, not positioning) and hands the actual
row-wrap/column-advance math to `css_layout_pass()`; nav-index
assignment happens in a real post-layout pass over the now-positioned
children, same visual order as before. `DOCK_MAX_PACK` deleted (real,
not half-removed) along with the `pack[]` array it sized.

Verified live via real frame dumps (not guessed): row 1 renders
correctly; paging to row 2 (`PAGEROW:+1` via the real pager) shows a
clean second row with ZERO bleed into row 1 (today's own reported
bug); paging back (`PAGEROW:-1`) collapses cleanly; the header window
(untouched by this phase - still its own separate hand-packed branch)
confirmed unaffected.

**Phase 2 (replace the bespoke +/- pager with the generic scrollbar)
and Phase 3 (delete now-dead constants/functions) not started.**
`dock_place_pager()`/`g_dock_plus_elem`/`g_dock_minus_elem`/
`PAGEROW:±1` are all still real and in use - phase 1 deliberately
scoped to the row-packing change only, per this doc's own staging
plan, verified independently before phase 2 begins.
