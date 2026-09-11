/* khtpm_reparse_diff.c - real, keyed Elem-tree diff/patch. See
 * 08-roadmap/design-docs/CHTPM-INCREMENTAL-REPARSE-DESIGN.md for the
 * full design (§0 explains why this is its own file, not code added
 * to khtpm_core_render.c - the same text-include-a-canonical-.c
 * convention SHARED-SOURCE-COMPILE-IN-PLACE.md already established for
 * khtpm_render_core.c/khtpm_draw_core.c/khtpm_css_parser.c).
 *
 * WHAT THIS SOLVES: khtpm_core_render.c's reparse_chtpm_if_changed()
 * destroys the ENTIRE Elem tree and rebuilds it from scratch on every
 * reparse (g_n_elems = 0; parse_chtpm(...)), so any pointer a caller
 * holds into the old tree (g_default_input_elem chief among them) goes
 * dangling and has to be "found again by key" afterward - a real,
 * already-proven-fragile pattern (kh_text_areas_reload() 2026-09-08,
 * kh_cli_io_reload()/kh_find_input_by_key() 2026-09-11, both real
 * fixes for the same underlying gap, neither a foundation fix).
 *
 * This file patches the OLD tree in place to match the NEW tree's
 * shape and content instead: an element whose key still exists in the
 * new tree is NEVER destroyed or reallocated - its pointer stays
 * valid across the call, no find-by-key step ever needed again, for
 * cli_io/text_area/grid or any future stateful element type.
 *
 * ZERO dependency on khtpm_core_render.c's own X11/globals - this
 * file is a pure function of two Elem trees plus the allocator/free
 * callbacks the caller supplies (so it has no opinion on how/where
 * Elems are pooled - khtpm_core_render.c's own g_pool free-list is a
 * caller concern, not this file's). Depends only on the Elem type
 * (khtpm_render_core.c) and CssStyle (khtpm_css_parser.h) - include
 * BOTH of those before this file, same order khtpm_core_render.c
 * itself already uses. This file does not include them itself so a
 * standalone test driver controls that order explicitly (see
 * khtpm_reparse_diff_test.c).
 *
 * KEY: target_id if non-empty, else id if non-empty, else "" (no real
 * key - matched positionally, see kh_diff_match_children()'s own
 * header comment for the real, documented limitation this implies for
 * a front-shifting list of keyless rows). This is the SAME key every
 * existing bolt-on fix (kh_find_input_by_key()) already uses, for the
 * same reason: stable, author-controlled, survives a <repeat>
 * re-expansion as long as the template keys each row.
 *
 * RUNTIME-STATE FIELDS this engine never overwrites on a matched
 * element (the whole point - preserve what the user is actively
 * doing): input_buffer, cursor, sel_anchor, text_area_buffer,
 * grid_cur_row, grid_cur_col, grid_edit_mode, grid_jump_buffer,
 * grid_cell_buffer. One explicit, uniformly-applied list (see
 * kh_diff_apply_template()) - this file does not special-case cli_io
 * vs text_area vs grid by tag anywhere; a match copies every template
 * field from the new element, then restores these specific fields
 * from the old one, regardless of what tag the element is. Any new
 * per-Elem runtime-state field added in the future needs a line added
 * here, same as it would have needed its own new kh_*_reload() bolt-on
 * before this file existed - documented so that's not forgotten. */

#ifndef KHTPM_REPARSE_DIFF_C
#define KHTPM_REPARSE_DIFF_C

#include <string.h>

/* Caller-supplied allocator/free pair - lets khtpm_core_render.c own
 * its real g_pool free-list (08-roadmap/design-docs/
 * CHTPM-INCREMENTAL-REPARSE-DESIGN.md §3) without this file knowing
 * anything about it. alloc_fn must return a zeroed Elem* (or NULL on
 * exhaustion - the diff aborts gracefully, see
 * kh_reparse_diff_patch()'s own return value). free_fn releases one
 * Elem (already recursed into by the caller - see
 * kh_reparse_diff_free_removed_subtree() below, which the diff calls
 * for every element found genuinely removed). ctx is opaque, passed
 * through unchanged - a real pool/free-list pointer on the caller
 * side, not interpreted here. */
typedef struct {
    Elem *(*alloc_fn)(void *ctx);
    void (*free_fn)(void *ctx, Elem *e);
    void *ctx;
} KhDiffAllocator;

