/* nb_worker_node_is_connected.c - Node isConnected helper (mechanical extract, single-TU include) */
static JSValue nb_node_isConnected(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)argc; (void)argv;
    NbNode *n = get_this(ctx, this_val);
    if (!n) {
        JSValue gdoc = nb_global_document(ctx);
        int is_live = JS_IsObject(gdoc) && JS_VALUE_GET_PTR(gdoc) == JS_VALUE_GET_PTR(this_val);
        JS_FreeValue(ctx, gdoc);
        return JS_NewBool(ctx, is_live);
    }
    NbNode *root = (NbNode *)nb_top_root(n);
    return JS_NewBool(ctx, root && root->tag && !strcmp(root->tag, "#document"));
}
