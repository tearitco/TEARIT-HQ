/* khtpm_render_core.c — Stage 2a of the khtpm merge (au11-hq/
 * khtpm-merge-how2.md), 2026-08-16. Canonical source for the pieces of
 * the Elem/parse_chtpm() rendering model that are genuinely
 * byte-identical or functionally identical across the 3 apps that
 * actually share this architecture:
 *   khtpm_hq_render.c (db-hq), khtpm_events_hq_render.c (events-hq),
 *   chat_hai_hq_render.c (chat-hai).
 *
 * REAL FIX 2026-08-16, direct correction ("we try not to use headers
 * cept from cross os shims"): this was originally written as a .h with
 * `static inline` functions - a NEW in-house header, which this house
 * avoids (checked the real reference, 1.TPMOS_c_+rmmp.0103.0001/
 * projects/wraith-alpha/ops/*.c - zero in-house headers anywhere, only
 * system headers, with `#ifdef _WIN32 ... #else #include <unistd.h>
 * #endif`-shaped blocks being the one legitimate exception). Converted
 * to a plain .c file with NO include guard, meant to be pulled in via a
 * direct `#include "khtpm_render_core.c"` (yes, a .c file, not a
 * separate compiled+linked translation unit) - this is the only way to
 * share a real struct definition + real functions across multiple
 * standalone binaries without introducing a header. Functions stay
 * `static` (correct here, not just habit - each consumer text-includes
 * this file into its OWN translation unit, so `static` keeps these
 * symbols private to each resulting binary, exactly as if they'd been
 * hand-duplicated - which is the whole point).
 *
 * Real, verified overlap only (not assumed) - see local-2do-15.txt's own
 * "Stage 2 real-architecture finding" entry for how each piece here was
 * checked before being moved:
 *   - Elem struct: db-hq/chat-hai were already identical (incl. onclick/
 *     parent); events-hq's was a strict subset (never used those two
 *     fields) - the fuller struct here is a safe superset, not a
 *     behavior change for events-hq.
 *   - MAX_CHILDREN: was 32 in events-hq, 64 in db-hq/chat-hai - unified
 *     on 64 (more headroom, not a behavior change).
 *   - hit_test(): was BYTE-IDENTICAL across all 3 already (pure Elem
 *     geometry, zero X11/global dependency).
 *   - find_by_tag()/find_by_id(): functionally identical (whitespace
 *     differences only).
 *
 * Scope note: draw_elem()/render_tree() are NOT here yet (Stage 2b,
 * deferred) - they depend on each app's own dpy/screen/buf/gc/
 * xftdraw_buf/cmap X11 globals, and matching those exactly across all 3
 * files needs its own real verification pass before sharing, per
 * khtpm-merge-how2.md's own caution on this class of coupling.
 *
 * Real single-binary end goal (per direct instruction, confirmed
 * 2026-08-16 - wraith_parser_alpha.+x IS the standard: one generic
 * binary, argv[1] = .chtpm path, project.pdl resolved from convention,
 * ZERO per-app hardcoded C logic): db-hq/events-hq/chat-hai each still
 * have real, different, hardcoded business logic in C today (chat-hai's
 * session/ledger loading, events-hq's event.ir.pdl page/command
 * parsing, db-hq's common_events listing) - none of that is data-driven
 * yet, so they're still 3 separate binaries for now. This file is the
 * real-.c-sharing step BEFORE that bigger, separately-planned
 * single-binary migration - see local-2do-15.txt.
 *
 * Convention: same directory as khtpm_css_parser.c/.h and
 * stb_image_write.h (see README.md) - source-of-truth only, NOT a shared
 * runtime include path. Each consumer's build_*.sh copies this file into
 * its own ops/ dir before compiling (same `cp` shape as the other files
 * here), then the consumer's own .c does `#include "khtpm_render_core.c"`
 * - NOT added to the $CC command line as a separate translation unit
 * (unlike khtpm_css_parser.c, which IS compiled+linked separately - this
 * file is text-included instead, specifically to avoid needing a header
 * for the struct definition). Add a
 * `cp "$SHARED/khtpm_render_core.c" khtpm_render_core.c` line to any new
 * consumer's build script - don't hand-copy this file again. */

#include <string.h>

#define MAX_CHILDREN 64

