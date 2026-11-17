/* nb_worker_node_get_root_node.c - Node getRootNode helper (mechanical extract, single-TU include) */
static JSValue nb_node_getRootNode(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)argc; (void)argv;
    NbNode *n = get_this(ctx, this_val);
    if (!n) {
        JSValue gdoc = nb_global_document(ctx);
        int is_live = JS_IsObject(gdoc) && JS_VALUE_GET_PTR(gdoc) == JS_VALUE_GET_PTR(this_val);
        if (is_live) return gdoc; /* live document root is itself */
        JS_FreeValue(ctx, gdoc);
        return JS_UNDEFINED;
    }
    NbNode *root = (NbNode *)nb_top_root(n);
    if (!root) return JS_UNDEFINED;
    if (root->tag && !strcmp(root->tag, "#document")) {
        if (root == g_dom_root) return nb_global_document(ctx);
        return push_node(ctx, root);
    }
    return push_node(ctx, root);
}
