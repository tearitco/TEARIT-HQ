/* nb_worker_node_insert_arg.c - element insertion arg helper and mode macros (mechanical extract, single-TU include) */
/* ==================== Element/Node methods (2026-11-02) ====================
 *
 * WHY: a probe of the shipped engine (tests/probe_fingerprint.js, group
 * elementMethods.missing) found prepend, append, before, after, replaceWith,
 * remove, matches, closest, cloneNode, isEqualNode, getClientRects,
 * scrollIntoView, replaceChildren, element.querySelector/querySelectorAll and
 * element.getElementsBy* all absent. glassdoor.com's own page script throws
 * "TypeError: not a function" at `t.prepend(e)` - i.e. exactly here - and
 * these are bread-and-butter methods on any real page, not anything
 * site-specific, which is why they are worth building once properly.
 *
 * WHERE: installed on Element.prototype / Node.prototype in
 * install_dom_classes, so every element gets them - including ones created
 * later by createElement - which is how a browser exposes them. The natives
 * resolve `this` through get_node(), so it does not matter that the wrapper
 * also carries many of its older methods as own properties.
 *
 * WHAT IS DELIBERATELY NOT SPEC: these take Nodes and strings (a string
 * becomes a Text node). A DocumentFragment inserts as ONE node rather than
 * being expanded into its children, because the worker has no fragment node
 * type; inserting a node into itself or into its own descendant is skipped
 * rather than raising HierarchyRequestError. Both are noted where they
 * matter; neither is something an observed page depends on. */

#define NBI_APPEND  0
#define NBI_PREPEND 1
#define NBI_BEFORE  2
#define NBI_AFTER   3

/* Resolve argument v into a node ready to be linked in, or NULL. Also
 * performs the two housekeeping steps every insertion needs: detach from any
 * current parent (insertion MOVES a node, it does not copy it) and drop it
 * from the orphan list, whose next_sibling field would otherwise be
 * clobbered by the link. `into` is the subtree the node is being inserted
 * into, used to refuse self/ancestor insertion. */
static NbNode *nbi_arg(JSContext *ctx, JSValueConst v, NbNode *into) {
    NbNode *n = NULL;
    if (JS_IsString(v)) {
        const char *s = JS_ToCString(ctx, v);
        if (!s) return NULL;
        n = calloc(1, sizeof(*n));
        if (n) { n->text = strdup(s); orphan_add(n); }
        JS_FreeCString(ctx, s);
        if (!n) return NULL;
    } else if (JS_IsObject(v)) {
        n = get_node(ctx, v);
        if (!n) return NULL;
    } else {
        return NULL;
    }
    for (NbNode *p = into; p; p = p->parent) if (p == n) return NULL;   /* would make a cycle */
    node_detach(n);
    orphan_remove(n);
    return n;
}

