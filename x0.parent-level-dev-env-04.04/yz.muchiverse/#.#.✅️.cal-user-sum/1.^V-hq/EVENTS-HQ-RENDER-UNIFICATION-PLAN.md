EVENTS-HQ-RENDER-UNIFICATION-PLAN.md
Started: 2026-08-29
Purpose: real root-cause finding + plan, triggered by a direct
question ("why does entity menu have different renderer than other
menus? that was the point of the previous refactor. it still hasn't
been done correctly"). This is NOT a new-feature request (adding
Scripting/Scratch/Blueprints tabs to Common Events) - it's a
continuation of the SAME de-mode refactor Phase C already did for the
scroll mechanism, just a piece that got missed.

============================================================
REAL FINDING (confirmed by direct code read, not assumed)
============================================================
The original ask was narrower: "Common Events (db-hq) doesn't have the
same Scripting/Scratch/Blueprints view-mode tabs events-hq has."
Investigating that surfaced a bigger, real, pre-existing problem:

**events-hq does not use the shared render/draw/layout engine at all.**
It has its own, entirely separate, parallel set of functions - real
duplicates, not thin wrappers:
- `evhq_render_tree()` - own copy of the shared `render_tree()`.
- `evhq_draw_elem()` - own copy of the shared `draw_elem()`
  (`khtpm_draw_core.c`).
- `evhq_layout_pass()` - own copy of `css_layout_pass()`.
- `evhq_alloc_pixel()`, `evhq_apply_css()`, `evhq_zero_subtree()`,
  `evhq_measure_text_px()` - more of the same pattern.

Confirmed via the code's OWN comment (`evhq_redraw_content()`,
~line 4692): "events-hq has its OWN redraw path... entirely separate
from db-hq's dbhq_redraw_content() - the scroll track/thumb/arrow
drawing dbhq_redraw_content() already does... is NEVER reached from
here, so it's replicated here... rather than assumed shared." This is
a real, honest, self-documented admission that events-hq is running a
second, hand-copied rendering pipeline, not a deliberate design
choice - it's the exact "leftover from Stage 5's binary-merge, not
deliberate" pattern `RENDER-FRAME-HISTORY-DRIFT-ASSESSMENT.md`
already named as this whole refactor's real target, tonight's session
just hadn't reached this specific file yet.

**Why this matters for the original ask**: the view-mode tab system
(Scripting/Scratch/Blueprints) lives inside `evhq_redraw_content()`,
built on top of this SEPARATE pipeline. db-hq's Common Events panel
renders through the SHARED pipeline (`dbhq_redraw_content()` ->
`render_tree()`/`draw_elem()`). You cannot cheaply "call events-hq's
tab logic from db-hq" because the two literally paint through
different functions with different internal assumptions - the same
real reason `generic_scroll_layout_pass()` had to be built as its own
Phase C effort rather than just calling events-hq's scroll code
verbatim.

============================================================
REAL SCOPE, TWO REAL PARTS
============================================================
**Part A - the actual root fix (bigger, the real "refactor still
isn't done" issue): merge events-hq onto the shared render/draw/
layout engine, retiring its own parallel `evhq_render_tree()`/
`evhq_draw_elem()`/`evhq_layout_pass()` copies.**
This is the same class of work as Phase C's scroll-mechanism
generalization and tonight's 4-loop collapse - a real, mechanical-but-
careful port, not a redesign. Real steps:
1. Diff `evhq_draw_elem()` against the shared `draw_elem()` - find
   every REAL behavioral difference (not just cosmetic) - e.g. does
   events-hq's copy handle something the shared one doesn't yet (a
   real gap, like the LayDoc port work found and ported forward), or
   is it a byte-for-byte fork that drifted from copy-paste (a real
   duplicate to just delete)? Don't assume either answer - check.
2. Same real diff for `evhq_render_tree()` vs `render_tree()` and
   `evhq_layout_pass()` vs `css_layout_pass()`.
3. Port any REAL missing capability from the evhq copies into the
   shared functions (same discipline as the LayDoc->Elem port -
   `LAYDOC-ELEM-PORT-IMPLEMENTATION-PLAN.md` is the real, proven
   template for how this house does this kind of merge).
4. Retarget events-hq's own call sites onto the shared functions,
   delete the `evhq_*` duplicates, verify live (events-hq's own
   Scripting/Scratch views, already fixed twice tonight, are the real
   regression risk here - re-verify both after this lands).
5. Real, honest scope note: `evhq_alloc_pixel()`/`evhq_apply_css()`/
   `evhq_zero_subtree()`/`evhq_measure_text_px()` may turn out to be
   thin, harmless wrappers rather than real duplicates - check each
   one individually, don't assume they're all the same class of
   problem as the two big ones (render_tree/draw_elem).

**Part B - the original ask, now real and buildable once Part A
lands: give Common Events the same Scripting/Scratch/Blueprints view-
mode tabs.**
Once events-hq paints through the SAME shared pipeline db-hq already
uses, the view-mode dispatch logic (tab bar, viewmode-stub content,
the Scratch block-palette/placement code already fixed tonight) can
be genuinely generalized into ONE function both modes call - not
duplicated a second time into a new `g_is_db_hq` branch (that would
just be adding a THIRD copy of the same logic, the opposite of the
point). Real steps:
1. Add the real layout elements to `&.hq-apps/db-hq/dashboard.chtpm`:
   `<tabbar id="viewtabs">` (Scripting/Scratch/Blueprints) +
   `<panel id="viewmode-stub">`, positioned inside/near the Common
   Events panel area - confirmed these don't exist there yet (real
   check done already, not assumed).