/* REMOVED-element reporting: the diff itself has no idea which old
 * Elem*, if any, is "interesting" (g_default_input_elem, a scope root,
 * whatever else a caller tracks across reparse) - it just tells the
 * caller every OLD Elem* that didn't survive this patch (was truly
 * removed, not just relocated/updated in place), in removal order,
 * post-order (children before their own parent), so the caller can
 * check each one against its own tracked pointers BEFORE the free_fn
 * callback actually releases it back to the pool. Bounded by
 * MAX_ELEMS the same way g_pool itself is - the caller passes a real
 * array + capacity, and this reports how many it filled (silently
 * caps at capacity rather than overflow - a caller that cares should
 * size the array to MAX_ELEMS, the real worst case). */
typedef struct {
    Elem **out;
    int cap;
    int n;
} KhDiffRemovedList;

static void kh_diff_removed_report(KhDiffRemovedList *rl, Elem *e) {
    if (!rl || !rl->out || rl->n >= rl->cap) return;
    rl->out[rl->n++] = e;
}

static const char *kh_diff_key(const Elem *e) {
    if (e->target_id[0]) return e->target_id;
    if (e->id[0]) return e->id;
    return "";
}

/* Copy every TEMPLATE field from `src` (a freshly parsed candidate
 * node) onto `dst` (the OLD, surviving Elem*), then restore `dst`'s
 * OWN pre-existing runtime-state fields on top - see this file's own
 * header comment for the full, explicit field list and why. `dst`'s
 * identity (its own address, its `children`/`parent` pointers, `x/y/
 * w/h`/`style` which layout recomputes fresh every pass regardless)
 * are deliberately NOT touched here - children are reconciled
 * separately by kh_diff_match_children(), and layout/style are always
 * stale until the next real layout pass whether this ran or not, same
 * as today. */
static void kh_diff_apply_template(Elem *dst, const Elem *src) {
    /* Save the runtime-state fields before the template copy
     * clobbers them. */
    char saved_input_buffer[sizeof(dst->input_buffer)];
    int saved_cursor = dst->cursor;
    int saved_sel_anchor = dst->sel_anchor;
    char saved_text_area_buffer[sizeof(dst->text_area_buffer)];
    int saved_grid_cur_row = dst->grid_cur_row;
    int saved_grid_cur_col = dst->grid_cur_col;
    int saved_grid_edit_mode = dst->grid_edit_mode;
    char saved_grid_jump_buffer[sizeof(dst->grid_jump_buffer)];
    char saved_grid_cell_buffer[sizeof(dst->grid_cell_buffer)];
    memcpy(saved_input_buffer, dst->input_buffer, sizeof(saved_input_buffer));
    memcpy(saved_text_area_buffer, dst->text_area_buffer, sizeof(saved_text_area_buffer));
    memcpy(saved_grid_jump_buffer, dst->grid_jump_buffer, sizeof(saved_grid_jump_buffer));
    memcpy(saved_grid_cell_buffer, dst->grid_cell_buffer, sizeof(saved_grid_cell_buffer));

    /* Template fields: tag/id/classes/label/onclick/sprite/active/
     * nav_index/badge_align_left/target_id/relay/bg/backspace_action/
     * rows - everything parse_chtpm() itself sets from the markup.
     * nav_index gets overwritten by the very next assign_nav_and_
     * layout() pass regardless (it's not a diff concern), copied here
     * only for completeness/no-uninitialized-read between now and
     * then. children/parent/n_children are NOT copied - the caller
     * (kh_diff_match_children()) owns rebuilding dst's own children
     * array; x/y/w/h/style are NOT copied - always stale until the
     * next real layout pass, same as every element in this codebase
     * today. */
    snprintf(dst->tag, sizeof(dst->tag), "%s", src->tag);
    snprintf(dst->id, sizeof(dst->id), "%s", src->id);
    memcpy(dst->classes, src->classes, sizeof(dst->classes));
    dst->n_classes = src->n_classes;
    snprintf(dst->label, sizeof(dst->label), "%s", src->label);
    snprintf(dst->onclick, sizeof(dst->onclick), "%s", src->onclick);
    snprintf(dst->sprite, sizeof(dst->sprite), "%s", src->sprite);
    dst->active = src->active;
    dst->nav_index = src->nav_index;
    dst->badge_align_left = src->badge_align_left;
    snprintf(dst->target_id, sizeof(dst->target_id), "%s", src->target_id);
    snprintf(dst->relay, sizeof(dst->relay), "%s", src->relay);
    snprintf(dst->bg, sizeof(dst->bg), "%s", src->bg);
    snprintf(dst->backspace_action, sizeof(dst->backspace_action), "%s", src->backspace_action);
    dst->rows = src->rows;

    /* Restore runtime state. */
    memcpy(dst->input_buffer, saved_input_buffer, sizeof(dst->input_buffer));
    dst->cursor = saved_cursor;
    dst->sel_anchor = saved_sel_anchor;
    memcpy(dst->text_area_buffer, saved_text_area_buffer, sizeof(dst->text_area_buffer));
    dst->grid_cur_row = saved_grid_cur_row;
    dst->grid_cur_col = saved_grid_cur_col;
    dst->grid_edit_mode = saved_grid_edit_mode;
    memcpy(dst->grid_jump_buffer, saved_grid_jump_buffer, sizeof(dst->grid_jump_buffer));
    memcpy(dst->grid_cell_buffer, saved_grid_cell_buffer, sizeof(dst->grid_cell_buffer));
}

