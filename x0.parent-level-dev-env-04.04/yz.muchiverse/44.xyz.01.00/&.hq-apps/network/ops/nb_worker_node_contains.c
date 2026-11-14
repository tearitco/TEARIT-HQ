/* nb_worker_node_contains.c - Node/Document.contains helpers (mechanical extract, single-TU include) */
static JSValue nb_el_contains(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *self = get_this(ctx, this_val);
    NbNode *other = (argc > 0 && JS_IsObject(argv[0])) ? get_node(ctx, argv[0]) : NULL;
    if (!self || !other) return JS_NewBool(ctx, 0);
    for (NbNode *n = other; n; n = n->parent)
        if (n == self) return JS_NewBool(ctx, 1);
    return JS_NewBool(ctx, 0);
}
static JSValue nb_doc_contains(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    NbNode *other = (argc > 0 && JS_IsObject(argv[0])) ? get_node(ctx, argv[0]) : NULL;
    if (!other || !g_dom_root) return JS_NewBool(ctx, 0);
    for (NbNode *n = other; n; n = n->parent)
        if (n == g_dom_root) return JS_NewBool(ctx, 1);
    return JS_NewBool(ctx, 0);
}