typedef struct Elem {
    char tag[32];
    char id[64];
    char classes[CSS_MAX_CLASSES][32];
    int n_classes;
    char label[256];
    /* REAL FIX 2026-08-16 (found live building khtpm_entity_menu_render.c,
     * Stage 2c proof): 64 was too small for a real objects.pdl-style
     * action= shell command (e.g. ava's real "Play" action is 200+
     * chars) - silently truncated mid-string, producing a malformed
     * command (an unterminated quote, confirmed live: "sh: Unterminated
     * quoted string"). db-hq/chat-hai only ever WRITE this field today,
     * never read it (grep-confirmed before bumping this) - safe size
     * increase, not a behavior change for them. */
    char onclick[512];
    int active;   /* tab active / sidebar item selected */
    int nav_index; /* wraith-alpha-standard index nav: 1-based sequential
                     * number assigned to every interactive element each
                     * redraw; 0 = not navigable. Ported from
                     * wraith_parser_alpha.c's own digit_accum/do_jump/
                     * display_num convention. */
    struct Elem *children[MAX_CHILDREN];
    int n_children;
    struct Elem *parent;
    /* computed layout, filled by each app's own layout_pass() */
    int x, y, w, h;
    CssStyle style;
} Elem;

/* Pure Elem-tree geometry - topmost-child-first hit test (children drawn
 * later win the hit, matching draw order), no X11/global dependency.
 * Verified byte-identical across db-hq/events-hq/chat-hai before being
 * moved here. */
static Elem *hit_test(Elem *e, int px, int py) {
    for (int i = e->n_children - 1; i >= 0; i--) {
        Elem *r = hit_test(e->children[i], px, py);
        if (r) return r;
    }
    if (px >= e->x && px < e->x + e->w && py >= e->y && py < e->y + e->h) return e;
    return NULL;
}

/* Depth-first, first-match-wins tag lookup. Verified functionally
 * identical (chat-hai/db-hq had this exact shape; events-hq didn't use
 * it - harmless to gain it as dead code if unused). */
static Elem *find_by_tag(Elem *e, const char *tag) {
    if (!e) return NULL;
    if (strcmp(e->tag, tag) == 0) return e;
    for (int i = 0; i < e->n_children; i++) {
        Elem *r = find_by_tag(e->children[i], tag);
        if (r) return r;
    }
    return NULL;
}

/* Depth-first, first-match-wins id lookup. Verified functionally
 * identical across all 3 (whitespace-only diffs). Disambiguates
 * same-tag elements (e.g. chat-hai's "status" vs "composer-text", both
 * tag "text") - use this over find_by_tag() whenever an id exists. */
static Elem *find_by_id(Elem *e, const char *id) {
    if (!e) return NULL;
    if (strcmp(e->id, id) == 0) return e;
    for (int i = 0; i < e->n_children; i++) {
        Elem *r = find_by_id(e->children[i], id);
        if (r) return r;
    }
    return NULL;
}

/* REAL START 2026-08-16, Stage 3 (khtpm-merge-how2.md §5) - real box-
 * model/flex layout engine, built against the real 9-pattern inventory
 * in that doc's own §5.1b (all 3 real consumers' current layout_pass()
 * functions read in full first, not guessed). Lives HERE (not
 * khtpm_css_parser.c) because it operates on Elem, which only exists in
 * this file - khtpm_css_parser.c knows nothing about trees, only flat
 * per-element style computation (css_compute_style()).
 *
 * REAL, DELIBERATE DESIGN CONSTRAINT: this engine has ZERO access to
 * real text measurement (Xft/font state is each app's own X11 global,
 * not available at this shared-library level - see §5.1b pattern #2's
 * own "measure_text_px()" note). Real contract: for any child whose
 * real size should be its OWN NATURAL/measured content size (pattern
 * #2's tab labels, footer buttons, etc) rather than an explicit CSS
 * width/height, the CALLER must pre-populate that child's own e->w/e->h
 * (via measure_text_px() or equivalent) BEFORE calling this function on
 * its PARENT. This function treats an already-nonzero e->w/e->h as the
 * real natural size for any child with no explicit style.has_width/
 * has_height and no flex_grow - it never re-measures text.
 *
 * Real contract for position:absolute children (§5.1b pattern #5's
 * <title> case): skipped entirely from normal flex flow (matches every
 * real app's own existing `continue` in this exact spot today),
 * positioned at parent_origin + top/left offset instead, using
 * whatever e->w/e->h the caller already set (same pre-measured-size
 * contract as above - position:absolute doesn't change that).
 *
 * Real contract for flex_grow (§5.1b pattern #3): a child with
 * style.has_flex_grow set consumes a real weighted share of whatever
 * main-axis space is left after every fixed/natural-sized sibling is
 * accounted for - matches db-hq/events-hq's own real "sidebar fixed,
 * panel gets the remainder" pattern with flex_grow:1 on the panel.
 *
 * Real contract for block (non-flex, the default): children are left
 * completely untouched by this function - matches every real app's own
 * existing pattern of positioning some elements by hand outside the
 * normal stacking flow (chat-hai's own phantom settings elements,
 * §5.1b pattern #9) - the engine only recurses into real flex
 * containers, it does not silently reposition things a caller
 * deliberately manages itself.
 *
 * NOT YET LIVE-TESTED against a real app as of this write - see
 * khtpm-merge-how2.md §5.3 step 3/4 for the real, required standalone-
 * test-then-port-db-hq-first sequence before this is trusted on a live
 * window. */