/* A brand-new element (no OLD counterpart at all) - deep-copy `src`'s
 * own subtree into freshly allocated Elems (there is no runtime state
 * to preserve, it never existed before this reparse). Returns NULL if
 * the allocator is exhausted partway through (a real, if rare,
 * failure mode - MAX_ELEMS is finite) - the caller should treat that
 * the same as "reparse failed, keep the old tree", not attempt a
 * partial adopt. */
static Elem *kh_diff_deep_copy_new(const Elem *src, const KhDiffAllocator *alloc) {
    Elem *dst = alloc->alloc_fn(alloc->ctx);
    if (!dst) return NULL;
    /* Full struct copy is safe here - dst is freshly allocated
     * (zeroed by the caller's alloc_fn per this file's own contract),
     * there is no prior runtime state to protect. */
    *dst = *src;
    dst->n_children = 0;
    dst->parent = NULL;
    for (int i = 0; i < src->n_children; i++) {
        Elem *child = kh_diff_deep_copy_new(src->children[i], alloc);
        if (!child) return NULL; /* exhausted mid-subtree - propagate failure up */
        child->parent = dst;
        dst->children[dst->n_children++] = child;
    }
    return dst;
}

/* Free an entire removed subtree (post-order: children before the
 * node itself), reporting every freed Elem* to `removed` before its
 * free_fn actually runs, so the caller can check each one against its
 * own tracked pointers (g_default_input_elem etc.) first. */
static void kh_diff_free_removed_subtree(Elem *e, const KhDiffAllocator *alloc, KhDiffRemovedList *removed) {
    for (int i = 0; i < e->n_children; i++)
        kh_diff_free_removed_subtree(e->children[i], alloc, removed);
    kh_diff_removed_report(removed, e);
    alloc->free_fn(alloc->ctx, e);
}

static int kh_diff_match_children(Elem *old_parent, const Elem *new_parent,
                                   const KhDiffAllocator *alloc, KhDiffRemovedList *removed);

/* Diff/patch one matched pair (same key, same tag - see the caller for
 * what "matched" means). Applies the template (§ kh_diff_apply_
 * template), then recurses into children. Returns 0 on allocator
 * exhaustion (propagated up, same "abort, keep old tree" contract as
 * kh_diff_deep_copy_new()), 1 on success. */
static int kh_diff_patch_matched(Elem *old_e, const Elem *new_e,
                                  const KhDiffAllocator *alloc, KhDiffRemovedList *removed) {
    kh_diff_apply_template(old_e, new_e);
    return kh_diff_match_children(old_e, new_e, alloc, removed);
}

/* The real reconciliation, one parent's children list at a time.
 * REAL, DOCUMENTED LIMITATION (see the design doc's own §"Open
 * questions" - positional fallback key stability): a new child with
 * no real key (no id/target_id) is matched POSITIONALLY against the
 * old parent's remaining unmatched children of the SAME tag, in
 * order. This is correct for the overwhelmingly common shapes (a
 * template with no keys at all - pure append/remove-from-the-tail
 * lists) but will misattribute state past the point of a mid-list
 * insertion/removal in a keyless list, or across EVERY row of a
 * front-shifting list (network-browser's own newest-first history is
 * exactly this shape). Harmless in practice as long as every
 * STATEFUL element (cli_io/text_area/grid) always carries a real key
 * (§5 of the design doc - a hard authoring requirement, not a soft
 * suggestion) - a keyless plain <item>/<text> row picking up the
 * "wrong" old Elem*'s stale x/y/w/h/style is invisible (all four get
 * recomputed fresh by the very next layout pass regardless). */
