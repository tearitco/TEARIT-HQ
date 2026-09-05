# Renderer Modularity + Performance Audit (2026-09-04)

**Scope:** `*.monads/*.livedesk-taskbar/ops/khtpm_core_render.c` (~12,200
lines) + the three shared-lib authoritative files
(`&.widgits/_shared-lib/khtpm_render_core.c`/`khtpm_draw_core.c`/
`khtpm_css_parser.c`). Measures compliance against `CENTROID_GOLD_STD.md`
§1/§3 (one real Elem tree, no business logic in the shared file, no new
`g_is_<project>` globals) plus a plain readability/reuse bar, and audits
for real, cited performance waste. Cross-references
`XHTPM-PARSER-REFERENCE.md` for the documented feature surface.

**Extends, does not repeat,** the same-day Rev 15 audit (see
`handoff-2026-09-04-master.md` Rev 15, and commit `e1247d5a` which
already stripped the `g_is_stats_hq`/`g_is_palettes`/`g_is_bookmarks`/
`g_is_events_hq` dead flags that pass found). This pass does not
re-report those.

**Explicit correction to this audit's own working assumptions, made
mid-pass by direct instruction**: "known architecturally correct" (e.g.
`tp_main()` being folded into one binary rather than linked, per the
house's own no-separate-binaries rule) is not the same claim as
"well-factored, readable, or maximally reusable internally," and this
audit does not treat the two as equivalent — a block can be
structurally the right call (one binary, not two) and still contain
real, fixable redundancy/inefficiency inside it. Nothing below gets a
free pass for being "intentional" at the macro level; macro-level and
micro-level findings are reported separately.

---

## Part 1: Modularity / Reuse Compliance

### 1.1 Remaining `g_is_*` globals

Only two left in the whole file (grep-confirmed exhaustive):

- **`g_is_events_hq`** (`khtpm_core_render.c:2289`) — already `static
  const int ... = 0`, dead by construction, already documented by its
  own comment ("events-hq C deleted 2026-09-03"). Not new, not
  re-reported as a finding.
- **`g_is_cursword`** (`khtpm_core_render.c:6759`) — **a real, open
  compliance question, not a clean pass.** Set by a one-time identity
  check (`khtpm_core_render.c:9756`: `g_is_cursword =
  (strcmp(basename(pkgcopy), "cursword") == 0)`) and checked at **24
  separate sites** inside `tp_main()` (lines 8040, 8262, 9769, 9856,
  9922-9923, 10020, 10187, 10299, 11233, 11272, 11280, 11449, 11493,
  11605, 11669-11670, 11695, 11785, 11799, 11845, 11943-11944) to
  special-case one specific desktop entity's own window shape/behavior
  (a taller "log" window, its own click-to-place mode, its own 3D-vs-2D
  frame logic). This is the *exact same shape* CENTROID_GOLD_STD.md §3
  rule 7 condemns for top-level window modes — a project-identity
  `strcmp` gating ~24 scattered dispatch points — just nested one level
  deeper, inside `tp_main()`'s own tile-mode dispatch instead of
  `main()`'s own top-level one. `tp_main()` itself being folded into
  one binary is the correct call per the house's own "no separate
  linked binaries" rule; that does not make a per-entity `strcmp`
  branch *inside* it compliant by association — the same rule 7
  reasoning applies recursively. **Not fixed here** (audit only), but
  this is a real candidate for the same treatment rule 7 already
  prescribes for top-level modes: a data-driven property (e.g. a
  `log_window=1` flag the entity's own `.pdl`/manager already
  publishes) read generically, rather than a hardcoded project name
  living in the shared renderer.

### 1.2 `dbhq_load_actors()` — already gone (BUG-LOG.md's own Open entry is stale)

`04-bugs/BUG-LOG.md`'s Open section currently reads: *"`khtpm_core_
render.c`'s `dbhq_load_actors()` loads real PDL data inline in the
shared parser/renderer file... An audit pass for sibling inline
loaders... has not been done."* **Grepped exhaustively for
`dbhq_load_actors` and bare `load_actors` in the current file: zero
hits, in code or comments.** The function no longer exists — it was
already removed in an earlier cleanup pass (consistent with the
`handoff-2026-09-04-master.md` Rev 11/12 "dbhq_* C deleted" history),
and the bug-log entry describing it was never updated to reflect that.
**No sibling inline loaders found either** (`dbhq_load_classes`/
`_skills`/`_items` or similar — grepped, none exist). Recommend
`04-bugs/BUG-LOG.md` mark this entry resolved/stale rather than open
(not done here — audit only, per this pass's own scope).

### 1.3 Copy-pasted logic inside `tp_main()`'s own tile mode (real redundancy, found under the corrected higher bar)

Two window-shape-mask builders exist ~50 lines apart doing the
functionally same job (build a 1-bit `XShapeCombineMask` mask from a
per-pixel test) with near-identical loop *structure*, driven by two
different pixel sources, and NEITHER shares a helper with the other:

- `khtpm_core_render.c` ~7953-7967 (sprite-alpha-driven): nested
  `for (y) for (x)`, nearest-neighbor-scales `(x,y)` down to sprite
  resolution (`sx = (x * g_sprite_res) / WIN_PX`, clamped), tests the
  alpha byte, `XFillRectangle(mask, x, y, 1, 1)` per opaque pixel.
- `khtpm_core_render.c` ~8006-8016 (ARGB-image-bg-diff-driven): nested
  `for (y) for (x)`, `XGetPixel(img, x, y)`, compares against a known
  background color, `XFillRectangle(mask, x, y, 1, 1)` per differing
  pixel.

Both exist purely to answer "which pixels are opaque/foreground,"
differing only in *how* they read a pixel — a natural `build_shape_
mask(Display*, Pixmap mask, GC mask_gc, int w, int h, int (*is_opaque)
(int x, int y, void *ctx), void *ctx)` helper (or even just a shared
inner-loop macro) would collapse both to one real implementation,
matching the exact spirit of `history_dir()`/`frame_changed_path()`
already being one shared helper (Rev 15's own finding) rather than two.

**A third, separate duplication inside the same neighborhood**: the
nearest-neighbor sprite-scaling index math (`sy = (y * g_sprite_res) /
WIN_PX`, clamped; same for `sx`) is repeated verbatim between the
sprite-alpha mask builder above (~7954-7957) and `draw_sprite_rgb()`
(`khtpm_core_render.c` ~8129-8132). A shared `sprite_scale_index(int
screen_px, int sprite_res, int win_px)` (or similar) inline helper
would remove the second copy.

### 1.4 `g_default_persistent` / `g_default_has_sidebar_panel` — re-checked against the "clearest expression" bar, not just "not a per-app hack"

Both are computed once from real, generic structural properties (does
the parsed tree contain both `<sidebar>` and `<panel>`; does the
window's own `class=` include `database-window`/`palettes-pal`) —
confirmed via `khtpm_core_render.c:2560`/`:2567` declarations and their
16/4 real call sites respectively (grep-counted). These are genuinely
the most direct way to express "this window has record-browser
structure" / "this window shouldn't auto-close" — not project-identity
checks, and no clearer alternative expression was found on inspection.
One minor, real readability note: `g_default_persistent`'s own name
doesn't self-document what it actually gates (dispatch's quit-gate +
the swatch-grid widen decision) without reading its declaration
comment — a name closer to its effect (e.g. `g_default_no_autoclose`)
would read clearer cold, though this is a naming nit, not a structural
finding.

### 1.5 Hand-built markup-string construction (rule 1/3 violation check)

Grepped `khtpm_core_render.c` + all three shared-lib files for
`sprintf`/`snprintf` building `<item`/`<window`/`<page`-shaped literal
strings (the Stage-2b-style violation CENTROID_GOLD_STD.md's own §2
history section condemns outright). **Zero hits.** Every window's
content still goes through a real `.chtpm`/`.xhtpm` parse — clean.

---

## Part 2: Performance

### 2.1 `kh_draw_canvas()`'s per-pixel `XPutPixel` loop — already known, cited for completeness only

`khtpm_draw_core.c` ~569-576: a `for (y) for (x) XPutPixel(...)` loop
building the canvas `XImage` client-side, one call per pixel, instead
of a single bulk memory write into the `XImage`'s own backing buffer
(the data is already a raw RGBA byte array — a direct `memcpy`-with-
channel-swizzle into `c_img->data` would do the same job without a
function-call-per-pixel). Already known from the earlier canvas-
blanking investigation this session (`pc-hq-bugs.md`'s "Canvas render
pipeline" section) — not re-investigated in depth, cited for
completeness since it's the same class of issue as 2.2 below.

### 2.2 Tile-mode's window-shape-mask builders do `XFillRectangle` PER PIXEL — new finding, likely more impactful than 2.1

The same two functions cited in 1.3 above (`khtpm_core_render.c`
~7953-7967 and ~8006-8016) each issue one `XFillRectangle(dpy, mask,
mask_gc, x, y, 1, 1)` **X protocol call per opaque/foreground pixel**
across a `WIN_PX × WIN_PX` window (every entity tile on the desktop
that isn't a plain circle goes through one of these on every shape
recompute) — this is a real, likely-worse-than-2.1 performance cost:
each 1×1 fill is a full round-trip-shaped protocol request (even if
buffered client-side, it's still `WIN_PX²` individual GC operations
queued and processed one at a time by the X server), where a single
client-side 1-bit bitmap built in local memory and pushed once via
`XPutImage`/`XCreateBitmapFromData` would do the identical job in one
server round trip. Both functions already build the mask into a
`Pixmap` via the X server anyway — the fix is building the bitmap
bytes locally first, matching the same "build once client-side, blit
once" principle `kh_draw_canvas()`'s own `XImage` cache already uses
correctly for its *content* (just not, per 2.1, for populating that
cache).

### 2.3 Tile mode has its own, separately-defined poll interval — inconsistent, not necessarily wrong

`khtpm_core_render.c:6724`: `#define POLL_INTERVAL_USEC 300000` (300ms),
used at `khtpm_core_render.c:10206`'s own `struct timeval tv = { 0,
POLL_INTERVAL_USEC }` inside `tp_main()`'s event loop — a completely
separate tick-rate policy from the generic default-mode scheme
(`hq_run_event_loop()`'s own `g_has_canvas ? 33ms : 150ms`,
`khtpm_core_render.c:6530`). Not flagged as definitely wrong — tile-
mode entities plausibly need less frequent polling than an active
canvas or an interactive menu — but it's a real, undocumented
divergence: nothing states WHY 300ms is tile mode's own right number
rather than reusing the generic 150ms default, and two independently-
tuned interval constants for conceptually the same "how often does this
window's own idle tick need to run" decision is exactly the kind of
drift `CENTROID_GOLD_STD.md` §3 rule 3 ("parse+layout+CSS run exactly
once... shared, unmodified") argues against for the rest of the
pipeline. Worth a real decision (keep two constants with a documented
reason, or unify) rather than leaving it as historical accident.

### 2.4 No per-tick `fork`/`system()` calls found in `khtpm_core_render.c` itself

Grepped all 10 `system()` call sites in the file: every one is inside
a click/dispatch handler (one-shot, user-triggered), none live inside
the main event-loop's own per-tick path. The double-process-spawn-
per-frame pattern found earlier this session (board-viewer's own
`bv_render_3d`/`bv_compose_frame` pal-script driver, see `pc-hq-
bugs.md`) is entirely external to this file — `khtpm_core_render.c`
itself does not have an equivalent per-tick fork/exec pattern anywhere.
Clean.

---

## Part 3: Ops/pal/events convertibility — could large inline blocks move OUT of the shared renderer entirely?

Two real, house-proven precedents for "convert an inline C mode to a
static template + real external process(es)" exist and were checked
directly as evidence, not cited from memory:

- **events-hq's real conversion** (`&.widgits/events-hq/`): a static
  `events-hq.xhtpm` + `pieces/dashboard.chtpm`/`picker.chtpm`, a real
  compiled manager (`ops/khtpm_events_hq_manager.c`, polls/owns
  business state), a thin projector (`ops/evhq_projector.c`, publishes
  plain `state/ui.txt`), and a dispatch script (`ops/evhq_action.sh`)
  — the shared renderer's own generic default-mode path draws it, zero
  events-hq-specific C left in `khtpm_core_render.c` (confirmed: `grep
  g_is_events_hq` only finds the dead const-0 flag, no live branches).
- **pchq-board's own conversion, THIS session** (`@.apps/piececraft-hq/
  pchq-board.xhtpm` + `ops/pchq_board_projector.c` + `ops/pchq_board_
  action.sh`): proves the SAME pattern extends even to a live,
  continuously-updating 3D canvas view (harder than a static
  dashboard) — the generic `<canvas>` primitive built this session
  reads a raw-RGBA framebuffer another process owns, at 30fps, with no
  render logic in the shared file at all.

### `tp_main()` (TILE MODE, ~2240 lines) — **convertible with two real, named prerequisites, not a clean "convertible now"**

What it actually does per real desktop entity (one process per tile):
sprite/emoji rendering at `WIN_PX×WIN_PX`, click-to-place and drag
repositioning, a non-rectangular window shape via `XShapeCombineMask`
(§1.3/§2.2's own subject), 2D/3D toggle, and `cursword`'s own special
taller "log" window variant (§1.1).

**Verdict: convertible with real prerequisite work.** The pchq-board
precedent is *harder* in one dimension (live 30fps 3D content) and
already proven — that rules out "structurally can't be done" as an
excuse. Two concrete, real gaps stand between here and there, neither
of which is just inertia:

1. **No generic "shaped window from live pixel data" capability exists
   yet.** `kh_draw_canvas()` blits a rectangle; it has no equivalent of
   tile mode's own `XShapeCombineMask`-from-alpha-or-bg-diff logic
   (§1.3/§2.2). Converting `tp_main()` cleanly means either building
   this as a REAL generic renderer capability (a `<canvas shape="1"/>`
   attribute, say, that also computes and applies a shape mask from
   the same alpha data it already reads) or accepting a square window
   for converted tiles — a real design decision, not a rewrite detail.
2. **No proof yet that the generic default-mode path handles smooth,
   continuous drag-repositioning at the same responsiveness tile mode
   achieves natively.** Every generic-mode window this session touched
   (pchq-board included) is click-driven, not continuously dragged —
   this specific interaction pattern (update real window position on
   every `MotionNotify` while a button is held) hasn't been proven on
   the shared path yet. This would need a real, live-tested build-out
   before conversion, not an assumption that "it'll just work."

**Not a real blocker**: `g_is_cursword`'s own special-casing (§1.1).
Once the above two prerequisites exist, `cursword`'s "taller log
window" behavior is trivially data-driven (a `log_mode=1` key its own
manager/`.pdl` already could publish, read the same generic way
`interact_class`/`no_session` etc. already are for pchq-board) — this
was never a structural obstacle, just something that would naturally
fall out of doing the conversion at all.

**What the split would look like, concretely** (matching events-hq's
own real shape): a real, compiled `tile_manager.c` per entity kind (or
one generic one, parameterized by the entity's own package dir, same
as `tp_main()` already is) owns click-hit-testing, drag-state, and the
2D/3D-mode decision, publishing `x=`/`y=`/`sprite_path=`/`mode=`/
`log_mode=` to a plain state file; a thin projector (or the manager
itself, events-hq's own manager+projector are already two separate
small processes for exactly this separation of concerns) writes
`state/ui.txt`; a static `tile.xhtpm` + `<canvas shape="...">` (once
built) replaces the current hand-rolled Xlib window-creation/shape/
present code entirely. No `.pal`/`prisc+x` step is obviously needed
here (unlike board-viewer, tile mode has no separate "game engine"
process generating the content — the manager itself IS the source of
truth), so the events-hq shape (manager + projector + static template,
no PAL interpreter) is the closer precedent of the two, not
board-viewer's.

### Nothing else audited in Part 1/2 is a real conversion candidate

The two shape-mask-builder functions (§1.3/§2.2) are implementation
details WITHIN `tp_main()`'s own eventual conversion, not separately
convertible. `g_default_persistent`/`g_default_has_sidebar_panel`
(§1.4) are generic dispatch-time flags already on the correct shared
path — there's nothing to convert, they're not inline business logic,
they're structural classification of an already-parsed tree.

---

## Recommended next steps (ordered by real risk/value)

1. **Correct `04-bugs/BUG-LOG.md`'s stale `dbhq_load_actors()` entry**
   (§1.2) — lowest effort, purely a documentation accuracy fix, and
   currently misleads anyone who reads the open-bugs list into
   thinking there's live code to audit/fix that no longer exists.
2. **Fix the two `XFillRectangle`-per-pixel shape-mask builders**
   (§2.2) — highest real perf value found in this pass: every non-
   circular entity tile on the desktop pays this cost on every shape
   recompute, and the fix (build a local bitmap, blit once) is
   well-precedented by `kh_draw_canvas()`'s own existing `XImage`
   cache pattern in the same file.
3. **Extract the shared shape-mask-builder helper** (§1.3) while fixing
   #2 above — the redundancy and the perf fix live in the exact same
   two functions, so this is naturally one real changeset, not two.
4. **Decide g_is_cursword's fate** (§1.1) — either document a real,
   specific reason tile-mode entity special-cases are exempt from rule
   7's own reasoning (if one exists and simply hasn't been written
   down), or treat it as a real migration candidate the same way rule
   7 already prescribes for top-level `g_is_<project>` modes. This is
   a judgment call for whoever owns the standard, not something to
   silently leave ambiguous.
5. **Reconcile the two poll-interval constants** (§2.3) — lower
   urgency, but a one-line decision (keep both with a documented
   reason, or collapse to one) closes a real, if minor, drift.
6. **`tp_main()` full ops/manager conversion** (Part 3) — the largest,
   highest-effort item here by far (a real prerequisite build-out, not
   a quick fix), but the one with the biggest architectural payoff:
   removing the last major inline mode from the shared renderer
   entirely, matching events-hq's own already-completed precedent.
   Sequence before starting: build the two named prerequisites (a
   generic shaped-canvas capability, proven continuous-drag support on
   the generic path) as their own small, independently-verifiable
   steps — attempting the full conversion in one pass would repeat
   this session's own "the fix was bigger than it looked" pattern
   (the override_redirect incident) at much larger scale.

---

## Document Info

- **Author:** Claude (audit pass, 2026-09-04), extending Rev 15's own
  same-day audit
- **Scope:** `khtpm_core_render.c` + the three shared-lib authoritative
  files, measured against `CENTROID_GOLD_STD.md` + a plain readability/
  reuse bar (not limited to the letter of the no-per-app-C rule)
- **Status:** Audit only — nothing in this document was fixed as part
  of writing it