static void css_layout_pass(Elem *e, int x, int y, int avail_w, int avail_h) {
    e->x = x;
    e->y = y;
    e->w = (e->style.has_width && !e->style.width_is_pct) ? e->style.width
         : (e->style.has_width && e->style.width_is_pct) ? (avail_w * e->style.width) / 100
         : (e->w > 0 ? e->w : avail_w);
    e->h = (e->style.has_height && !e->style.height_is_pct) ? e->style.height
         : (e->style.has_height && e->style.height_is_pct) ? (avail_h * e->style.height) / 100
         : (e->h > 0 ? e->h : avail_h);

    int is_flex = e->style.has_display && e->style.display_flex;
    if (!is_flex) return; /* block: children untouched, caller manages them by hand */

    int is_row = e->style.has_flex_direction ? e->style.flex_row : 0;
    int n = e->n_children;
    /* REAL 2026-08-16, added after db-hq's own real live tabbar port
     * found the gap (see khtpm_css_parser.h's own header comment on
     * these 2 fields for the full real story) - `padding` insets flow
     * children on BOTH axes (real box-model padding, all 4 sides);
     * `gap` is real, additive main-axis space BETWEEN consecutive flow
     * children only (never before the first or after the last).
     * position:absolute children are deliberately NOT inset by padding
     * (matches this file's own already-tested/passing real behavior -
     * db-hq's own <title> is positioned relative to the parent's raw
     * x/y, not its padding box, and that's the real, already-verified
     * contract, not something this change should alter). */
    int pad = e->style.has_padding ? e->style.padding : 0;
    int gap = e->style.has_gap ? e->style.gap : 0;

    int fixed_total = 0, grow_total = 0, flow_count = 0;
    for (int i = 0; i < n; i++) {
        Elem *c = e->children[i];
        if (c->style.has_position && c->style.position_absolute) continue; /* real pattern #5 - out of flow entirely */
        flow_count++;
        int c_grow = c->style.has_flex_grow ? c->style.flex_grow : 0;
        if (c_grow > 0) { grow_total += c_grow; continue; }
        int c_size = is_row
            ? (c->style.has_width ? c->style.width : c->w)
            : (c->style.has_height ? c->style.height : c->h);
        fixed_total += c_size;
    }
    int gap_total = flow_count > 1 ? gap * (flow_count - 1) : 0;
    int main_avail = (is_row ? e->w : e->h) - 2 * pad;
    int remaining = main_avail - fixed_total - gap_total;
    if (remaining < 0) remaining = 0;

    int pos = (is_row ? e->x : e->y) + pad;
    int cross_origin = (is_row ? e->y : e->x) + pad;
    int cross_size = (is_row ? e->h : e->w) - 2 * pad;
    for (int i = 0; i < n; i++) {
        Elem *c = e->children[i];
        if (c->style.has_position && c->style.position_absolute) {
            int t = c->style.has_top ? c->style.top : 0;
            int l = c->style.has_left ? c->style.left : 0;
            css_layout_pass(c, e->x + l, e->y + t, c->w, c->h);
            continue;
        }
        int c_grow = c->style.has_flex_grow ? c->style.flex_grow : 0;
        int main_size;
        if (c_grow > 0 && grow_total > 0) main_size = (remaining * c_grow) / grow_total;
        else main_size = is_row
            ? (c->style.has_width ? c->style.width : c->w)
            : (c->style.has_height ? c->style.height : c->h);
        if (is_row) css_layout_pass(c, pos, cross_origin, main_size, cross_size);
        else css_layout_pass(c, cross_origin, pos, cross_size, main_size);
        pos += main_size + gap;
    }
}
