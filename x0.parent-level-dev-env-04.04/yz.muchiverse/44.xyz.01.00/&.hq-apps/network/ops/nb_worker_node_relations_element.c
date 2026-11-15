/* nb_worker_node_relations_element.c - element-only relation getters/count (mechanical extract, single-TU include) */
static JSValue nb_el_firstElementChild(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    if (n) for (const NbNode *c = n->first_child; c; c = c->next_sibling)
        if (nb_is_element_node(c)) return push_node(ctx, (NbNode *)c);
    return JS_NULL;
}
static JSValue nb_el_lastElementChild(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    NbNode *last = NULL;
    if (n) for (const NbNode *c = n->first_child; c; c = c->next_sibling)
        if (nb_is_element_node(c)) last = (NbNode *)c;
    return last ? push_node(ctx, last) : JS_NULL;
}
static JSValue nb_el_nextElementSibling(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    if (n) for (const NbNode *c = n->next_sibling; c; c = c->next_sibling)
        if (nb_is_element_node(c)) return push_node(ctx, (NbNode *)c);
    return JS_NULL;
}
static JSValue nb_el_previousElementSibling(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    if (!n || !n->parent) return JS_NULL;
    NbNode *prev = NULL;
    for (const NbNode *c = n->parent->first_child; c && c != n; c = c->next_sibling)
        if (nb_is_element_node(c)) prev = (NbNode *)c;
    return prev ? push_node(ctx, prev) : JS_NULL;
}
static JSValue nb_el_childElementCount(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *n = get_this(ctx, this_val);
    int count = 0;
    if (n) for (const NbNode *c = n->first_child; c; c = c->next_sibling)
        if (nb_is_element_node(c)) count++;
    return JS_NewInt32(ctx, count);
}