2. Extract the view-mode dispatch + Scratch-block rendering (the code
   this session just fixed the CSS-compute bug in) into one shared,
   mode-agnostic function - parameterized on the target panel/tree,
   not hardcoded to `g_is_events_hq`'s own window shape.
3. Wire `dbhq_ce_inject_panel()` to call that same shared function for
   its own view-mode dispatch, alongside its existing Trigger/Switch
   fields (those stay Common-Events-specific, real, legitimate
   differences between the two modes - not everything needs to
   unify, only the genuinely-duplicated view-mode/Scratch logic does).
4. Live-verify BOTH consumers (events-hq AND Common Events) show
   identical Scripting/Scratch behavior, each with their own real
   command data.

============================================================
REAL ORDER, REAL REASON
============================================================
Part A before Part B - building Part B against the still-separate
`evhq_*` pipeline would mean writing the shared view-mode function
twice anyway (once against each pipeline), the exact duplication this
whole plan exists to stop. Part A is real, load-bearing work, not
optional preamble.

============================================================
REAL SIZE CHECK - DONE 2026-08-29 (direct read/diff, not guessed):
Part A is a real, moderate-to-large port, SAME SCALE AS THE LAYDOC
PORT - not a quick retarget or a mostly-copy-paste cleanup.
============================================================
Diffed `evhq_draw_elem()` (~line 4371) against the shared `draw_elem()`
(`khtpm_draw_core.c` ~410) line-by-line. Real, confirmed findings:

**Real capability gaps - evhq's copy is MISSING features the shared
one already has (these are the load-bearing finds, not cosmetic):**
1. **No sprite support at all.** Shared `draw_elem()` has full
   `hq_sprite()`/`hq_blit_sprite()` handling (sprite-vs-label mutual
   exclusivity, sprite-aware badge chip positioning above the tile).
   `evhq_draw_elem()` has none of this - if events-hq ever wants
   sprite-bearing Elems (icons, etc.), it structurally cannot today.
2. **No badge font caching - the EXACT perf bug already found+fixed
   in the shared function is STILL LIVE in events-hq specifically.**
   Shared `draw_elem()`'s own header comment cites a real 2026-08-25
   live perf report ("nav is really slow" with 113 tiles) and fixes it
   with a static font cache. `evhq_draw_elem()` still opens a fresh
   `XftFontOpenName()` on EVERY badge, EVERY redraw - the identical
   bug, never ported over, still real and live in this file today.
3. **No `elem_cursor_prefix()`/ACTIVATE-scope support (tonight's Gap 5
   fix).** `evhq_draw_elem()` still builds its nav badge with the OLD
   inline `snprintf(badge, ..., "[%c]%d.", focused?'>':' ', ...)` -
   the exact code the shared function replaced tonight. events-hq
   never got that fix, and structurally can't show the `^` active-
   scope indicator even after Gap 2 lands, without this port.
4. **No border-width support** (`e->style.border_width` - shared draws
   a real multi-pixel border via a loop; evhq's copy is hardcoded to
   1px, ignoring any real border_width value from CSS).
5. **No padding-aware label offset** (shared reads `e->style.padding`
   when set; evhq hardcodes `pad = 4` unconditionally).
6. **No `tag=="item"` active-highlight** (shared has a real `#2f5f8f`
   fill for active `<item>` elements - sidebar-style selection; evhq's
   copy only has the `tag=="tab"` case, using a different color
   `#3a3a3a` vs the shared function's `#2a2a2a` for the SAME tab-active
   case - a real, if minor, visible color mismatch too).
