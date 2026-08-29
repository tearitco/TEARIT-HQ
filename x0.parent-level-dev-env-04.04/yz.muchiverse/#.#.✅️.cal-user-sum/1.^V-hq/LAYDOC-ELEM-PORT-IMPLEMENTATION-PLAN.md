LAYDOC-ELEM-PORT-IMPLEMENTATION-PLAN.md
Started: 2026-08-28
Purpose: concrete, code-level implementation plan for porting LayDoc's
real capabilities into the Elem/CSS engine, per the design already
agreed and posted in `GROK-RENDER-INPUT-REFACTOR-HANDOFF.md` (search
that doc for "LayDoc→Elem port DESIGN, part 1 of 2" and "part 2 of 2"
for the full reasoning behind each choice below - this file is the
"now turn it into real signatures and insertion points" follow-up,
not a re-derivation). Written for Grok/whoever implements, and for
the user to review before any code lands. No code has been written
yet - this is still a plan.

============================================================
REAL, CURRENT STATE (verified by direct read just now, not assumed)
============================================================
`Elem` struct - `&.widgits/_shared-lib/khtpm_render_core.c` lines
73-131:
    typedef struct Elem {
        char tag[32];
        char id[64];
        char classes[CSS_MAX_CLASSES][32];
        int n_classes;
        char label[256];
        char onclick[1536];
        char sprite[256];
        int active;      /* tab active / sidebar item selected */
        int nav_index;   /* wraith-alpha-standard 1-based digit-jump number */
        int badge_align_left;
        struct Elem *children[MAX_CHILDREN];
        int n_children;
        struct Elem *parent;
        int x, y, w, h;
        CssStyle style;
    } Elem;

Existing shared functions in the same file: `hit_test()` (~137),
`find_by_tag()` (~149), `find_by_id()` (~163), `css_layout_pass()`
(~218).

Badge/cursor draw today - `khtpm_draw_core.c` `draw_elem()`:
- Line 430: `if (e->nav_index > 0 && e->nav_index == g_focus_nav)` -
  draws the focus-ring border.
- Lines 444-450: builds `nav_badge[16]` inline via
  `snprintf(nav_badge, sizeof(nav_badge), "[%c]%d.", focused ? '>' :
  ' ', e->nav_index)` - this is the exact code Gap 5's
  `elem_cursor_prefix()` replaces.
- Lines 517-525+: badge position/paint (above-label chip, etc).

Nav-index ASSIGNMENT today is per-mode, NOT generic - confirmed real
duplication (same class of thing Phase C fixed for scroll, not yet
fixed for nav assignment):
- `dbhq_assign_nav_indices(Elem *window)` - `khtpm_entity_menu_
  render.c` ~1650, called at ~2019/2401/2442/6658.
- `evhq_assign_nav_indices(Elem *window)` - ~3403, called at
  ~3798/4154/6659.
- `chai_assign_nav_indices(Elem *window)` - ~5479, called at
  ~6053/6546/6660.
- Dispatched from one real switch at ~6658-6660 (`layout_pass()`'s
  own per-mode branch).

This matters for the plan below: Gap 2's `elem_is_navigable()` gate
has to be called from INSIDE all three of these functions until/
unless the loop-collapse work (separate, queued task) also
generalizes nav-index assignment into one function - don't wait for
that collapse to land first, just don't forget to wire the gate into
all three existing call sites.

============================================================
GAP 3 - flat-array serialization helper (independent, do first)
============================================================
**File:** `khtpm_render_core.c`, near `find_by_id()`/`find_by_tag()`.

**New function:**
    /* Walks the pointer tree once in real preorder (non-title-first,
     * title-deferred - same order render_tree() already uses) and
     * fills `out[]` with flat index records. Returns element count,
     * or -1 if `cap` was too small (caller's tree has more elements
     * than the buffer - do not silently truncate). Read-only: never
     * mutates the tree. */
    typedef struct {
        int index;          /* 0-based position in this walk */
        int parent_index;   /* -1 for root, else index of parent in
                              * THIS SAME walk */
        Elem *elem;          /* live pointer, for the caller's own use -
                              * NOT serialized to any file, index/
                              * parent_index are what gets written */
    } ElemFlatEntry;

    int elem_flatten(Elem *root, ElemFlatEntry *out, int cap);