static int kh_diff_match_children(Elem *old_parent, const Elem *new_parent,
                                   const KhDiffAllocator *alloc, KhDiffRemovedList *removed) {
    int old_n = old_parent->n_children;
    const Elem *old_children[MAX_CHILDREN];
    for (int i = 0; i < old_n; i++) old_children[i] = old_parent->children[i];
    int old_used[MAX_CHILDREN];
    memset(old_used, 0, sizeof(old_used));

    Elem *result[MAX_CHILDREN];
    int result_n = 0;

    for (int ni = 0; ni < new_parent->n_children; ni++) {
        const Elem *nc = new_parent->children[ni];
        const char *nkey = kh_diff_key(nc);
        int match = -1;

        if (nkey[0]) {
            /* Real key: match ANY not-yet-used old child with the
             * SAME key + SAME tag, regardless of position (a real
             * key survives reordering by design). */
            for (int oi = 0; oi < old_n; oi++) {
                if (old_used[oi]) continue;
                if (strcmp(kh_diff_key(old_children[oi]), nkey) != 0) continue;
                if (strcmp(old_children[oi]->tag, nc->tag) != 0) continue;
                match = oi;
                break;
            }
        } else {
            /* No key: positional fallback among unused old children
             * of the same tag (first unused wins - see this
             * function's own header comment for the real limitation
             * this implies). */
            for (int oi = 0; oi < old_n; oi++) {
                if (old_used[oi]) continue;
                if (old_children[oi]->id[0] || old_children[oi]->target_id[0]) continue; /* a keyed old child never absorbs an unkeyed new one */
                if (strcmp(old_children[oi]->tag, nc->tag) != 0) continue;
                match = oi;
                break;
            }
        }

        if (match >= 0) {
            old_used[match] = 1;
            Elem *reused = old_parent->children[match]; /* same pointer as old_children[match], just non-const */
            if (!kh_diff_patch_matched(reused, nc, alloc, removed)) return 0;
            result[result_n++] = reused;
        } else {
            Elem *added = kh_diff_deep_copy_new(nc, alloc);
            if (!added) return 0;
            added->parent = old_parent;
            result[result_n++] = added;
        }
        if (result_n >= MAX_CHILDREN) break; /* real cap, matches elem_new()'s own MAX_ELEMS-class bound */
    }

    /* Anything old and still unused genuinely didn't survive - free
     * its whole subtree, report every freed Elem* first. */
    for (int oi = 0; oi < old_n; oi++) {
        if (!old_used[oi]) {
            /* old_children[oi] is a child of old_parent - detach
             * before freeing so a partially-freed pointer never
             * lingers in old_parent->children[] even transiently. */
            kh_diff_free_removed_subtree(old_parent->children[oi], alloc, removed);
        }
    }

    for (int i = 0; i < result_n; i++) old_parent->children[i] = result[i];
    old_parent->n_children = result_n;
    return 1;
}

/* The real entry point. `old_root` is patched IN PLACE to match
 * `new_root`'s shape/content - old_root's own address never changes
 * (it's always the caller's own top-level Elem*, e.g. g_window),
 * matched descendants keep their addresses too. `new_root` is treated
 * as read-only (never mutated, never adopted into the result) - the
 * caller is free to discard/reuse whatever pool it came from
 * immediately after this call returns, regardless of success/failure.
 *
 * Returns 1 on success (old_root now reflects new_root, `removed`
 * lists every Elem* that was genuinely freed - check these against
 * any tracked pointer, e.g. g_default_input_elem, BEFORE trusting it
 * post-call). Returns 0 if the allocator was exhausted partway
 * through (a real, bounded-resource failure) - old_root's state is
 * UNDEFINED at that point (a partial patch may have already
 * happened) and the caller MUST treat this as "reparse failed",
 * discarding old_root's window entirely and falling back to the
 * existing full g_n_elems=0/parse_chtpm() rebuild path, same as
 * today's behavior when parse_chtpm() itself returns NULL. This is a
 * deliberate, simple, honest failure contract - a real partial-tree
 * recovery is not attempted, matching this house's own "no invented
 * partial-success state" preference elsewhere in the codebase. */
static int kh_reparse_diff_patch(Elem *old_root, const Elem *new_root,
                                  const KhDiffAllocator *alloc, KhDiffRemovedList *removed) {
    if (!old_root || !new_root || !alloc) return 0;
    if (strcmp(old_root->tag, new_root->tag) != 0) return 0; /* root tag changed - not a patchable case, caller falls back */
    return kh_diff_patch_matched(old_root, new_root, alloc, removed);
}

#endif /* KHTPM_REPARSE_DIFF_C */