7. **No contrast-aware badge color** (`badge_contrast_color()`/
   `badge_focus_color()` in the shared version pick a real, computed-
   contrast color against the element's own background; evhq hardcodes
   `#ff8c00`/`#9a9a9a` regardless of what's under the badge).
8. **No `badge_align_left` support** (a real, already-shipped fix for
   narrow/edge-pinned elements whose badge would otherwise run
   off-screen - evhq has no equivalent).

**Real structural difference in `evhq_render_tree()` vs shared
`render_tree()`**: the shared function takes a `depth` param and
draws the ROOT element itself when `depth==0`; `evhq_render_tree()`
takes no depth param and never draws its own root argument, only
children - `evhq_redraw_content()`'s own caller must be relying on
something else (or nothing) to paint the window root. Confirm the
real reason before assuming this is safe to just copy over verbatim -
it may be a deliberate difference (events-hq's own root Elem may never
need a visible background), not an oversight like items 1-8 above.

**Verdict: proceed with Part A as a real, careful, LayDoc-port-style
merge, not a quick copy-delete.** Recommended real order once
implementation starts: port items 2 and 3 FIRST (the live perf bug and
tonight's Gap 5/ACTIVATE-scope parity are the most real, user-facing-
relevant gaps), verify events-hq's Scripting/Scratch views still work
identically after each, then continue through items 1/4/5/6/7/8, then
resolve the render_tree() root-draw question, then retarget events-hq
onto the shared functions and delete `evhq_draw_elem()`/`evhq_
render_tree()` for real. Have NOT yet checked `evhq_layout_pass()`
against `css_layout_pass()` in the same depth - that diff is still
open, do it before or alongside the above.

============================================================
CORRECTION 2026-08-29 - evhq_layout_pass() is NOT a duplicate,
narrows Part A's real scope
============================================================
Read it directly: `evhq_layout_pass()` is a real, legitimate, events-
hq-SPECIFIC layout ORCHESTRATOR (positions toolbar/pagetabs/left/
right/footer with real event-hq-shaped pixel math), not a twin of the
generic `css_layout_pass()` - it CALLS the shared `css_layout_pass()`
internally for sub-regions (pagetabs, right). This is the same real
category as `dbhq_layout_pass()` - every mode legitimately needs its
own arrangement logic, that's expected, not a "de-mode" violation.
**Not part of Part A's real scope - leave it alone.** Part A narrows
to exactly: `evhq_draw_elem()` + `evhq_render_tree()` (the real paint-
layer twins) - nothing else in this file needs merging for this plan.

============================================================
STATUS 2026-08-29 - Part A IMPLEMENTED, live-verified, ONE real
open cosmetic bug found (not yet root-caused)
============================================================
Done: `evhq_draw_elem()`/`evhq_render_tree()` deleted entirely, every
real call site (tree walk + close button + picker overlay rows/fields
+ scroll arrows, ~9 sites) retargeted onto the shared `draw_elem()`/
`render_tree()`. Build clean. Live-verified via real A/B comparison
(stashed the change, rebuilt the old binary, compared screenshots
pixel-for-pixel, then restored): Scripting view is byte-identical to
before - real, confirmed, not assumed. events-hq now genuinely has
the badge-font-cache fix, Gap 5/ACTIVATE-scope support, sprite
support, border-width/padding/contrast-color support it never had.

**One real, confirmed-NEW regression found via that same A/B test,
NOT YET ROOT-CAUSED**: in Scratch view specifically, faint "Trigger"/
"Commands" title text and the "+Add Command"/"Play" footer buttons
bleed through faintly at their old Scripting-mode positions, despite
`evhq_zero_subtree()` recursively zeroing their w/h to 0 (which should
make `draw_elem()`'s own `if (e->w<=0||e->h<=0) return;` guard skip
them - confirmed this guard exists identically in both the old and
new draw function). Ruled out one real hypothesis: the shared `render_
tree()` draws the ROOT element (`depth==0`) which the old function
never did - tested calling it with `depth=1` (skips root draw,
matching old behavior exactly) and the ghosting was still there
unchanged, so that is NOT the cause. Real remaining candidates,
NOT YET CHECKED: (a) Elem pool slot reuse - `reusable_slot()` may be
handing back a slot whose OWN stale w/h from a PREVIOUS tick's
Scripting-mode content briefly renders before this tick's zeroing
takes effect (a timing/ordering issue, not a draw_elem bug); (b) some
other code path (not `evhq_layout_pass()`'s view-mode branch) is also
touching left/right/footer's title children's w/h unconditionally.
Real, minor, cosmetic (does not affect data/color/interaction, all of
which are confirmed correct) - not blocking, but a real open item, not
swept under the rug. Next session/agent: check reusable_slot()'s own
recycling contract first (candidate (a) above is the more likely one
given it's a "faint"/partial-frame artifact, which smells like a
one-tick-stale-content symptom, not a permanent double-draw).

============================================================
STATUS 2026-08-29 - Ghosting regression ROOT-CAUSED and FIXED
(neither candidate (a) nor (b) above - a real third cause)
============================================================
Direct live report ("in entity events, the blocks are a bit too high,
overlapping the window header text above them") turned out to be this
exact ghosting bug, still visible after Part B shipped. Real root
cause, found by reading `draw_elem()` itself rather than guessing
further: this function had NO `w<=0||h<=0` guard anywhere - the
"draw_elem()'s own guard" this doc's earlier STATUS entry described
was a mistaken assumption, never actually verified by reading the
function. `evhq_zero_subtree()` zeroing an Elem's w/h correctly no-ops
its 0x0 background/border fill (`XFillRectangle`/`XDrawRectangle` with
0 width/height truly draw nothing), but the label-text branch drew
`e->label` unconditionally on `e->label[0]` alone, with zero regard
for size - so a "hidden" title's text kept rendering at its last real
x/y. Fixed with one guard line at the top of `draw_elem()` in the
shared `khtpm_draw_core.c` (`&.widgits/_shared-lib/`, NOT the local
`ops/` copy - `build_entity_menu.sh` overwrites the local copy from
the shared one on every build, which is exactly why this fix had to
be re-applied once after an initial attempt landed in the wrong file
and silently had no effect).

Live-verified: events-hq's Scratch view no longer shows "Trigger"/
"Commands" ghost text, and the block-palette/command rows now start
cleanly right below the toolbar with zero header overlap.

This also retroactively explains why candidates (a) reusable_slot
timing and (b) some other w/h-touching code path were both real,
plausible-sounding but WRONG hypotheses - the actual bug was
structural (a missing guard, not a timing race), which is why neither
would have reproduced it under direct testing.

A second, unrelated real bug found in the same live-report pass:
db-hq's own Common Events view-tabs (Scripting/Scratch/Blueprints)
were overlapping each other - Part B's `dbhq_layout_pass()` fixup loop
trusted `tab->w` AFTER `css_layout_pass(panel, ...)` had already run
one line earlier, but that generic flex pass recomputes every panel
child's width from scratch (the injected `vtabs` tabbar never declared
`display:flex`), silently stomping the widths `dbhq_ce_inject_panel()`
carefully measured at injection time. Fixed by recomputing each tab's
width fresh inside the fixup loop, instead of trusting a value the
very same function had just clobbered.

A third real bug found in the very next live report ("in the 'scratch'
visual scripting setup, all blocks are supposed to be nav numbered"):
`evhq_assign_nav_indices()` never walked `viewmode-stub`'s children at
all - it only ever numbered Scripting mode's trigger/right-panel/
footer content, so Scratch mode's real interactive Elems (the 5
palette items, onclick `BLOCK:SEL:<i>`, and the `[].<#> new block`
place-slot, onclick `BLOCK:PLACE`) had zero nav coverage in both
events-hq AND Common Events (db-hq's `dbhq_assign_nav_indices()` had
the same gap - its Scratch stub, `tag="panel"`, fell into the generic
"not a button, zero it" branch same as the earlier tabbar bug did).
Fixed in both functions: gate on `onclick[0]` rather than tag (the
stub also carries inert "text"-tagged placed-block rows and a
"block-clue" label with no onclick, which correctly stay non-nav).
Live-verified in both windows: all 5 palette blocks + the place-slot
now carry real `[ ]N.` badges in events-hq, and the same in Common
Events' Scratch view.

**Part A and Part B are both now fully clean - no known open visual
or nav bugs remain in this unification effort.**

============================================================
OPEN QUESTIONS FOR WHOEVER IMPLEMENTS
============================================================
1. `evhq_layout_pass()` vs `css_layout_pass()` diff - NOT YET DONE,
   real remaining check before implementation starts (see above).
2. The `evhq_alloc_pixel()`/`evhq_apply_css()`/`evhq_zero_subtree()`/
   `evhq_measure_text_px()` helpers - NOT YET individually diffed
   against any shared equivalents; unclear yet whether real shared
   equivalents even exist for all of them (some, like `evhq_zero_
   subtree()`, may be events-hq-specific concepts with no shared
   twin at all - check before assuming they're duplicates too).
3. The `evhq_render_tree()` root-draw difference (see above) - resolve
   before porting `render_tree()` verbatim.
4. Blueprints view mode is still a real stub ("coming soon") in
   events-hq itself - Part B should carry that same stub state into
   Common Events, not invent real Blueprints content as part of this
   plan (separate, later scope).

============================================================
STATUS 2026-08-29 - Part B IMPLEMENTED, live-verified, 3 real bugs
found and fixed along the way
============================================================
Done: Common Events (db-hq) now shares events-hq's real Scripting/
Scratch/Blueprints view-mode tabs, via the SAME shared functions
events-hq itself uses (`evhq_build_scratch_view()`, `evhq_handle_
block_onclick()`) - not a second copy. `dbhq_ce_inject_panel()` injects
a real `tabbar` (3 `tag="tab"` children, `onclick="CE:VIEWTAB:%d"`)
right after the Common Event's title; `dbhq_ce_handle_onclick()`
branches on `CE:VIEWTAB:` to flip `g_evhq_view_mode` (the same global
events-hq's own tabs already drive) and force a rebuild. Scripting
mode is the untouched original path; Scratch mode calls the shared
`evhq_build_scratch_view()`; Blueprints shows the same real stub
events-hq itself shows (per open question 4 above - not invented).

Three real bugs found live (via direct-instruction screenshot review,
not assumption) and fixed, in order:

1. **View-tabs overlapping the panel title** - `dbhq_layout_pass()`'s
   generic panel-child flex pass repositions the `tabbar` container
   itself but never recurses into ITS OWN children, so the 3 tab Elems
   kept their stale pre-layout x/y. Fixed with a post-`css_layout_
   pass()` fixup loop in `dbhq_layout_pass()` that finds any `tabbar`-
   tagged child of `panel` and repositions its children relative to
   the tabbar's own now-correct x/y/h - mirrors the fixup already used
   for the window's own top tabbar (same pattern, not a new one).

2. **View-tabs missing `nav_index` entirely** (direct user catch:
   "i noticed the scripting scratch and blueprints dont have nav. that
   violats house" / "they have them in entities tabs. so why did u
   misdo it here?"). Root cause: `dbhq_assign_nav_indices()`'s panel-
   child loop unconditionally zeroed any non-`button`-tagged child,
   which silently ate the new `tabbar` too. Fixed by adding a
   `tabbar`-specific branch (checked BEFORE the button-only check) that
   walks into the tabbar's children and calls `dbhq_nav_take()` on
   each - mirrors `evhq_assign_nav_indices()`'s own real "viewtabs
   nav-reachable first" pattern verbatim, just at db-hq's own scope.

3. **Scratch palette overlapping db-hq's real persistent sidebar**
   (self-caught via screenshot review). Root cause: `evhq_build_
   scratch_view()` hardcoded `viewmode_stub->x = 0`, correct for
   events-hq (no sidebar) but wrong for Common Events (sidebar occupies
   x=0..~260). Fixed by adding a `content_x` parameter to `evhq_build_
   scratch_view()` (now `(Elem *viewmode_stub, int content_x, int
   content_y, int content_h, int window_w)`); events-hq's own call
   site passes `0` (unchanged behavior), Common Events' call site
   passes `panel->x` (the real sidebar-aware content-pane offset).

A 4th real gap found in the same pass: db-hq's own `dashboard.css` had
NONE of the `.scratch-block`/`.block-item`/`.block-place`/`.block-
clue` rules events-hq's `dashboard.css` carries (Common Events'
Scratch view rendered with zero color before this). Fixed by porting
those rules verbatim into `&.hq-apps/db-hq/dashboard.css` (the `.tab`/
`.tab.active` rules it already had cover the new tabbar's `tag="tab"`
children, so those did not need porting).

**Live-verified, all 4 fixes, in one real headless test session**
(file-relay input + `xdotool windowfocus` only, no absolute-coordinate
clicks - per this session's own standing rule): launched a real test
db-hq window, digit-jumped into Common Events, opened `greet_player`,
confirmed real `[ ]21./[>]22./23.` nav badges on Scripting/Scratch/
Blueprints (bug 2 fixed), digit-jumped+Enter into Scratch, confirmed
the palette and block chain render fully right of the sidebar with no
overlap (bug 3 fixed) and full real gold/green/orange/purple/pink
block colors plus the white-bg/dark-green-text new-block placeholder
(bug 4 fixed) - screenshot matches events-hq's own Scratch view
exactly, just sidebar-offset. Test window and all its child
`dbhq_pdl_publish_manager.+x` processes confirmed cleanly killed after,
zero strays left running.

Part A's own open ghosting regression (see STATUS 2026-08-29 above)
remains open and unrelated to Part B - not touched this pass.