**Why this shape:** matches `dbhq_serialize_frame_subtree()`'s own
real walk order exactly (so Phase 2's palettes frame-file code can be
refactored to call this instead of duplicating its own recursion -
a real simplification, not just new code). Read-only and pointer-
preserving means it adds zero risk to the 3 live apps' actual
navigation, which stays 100% pointer-based.

**Verification:** call on palettes' real tree, diff the flat output's
parent/child relationships against what `dbhq_serialize_frame_subtree()`
already writes to `palettes_frame.txt` today - they must produce the
identical tree shape (same elements, same order) before this is
considered proven, since it's meant to become the SAME serialization
these two call sites use.

============================================================
GAP 5 - elem_cursor_prefix() (independent, do first)
============================================================
**File:** `khtpm_render_core.c` (logic) - called from
`khtpm_draw_core.c`'s `draw_elem()` (draw site).

**New function, real port of `lay_cursor_prefix()`
(`khtpm_strip_layout.c` ~568-576):**
    /* Returns a static (or caller-provided buf) prefix string for e's
     * current nav state, computed fresh - never stored back into
     * e->label. Mirrors lay_cursor_prefix()'s 3-way: "[^]" active,
     * "[>]" focused+navigable, "[ ]" otherwise. `focus_nav` is
     * whatever g_focus_nav the caller currently has (each mode owns
     * its own copy today - pass it in, don't reach for a global from
     * inside render_core.c, which stays mode-agnostic). */
    void elem_cursor_prefix(const Elem *e, int focus_nav, char *out,
                             size_t outsz);

**Insertion point:** `khtpm_draw_core.c` lines 444-450, replacing the
inline `snprintf(nav_badge, ..., "[%c]%d.", ...)` with:
    char prefix[8];
    elem_cursor_prefix(e, g_focus_nav, prefix, sizeof(prefix));
    snprintf(nav_badge, sizeof(nav_badge), "%s%d.", prefix, e->nav_index);

