/* nb_worker_node_relations_basic.c - parentNode/firstChild/nextSibling getters (mechanical extract, single-TU include) */
static JSValue nb_el_parentNode(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    NbNode *p = n ? n->parent : NULL;
    if (p) return push_node(ctx, p);
    return JS_NULL;
}
static JSValue nb_el_firstChild(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    if (n && n->first_child) return push_node(ctx, n->first_child);
    return JS_NULL;
}
static JSValue nb_el_nextSibling(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    if (n && n->next_sibling) return push_node(ctx, n->next_sibling);
    return JS_NULL;
}
