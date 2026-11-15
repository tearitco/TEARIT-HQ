/* nb_worker_node_relations_struct.c - lastChild/previousSibling/parentElement and helper (mechanical extract, single-TU include) */
/* Structural navigation (2026-11-11): lastChild existed but was never
 * installed on elements; previousSibling, parentElement, and the element-only
 * sibling/child accessors were absent entirely. All of these read the live C
 * tree on every access, so a mutation shows up on the next read with no cache
 * to invalidate. Text nodes participate in the *Sibling properties and are
 * skipped by the *ElementSibling ones, as in a browser; a text node's
 * parentElement is still the element that owns it. */
static int nb_is_element_node(const NbNode *n) {
    return n && n->tag && n->tag[0] && n->tag[0] != '#';
}
static JSValue nb_el_lastChild(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    if (n && n->last_child) return push_node(ctx, n->last_child);
    return JS_NULL;
}
static JSValue nb_el_previousSibling(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    if (!n || !n->parent) return JS_NULL;
    NbNode *prev = NULL;
    for (const NbNode *c = n->parent->first_child; c && c != n; c = c->next_sibling) prev = (NbNode *)c;
    return prev ? push_node(ctx, prev) : JS_NULL;
}
static JSValue nb_el_parentElement(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    NbNode *p = n ? n->parent : NULL;
    return nb_is_element_node(p) ? push_node(ctx, p) : JS_NULL;
}