**Real behavior change to confirm, not assume**: today's badge only
has 2 states (focused `>` / not-focused ` `). Adding the LayDoc-style
3rd state (`^` = active/open, distinct from `>` = cursor-here) means
`elem_cursor_prefix()` needs SOME notion of "active scope," which
does not exist on Elem yet - this function's 3rd branch is a no-op
(never returns `^`) UNTIL Gap 2 lands. Ship Gap 5 now with only 2 real
states wired (matching today's exact behavior, zero regression), and
wire the 3rd (`^`) branch when Gap 2's `g_elem_active_scope_root`
exists. Say this explicitly in the commit/execution record so nobody
mistakes "2-state version shipped" for "Gap 5 fully done."

============================================================
GAP 2 - ACTIVATE scope + BACK (the real architectural piece - do
after 3 & 5, before 6)
============================================================
**File:** `khtpm_entity_menu_render.c` (per-mode state, since each
mode is its own process/tree) - NOT `khtpm_render_core.c` itself,
since scope is runtime state, not a structural Elem field. (If a
future need arises to share this logic verbatim across modes, lift
the walk functions into render_core.c then - don't do it speculatively
now with only one real consumer.)

**New per-mode global (pick db-hq as the pilot, same discipline as
every other phase tonight):**
    static Elem *g_dbhq_active_scope_root = NULL; /* NULL = no scope open */

**New functions (real ports of `lay_activate`/`lay_back`,
`khtpm_strip_layout.c` ~520-548):**
    /* Called when hit-test resolves to an Elem whose onclick starts
     * with "ACTIVATE" (exact match "ACTIVATE" or prefix "ACTIVATE:").
     * Opens that Elem's own subtree as the active scope. */
    static void dbhq_activate_scope(Elem *e) {
        g_dbhq_active_scope_root = e;
    }
    /* Called on onclick=="BACK". Walks e's ANCESTORS (via e->parent)
     * looking for the nearest one whose own onclick was itself an
     * ACTIVATE marker, and makes THAT the new scope root (or NULL if
     * none found - fully closed). */
    static void dbhq_back_scope(void) {
        Elem *p = g_dbhq_active_scope_root ? g_dbhq_active_scope_root->parent : NULL;
        while (p && strncmp(p->onclick, "ACTIVATE", 8) != 0) p = p->parent;
        g_dbhq_active_scope_root = p;
    }
    /* Real port of lay_is_navigable (~480-518): while a scope is
     * open, only its own descendants (checked via parent-walk, since
     * Elem has no parent_index to compare integers) are navigable;
     * with no scope open, everything outside any CLOSED ACTIVATE
     * marker's subtree is navigable. */
    static int dbhq_elem_is_navigable(Elem *e) {
        if (!g_dbhq_active_scope_root) return 1; /* no scope open: default open */
        Elem *p = e;
        while (p) { if (p == g_dbhq_active_scope_root) return 1; p = p->parent; }
        return 0;
    }

**Wiring into existing dispatch**: `dbhq_activate_elem()` (the
existing onclick dispatcher, already real and in place per this
session's earlier bookmarks/palettes work) gets two new prefix checks
before its existing fallthrough: `onclick == "ACTIVATE"` (or prefix)
-> `dbhq_activate_scope(e)`; `onclick == "BACK"` -> `dbhq_back_scope()`.
**CORRECTION 2026-08-28, post-implementation**: Grok's real build used
`onclick == "DEACTIVATE"` instead of `"BACK"` for this - a pre-existing,
unrelated `"BACK"` string was already in use by the popup/entity-menu's
own page-stack navigation (`switch_page()`-based, predates this gap),
and using the same string here would have collided the moment either
dispatch function is ever generalized. `"BACK"` below in this doc means
`"DEACTIVATE"` in the real, shipped code - not corrected inline below to
preserve this plan's own original reasoning trail; treat `"DEACTIVATE"`
as authoritative wherever this doc says `"BACK"` for Gap 2 specifically.

**Wiring into nav assignment**: `dbhq_assign_nav_indices()` (~1650)
gets one new guard added to whatever loop currently walks the tree
assigning sequential numbers - skip (don't assign a number to) any
Elem where `dbhq_elem_is_navigable(e)` returns 0. Real, minimal diff -
this function already walks the whole tree, it just needs one `if`
added before it increments the counter.

**Real live verification required** (per this doc's own standing
rule - a compile-clean claim is not a done claim): open a real
ACTIVATE-marked popup/menu in db-hq (or add one disposable test
button with `onclick="ACTIVATE"` wrapping a couple of test children
if nothing real exists yet), confirm digit-jump only reaches those
children while open, confirm a "BACK" click restores the parent
scope's own numbering, confirm elements OUTSIDE the popup are not
reachable by digit while it's open.

============================================================
GAP 6 - cli_io tag (rides on Gap 2, do immediately after)
============================================================
**File:** `khtpm_render_core.c` (the tag-check is structural/generic)
+ `khtpm_entity_menu_render.c` (per-mode nav-assignment call site,
same 3 functions as Gap 2).

**New function:**
    /* Real port of lay_is_navigable's cli_io special-case
     * (khtpm_strip_layout.c ~485-494): a cli_io-tagged element is
     * navigable ONLY when it IS the current active scope root itself
     * (not merely a descendant of one) - reuses Gap 2's scope-root
     * pointer directly, no new index field needed. */
    static int dbhq_cli_io_navigable(Elem *e) {
        if (strcmp(e->tag, "cli_io") != 0) return 1; /* not cli_io: no special rule */
        return (e == g_dbhq_active_scope_root);
    }

**Wiring:** `dbhq_elem_is_navigable()` from Gap 2 gets ANDed with this:
    static int dbhq_elem_is_navigable(Elem *e) {
        if (!dbhq_cli_io_navigable(e)) return 0;
        /* ...existing Gap-2 body... */
    }

**Note:** `cli_io` is not yet a tag anything actually emits - this
lands the RULE, ready for whichever mode first adds a real
`<cli_io>` element (chat-hai's composer is the obvious real future
consumer, currently bespoke `chai_composer_*` C per the original gap
citation). Do not force chat-hai to adopt `cli_io` in this same pass -
that's a separate, later migration of chat-hai's own composer code,
out of scope here.

============================================================
GAP 7 - header+footer synthetic nav-only root (independent, taskbar
side - NOT khtpm_entity_menu_render.c)
============================================================
**File:** `khtpm_strip_parser.c` (taskbar's own consumer file, not
the shared Elem engine - this gap is LayDoc-side plumbing, not a
port INTO Elem, since it only matters once/if taskbar itself sits on
top of whichever tree model wins).

**Deferred pending Gap 2/6 landing and initial verification** - real
sequencing reason: gap 7's own "one continuous nav_index walk across
two trees" needs a working single-tree nav-assignment pattern to copy
first (Gap 2's work IS that pattern, just per-window instead of
per-document). Revisit this gap's concrete signatures once Gap 2 is
live and verified - premature to nail down exact function names
against a still-LayDoc-shaped file before the Elem-side pattern it's
copying has been proven once.

**CORRECTION 2026-08-28 (do not implement as written):** Gap 7 must
**not** grow an Elem synthetic header+footer root. The strip already
has `g_nav_focus` + `unified_apply` / `unified_step` in
`khtpm_strip_parser.c`. Leave that path; do not port a second tree
into Elem for this gap. Source: Sonnet ACK of Grok 1–11 in
`GROK-RENDER-INPUT-REFACTOR-HANDOFF.md` (Gap 7 already on the strip).

============================================================
GAP 8 - elem_inject_loop() helper (independent, lowest priority,
pure cleanup)
============================================================
**File:** `khtpm_render_core.c`.

**New function:**
    typedef Elem *(*ElemFactory)(void *row, void *ctx);
    /* Appends n Elems (one per row, built by fn) as children of
     * parent, handling n_children/parent-pointer bookkeeping. Does
     * NOT clear parent's existing children first - caller's
     * responsibility (matches today's per-mode injectors, which
     * already have their own clear-then-rebuild convention - don't
     * change that behavior implicitly here). */
    void elem_inject_loop(Elem *parent, void **rows, int n,
                           ElemFactory fn, void *ctx);

**Migration, real and incremental, not a rewrite**: pick ONE existing
injector first (`dbhq_inject_bookmark_items()` is a reasonable pilot -
already reviewed, already understood) and refactor it to call this
helper, verify identical behavior live, THEN migrate the others one
at a time. Do not touch all per-mode injectors in one pass.

============================================================
BUILD ORDER (unchanged from the handoff's part-2 recommendation,
restated here as the authoritative sequence for implementation)
============================================================
1. Gap 3 (flatten helper) - independent, zero risk.
2. Gap 5 (cursor prefix, 2-state only until Gap 2 lands) - independent,
   zero risk.
3. Gap 2 (ACTIVATE scope + BACK) - the real architectural piece,
   db-hq pilot only, full live verification required before "done."
4. Gap 6 (cli_io gate) - rides on Gap 2's scope-root pointer, do
   right after.
5. Gap 7 (header+footer synthetic root) - taskbar-side, revisit
   concrete design after Gap 2 is live (see note above).
6. Gap 8 (elem_inject_loop helper) - cleanup, lowest priority, safe
   to interleave anywhere or do last.

Each step keeps its own claim/release cycle in
`GROK-RENDER-INPUT-REFACTOR-HANDOFF.md` per that doc's existing hard-
boundary protocol - this file is the reference plan, not a substitute
for that doc's real-time coordination.
